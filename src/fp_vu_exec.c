#include "fp_vu.h"
#include <math.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void wr32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static float f32(uint32_t v){float f;memcpy(&f,&v,4);return f;}
static uint32_t u32(float f){uint32_t v;memcpy(&v,&f,4);return v;}
static int32_t sx11(uint32_t v){v&=0x7ffu;return (v&0x400u)?(int32_t)(v|0xfffff800u):(int32_t)v;}

static float qf(const TsFpVuState *s){return f32(s->q);}
static void q_schedule(TsFpVuState *s,uint32_t value,int invalid,int divzero,uint32_t cycles){
    uint32_t st=s->status_flag & 0xfcfu;
    st|=(invalid?1u:0u)<<4;
    st|=(divzero?1u:0u)<<5;
    if(invalid)st|=1u<<10;
    if(divzero)st|=1u<<11;
    s->q_pending_value=value;
    s->q_pending_status=st;
    s->q_pending_cycles=cycles;
    s->q_pending=1;
}
static void q_commit(TsFpVuState *s){
    if(!s->q_pending)return;
    s->q=s->q_pending_value;
    s->status_flag=(s->status_flag&0xfcfu)|(s->q_pending_status&0xc30u);
    s->q_pending=0;
    s->q_pending_cycles=0;
}
static void p_schedule(TsFpVuState *s,uint32_t value,uint32_t cycles){
    s->p_pending_value=value;
    s->p_pending_cycles=cycles;
    s->p_pending=1;
}
static void p_commit(TsFpVuState *s){
    if(!s->p_pending)return;
    s->p=s->p_pending_value;
    s->p_pending=0;
    s->p_pending_cycles=0;
}
static void p_wait(TsFpVuState *s){
    while(s->p_pending && s->p_pending_cycles){
        s->p_pending_cycles--;
        if(s->p_pending_cycles==0)p_commit(s);
    }
}
static void p_wait_producer(TsFpVuState *s){
    /*
     * EFU throughput is one cycle shorter than result latency.  A producer
     * may start one cycle before P writeback because no instruction can
     * consume that just-finishing result in the same cycle.
     */
    while(s->p_pending && s->p_pending_cycles>2u)
        s->p_pending_cycles--;
    if(s->p_pending && s->p_pending_cycles<=2u)p_commit(s);
}
static void p_tick(TsFpVuState *s){
    if(!s->p_pending || !s->p_pending_cycles)return;
    s->p_pending_cycles--;
    if(s->p_pending_cycles==0)p_commit(s);
}
static int is_waitq(uint32_t lo){
    return ((lo>>25)&0x7fu)==0x40u &&
           (((lo&3u)|((lo>>4)&0x7cu))==0x3bu);
}
static int is_fdiv(uint32_t lo){
    unsigned op=(lo>>25)&0x7fu;
    /*
     * FDIV instructions have two encodings in this interpreter: the
     * special1 form and the direct lower-opcode forms used by the decoder
     * below.  All four operations share the single Q-producing pipeline.
     */
    if(op==0x7cu||op==0x7du||op==0x7eu||op==0x7fu)return 1;
    if(op!=0x40u)return 0;
    {
        unsigned special=(lo&3u)|((lo>>4)&0x7cu);
        return special>=0x38u && special<=0x3au;
    }
}
static int is_waitp(uint32_t lo){
    return ((lo>>25)&0x7fu)==0x40u &&
           (((lo&3u)|((lo>>4)&0x7cu))==0x7bu);
}
static int is_efu_upper(uint32_t up){
    unsigned op=up&63u, sop=(up&3u)|(((up>>6)&31u)<<2);
    return op>=0x3cu && sop>=112u && sop<=126u;
}
static void q_wait(TsFpVuState *s){
    while(s->q_pending && s->q_pending_cycles){
        s->q_pending_cycles--;
        if(s->q_pending_cycles==0) q_commit(s);
    }
}

static float if_(const TsFpVuState *s){return f32(s->vi[21]);}
static float bc(const TsFpVuState *s,unsigned ft,unsigned b){return f32(s->vf[ft][b&3u]);}
static int mask_has(unsigned mask,unsigned component){
    /* VU field encoding is W=bit0, Z=bit1, Y=bit2, X=bit3. */
    return (mask & (1u << (3u-component))) != 0u;
}
static void write_mask(TsFpVuState *s,unsigned fd,unsigned mask,const float r[4]){
    if(!fd)return;
    for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->vf[fd][i]=u32(r[i]);
}
static uint32_t float_to_int_scaled(float x,unsigned shift){
    float v=shift?x*(float)(1u<<shift):x;
    uint32_t raw=u32(v);
    if((raw&0x7f800000u)>=0x4f000000u)
        return (raw&0x80000000u)?0x80000000u:0x7fffffffu;
    return (uint32_t)(int32_t)v;
}
static float fpmax(float a,float b){
    uint32_t x=u32(a),y=u32(b),r;
    if((int32_t)x<0 && (int32_t)y<0) r=(int32_t)x<(int32_t)y?x:y;
    else r=(int32_t)x>(int32_t)y?x:y;
    return f32(r);
}
static float fpmin(float a,float b){
    uint32_t x=u32(a),y=u32(b),r;
    if((int32_t)x<0 && (int32_t)y<0) r=(int32_t)x>(int32_t)y?x:y;
    else r=(int32_t)x<(int32_t)y?x:y;
    return f32(r);
}

/*
 * VU EFU arctangent uses the fixed SCEI polynomial, not the host libm
 * implementation.  The hardware microcode reduces the input to [0,1]
 * before applying this approximation; keeping the polynomial here also
 * preserves the documented single-precision constants.
 */
static float efu_atan_unit(float x){
    static const float t[8]={
        0.999999344348907f,-0.333298563957214f,0.199465364217758f,
        -0.139085337519646f,0.096420042216778f,-0.055909886956215f,
        0.021861229091883f,-0.004054057877511f
    };
    float ax=fabsf(x), r;
    if(ax==0.0f)return x;
    if(ax>1.0f){
        float inv=1.0f/ax;
        float y=(inv-1.0f)/(inv+1.0f);
        float y2=y*y, p=y, a=t[0]*p;
        for(unsigned i=1;i<8;i++){p*=y2;a+=t[i]*p;}
        r=1.5707963705062866f-(a+0.785398185253143f);
    }else{
        float y=(ax-1.0f)/(ax+1.0f);
        float y2=y*y, p=y, a=t[0]*p;
        for(unsigned i=1;i<8;i++){p*=y2;a+=t[i]*p;}
        r=a+0.785398185253143f;
    }
    return x<0.0f?-r:r;
}

static float fmac_condition(float x){
    uint32_t raw=u32(x), exp=raw&0x7f800000u, frac=raw&0x007fffffu;
    if(exp==0u){
        if(frac!=0u) return f32(raw&0x80000000u); /* denormal -> signed zero */
        return x;
    }
    if(exp==0x7f800000u) return f32((raw&0x80000000u)|0x7f7fffffu); /* overflow/NaN -> signed max */
    return x;
}
static uint16_t mac_component_flags(float x){
    uint32_t raw=u32(x);
    uint16_t f=0;
    uint32_t exp=raw&0x7f800000u, frac=raw&0x007fffffu;
    if(exp==0u && frac!=0u) f|=1u<<8;       /* U */
    if(raw&0x80000000u) f|=1u<<4;           /* S */
    if((raw&0x7fffffffu)==0u || (exp==0u&&frac!=0u)) f|=1u; /* Z */
    if(exp==0x7f800000u) f|=1u<<12;         /* O */
    return f;
}
static void update_status_from_mac(TsFpVuState *s){
    uint32_t m=s->mac_flag&0xffffu;
    uint32_t cur=0;
    for(unsigned i=0;i<4;i++){
        if(m&(1u<<i)) cur|=1u;          /* Z */
        if(m&(1u<<(4u+i))) cur|=2u;     /* S */
        if(m&(1u<<(8u+i))) cur|=4u;     /* U */
        if(m&(1u<<(12u+i))) cur|=8u;    /* O */
    }
    s->status_flag=(s->status_flag&0xfc0u)|cur|
                   ((s->status_flag|cur)&0xfu)<<6;
}
static void update_mac_flags(TsFpVuState *s,const float r[4],unsigned mask){
    uint32_t f=s->mac_flag&0xffffu;
    for(unsigned i=0;i<4;i++){
        unsigned zbit=3u-i,sbit=7u-i,ubit=11u-i,obit=15u-i;
        f &= ~((1u<<zbit)|(1u<<sbit)|(1u<<ubit)|(1u<<obit));
        if(mask_has(mask,i)){
            uint16_t cf=mac_component_flags(r[i]);
            if(cf&1u) f|=1u<<zbit;
            if(cf&0x10u) f|=1u<<sbit;
            if(cf&0x100u) f|=1u<<ubit;
            if(cf&0x1000u) f|=1u<<obit;
        }
    }
    s->mac_flag=f;
    update_status_from_mac(s);
}

static void mac2(TsFpVuState *s,unsigned fd,unsigned fs,unsigned mask,
                  float b,int op,int acc){
    float r[4];
    for(unsigned i=0;i<4;i++){
        float x=f32(s->vf[fs][i]);
        if(op==0)r[i]=x+b;
        else if(op==1)r[i]=x-b;
        else if(op==2)r[i]=x*b;
        else if(op==3)r[i]=f32(s->acc[i])+x*b;
        else r[i]=f32(s->acc[i])-x*b;
        if(acc){if(mask_has(mask,i))s->acc[i]=u32(fmac_condition(r[i]));}
    }
    update_mac_flags(s,r,mask);
    if(!acc){
        for(unsigned i=0;i<4;i++)r[i]=fmac_condition(r[i]);
        write_mask(s,fd,mask,r);
    }
}
static void mac3(TsFpVuState *s,unsigned fd,unsigned fs,unsigned ft,unsigned mask,int op,int acc){
    float r[4];
    for(unsigned i=0;i<4;i++){
        float x=f32(s->vf[fs][i]),y=f32(s->vf[ft][i]);
        if(op==0)r[i]=x+y;
        else if(op==1)r[i]=x-y;
        else if(op==2)r[i]=x*y;
        else if(op==3)r[i]=f32(s->acc[i])+x*y;
        else r[i]=f32(s->acc[i])-x*y;
        if(acc){if(mask_has(mask,i))s->acc[i]=u32(fmac_condition(r[i]));}
    }
    update_mac_flags(s,r,mask);
    if(!acc){
        for(unsigned i=0;i<4;i++)r[i]=fmac_condition(r[i]);
        write_mask(s,fd,mask,r);
    }
}

static void special_upper(TsFpVuState *s,uint32_t up){
    unsigned ft=(up>>16)&31u,fs=(up>>11)&31u,mask=(up>>21)&15u,fd=(up>>6)&31u;
    unsigned sop=(up&3u)|(fd<<2);
    switch(sop){
    case 0:case 1:case 2:case 3: mac2(s,0,fs,mask,bc(s,ft,sop),0,1);return;
    case 4:case 5:case 6:case 7: mac2(s,0,fs,mask,bc(s,ft,sop-4),1,1);return;
    case 8:case 9:case 10:case 11: mac2(s,0,fs,mask,bc(s,ft,sop-8),3,1);return;
    case 12:case 13:case 14:case 15: mac2(s,0,fs,mask,bc(s,ft,sop-12),4,1);return;
    case 16:case 17:case 18:case 19:case 20:case 21:case 22:case 23:{
        unsigned sh=(sop==16||sop==20)?0:(sop==17||sop==21)?4:(sop==18||sop==22)?12:15;
        int toint=sop>=20;
        if(ft==0)return;
        for(unsigned i=0;i<4;i++)if(mask_has(mask,i)){
            float x=f32(s->vf[fs][i]);
            s->vf[ft][i]=toint?float_to_int_scaled(x,sh):u32((float)(int32_t)s->vf[fs][i]/(float)(1u<<sh));
        } return;}
    case 24:case 25:case 26:case 27: mac2(s,0,fs,mask,bc(s,ft,sop-24),2,1);return;
    case 28:mac2(s,0,fs,mask,qf(s),2,1);return;
    case 29:if(ft)for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->vf[ft][i]=s->vf[fs][i]&0x7fffffffu;return;
    case 30:mac2(s,0,fs,mask,if_(s),2,1);return;
    case 112: case 113: case 114: case 115:
    case 116: case 117: case 118:
    case 120: case 121: case 122:
    case 124: case 125: case 126: {
        float x=f32(s->vf[fs][0]), y=f32(s->vf[fs][1]), z=f32(s->vf[fs][2]), w=f32(s->vf[fs][3]);
        float v=0.0f; uint32_t cycles=12u;
        switch(sop){
        case 112: v=x*x+y*y+z*z; cycles=11u; break; /* ESADD */
        case 113: { float q=x*x+y*y+z*z; v=q!=0.0f?1.0f/q:q; cycles=18u; break; } /* ERSADD */
        case 114: v=sqrtf(x*x+y*y+z*z); cycles=18u; break; /* ELENG */
        case 115: { float q=x*x+y*y+z*z; v=q>0.0f?1.0f/sqrtf(q):q; cycles=24u; break; } /* ERLENG */
        case 116: v=x!=0.0f?efu_atan_unit(y/x):0.0f; cycles=54u; break; /* EATANxy */
        case 117: v=x!=0.0f?efu_atan_unit(z/x):0.0f; cycles=54u; break; /* EATANxz */
        case 118: v=x+y+z+w; cycles=12u; break; /* ESUM */
        case 120: { unsigned sf=(up>>21)&3u; float a=f32(s->vf[fs][sf]); v=a>=0.0f?sqrtf(a):a; cycles=12u; break; } /* ESQRT */
        case 121: { unsigned sf=(up>>21)&3u; float a=f32(s->vf[fs][sf]); v=a>=0.0f?(1.0f/sqrtf(a)):a; cycles=18u; break; } /* ERSQRT */
        case 122: { unsigned sf=(up>>21)&3u; float a=f32(s->vf[fs][sf]); v=a!=0.0f?1.0f/a:a; cycles=12u; break; } /* ERCPR */
        case 124: { unsigned sf=(up>>21)&3u; float a=f32(s->vf[fs][sf]); v=a-(0.166666567325592f*a*a*a)+(0.008333025500178f*a*a*a*a*a)-(0.000198074136279f*a*a*a*a*a*a*a)+(0.000002601886990f*a*a*a*a*a*a*a*a*a); cycles=29u; break; } /* ESIN */
        case 125: { unsigned sf=(up>>21)&3u; v=efu_atan_unit(f32(s->vf[fs][sf])); cycles=54u; break; } /* EATAN */
        case 126: { unsigned sf=(up>>21)&3u; float a=f32(s->vf[fs][sf]); float q=1.0f+0.249998688697815f*a+0.031257584691048f*a*a+0.002591371303424f*a*a*a+0.000171562001924f*a*a*a*a+0.000005430199963f*a*a*a*a*a+0.000000690600018f*a*a*a*a*a*a; q=q*q*q*q; v=q!=0.0f?1.0f/q:q; cycles=44u; break; } /* EEXP */
        default: break;
        }
        p_schedule(s,u32(v),cycles);
        return;
    }
    case 31: { /* CLIPw.xyz: append six clipping-result bits. */
        /*
         * VU CLIP compares the floating-point bit representation after
         * removing the sign from W; denormals use the largest denormal as
         * the threshold.  This also gives the architectural NaN behavior.
         */
        uint32_t wr=u32(f32(s->vf[ft][3]));
        int32_t w=(wr&0x7f800000u)?(int32_t)(wr&0x7fffffffu):0x007fffff;
        uint32_t flags=0;
        for(unsigned i=0;i<3;i++){
            uint32_t v=s->vf[fs][i];
            if((int32_t)v>w) flags|=1u<<(i*2u);
            if((int32_t)(v^0x80000000u)>w) flags|=1u<<(i*2u+1u);
        }
        s->clip_flag=((s->clip_flag<<6)&0x00ffffffu)|flags;
        return;
    }
    case 32:mac2(s,0,fs,mask,qf(s),0,1);return;
    case 33:mac2(s,0,fs,mask,qf(s),3,1);return;
    case 34:mac2(s,0,fs,mask,if_(s),0,1);return;
    case 35:mac2(s,0,fs,mask,if_(s),3,1);return;
    case 36:mac2(s,0,fs,mask,qf(s),1,1);return;
    case 37:mac2(s,0,fs,mask,qf(s),4,1);return;
    case 38:mac2(s,0,fs,mask,if_(s),1,1);return;
    case 39:mac2(s,0,fs,mask,if_(s),4,1);return;
    case 40:mac3(s,0,fs,ft,mask,0,1);return;
    case 41:mac3(s,0,fs,ft,mask,3,1);return;
    case 42:mac3(s,0,fs,ft,mask,2,1);return;
    case 44:mac3(s,0,fs,ft,mask,1,1);return;
    case 45:mac3(s,0,fs,ft,mask,4,1);return;
    case 46: {
        /* OPMULA: ACC.xyz = Fs x Ft (partial outer-product form). */
        float r[4]={
            f32(s->vf[fs][1])*f32(s->vf[ft][2]),
            f32(s->vf[fs][2])*f32(s->vf[ft][0]),
            f32(s->vf[fs][0])*f32(s->vf[ft][1]),
            f32(s->acc[3])
        };
        unsigned xyz_mask=mask&0xeu;
        for(unsigned i=0;i<3;i++)if(mask_has(xyz_mask,i))s->acc[i]=u32(fmac_condition(r[i]));
        update_mac_flags(s,r,xyz_mask);
        return;
    }
    case 47:return;
    default:s->unsupported++;return;
    }
}
static unsigned upper_vf_dest(uint32_t up){
    unsigned op=up&63u,fd=(up>>6)&31u,ft=(up>>16)&31u;
    if(op<0x30u)return fd;
    if(op<0x3cu)return 0u;
    {
        unsigned sop=(up&3u)|(fd<<2);
        if((sop>=16u&&sop<=23u)||sop==29u)return ft;
    }
    return 0u;
}

static void upper_exec(TsFpVuState *s,uint32_t up){
    unsigned ft=(up>>16)&31u,fs=(up>>11)&31u,fd=(up>>6)&31u,mask=(up>>21)&15u,op=up&63u;
    if(op>=0x3cu){special_upper(s,up);return;}
    if(op<=0x03u){mac2(s,fd,fs,mask,bc(s,ft,op),0,0);return;}
    if(op<=0x07u){mac2(s,fd,fs,mask,bc(s,ft,op-4),1,0);return;}
    if(op<=0x0bu){mac2(s,fd,fs,mask,bc(s,ft,op-8),3,0);return;}
    if(op<=0x0fu){mac2(s,fd,fs,mask,bc(s,ft,op-12),4,0);return;}
    if(op<=0x13u){float r[4];for(unsigned i=0;i<4;i++)r[i]=fpmax(f32(s->vf[fs][i]),bc(s,ft,op-16));write_mask(s,fd,mask,r);return;}
    if(op<=0x17u){float r[4];for(unsigned i=0;i<4;i++)r[i]=fpmin(f32(s->vf[fs][i]),bc(s,ft,op-20));write_mask(s,fd,mask,r);return;}
    if(op<=0x1bu){mac2(s,fd,fs,mask,bc(s,ft,op-24),2,0);return;}
    if(op==0x1cu){mac2(s,fd,fs,mask,qf(s),2,0);return;}
    if(op==0x1du){float r[4];for(unsigned i=0;i<4;i++)r[i]=fpmax(f32(s->vf[fs][i]),if_(s));write_mask(s,fd,mask,r);return;}
    if(op==0x1eu){mac2(s,fd,fs,mask,if_(s),2,0);return;}
    if(op==0x1fu){float r[4];for(unsigned i=0;i<4;i++)r[i]=fpmin(f32(s->vf[fs][i]),if_(s));write_mask(s,fd,mask,r);return;}
    if(op==0x20u){mac2(s,fd,fs,mask,qf(s),0,0);return;}
    if(op==0x21u){mac2(s,fd,fs,mask,qf(s),3,0);return;}
    if(op==0x22u){mac2(s,fd,fs,mask,if_(s),0,0);return;}
    if(op==0x23u){mac2(s,fd,fs,mask,if_(s),3,0);return;}
    if(op==0x24u){mac2(s,fd,fs,mask,qf(s),1,0);return;}
    if(op==0x25u){mac2(s,fd,fs,mask,qf(s),4,0);return;}
    if(op==0x26u){mac2(s,fd,fs,mask,if_(s),1,0);return;}
    if(op==0x27u){mac2(s,fd,fs,mask,if_(s),4,0);return;}
    switch(op){
    case 0x28:mac3(s,fd,fs,ft,mask,0,0);break;
    case 0x29:mac3(s,fd,fs,ft,mask,3,0);break;
    case 0x2a:mac3(s,fd,fs,ft,mask,2,0);break;
    case 0x2b:{float r[4];for(unsigned i=0;i<4;i++)r[i]=fpmax(f32(s->vf[fs][i]),f32(s->vf[ft][i]));write_mask(s,fd,mask,r);break;}
    case 0x2c:mac3(s,fd,fs,ft,mask,1,0);break;
    case 0x2d:mac3(s,fd,fs,ft,mask,4,0);break;
    case 0x2e:{
        /* OPMSUB: VFd.xyz = ACC.xyz - (Fs x Ft). */
        float r[4]={f32(s->acc[0])-f32(s->vf[fs][1])*f32(s->vf[ft][2]),
                    f32(s->acc[1])-f32(s->vf[fs][2])*f32(s->vf[ft][0]),
                    f32(s->acc[2])-f32(s->vf[fs][0])*f32(s->vf[ft][1]),0.0f};
        unsigned xyz_mask=mask&0xeu;
        for(unsigned i=0;i<3;i++)if(mask_has(xyz_mask,i))r[i]=fmac_condition(r[i]);
        write_mask(s,fd,xyz_mask,r);
        update_mac_flags(s,r,xyz_mask);
        break;
    }
    case 0x2f:{float r[4];for(unsigned i=0;i<4;i++)r[i]=fpmin(f32(s->vf[fs][i]),f32(s->vf[ft][i]));write_mask(s,fd,mask,r);break;}
    default:s->unsupported++;break;
    }
}

static size_t mem_addr(const TsFpVuState *s,unsigned is,int32_t imm){
    int32_t a=(int32_t)s->vi[is]+imm;
    a&=0x3ff;
    return (size_t)a*16u;
}
static size_t gif_packet_size(const uint8_t *mem,size_t size,size_t start){
    size_t p=start;
    while(p+16<=size){
        uint64_t lo=rd32(mem+p)|((uint64_t)rd32(mem+p+4)<<32);
        uint32_t nloop=(uint32_t)(lo&0x7fffu);
        uint8_t eop=(uint8_t)((lo>>15)&1u),flg=(uint8_t)((lo>>58)&3u),nreg=(uint8_t)((lo>>60)&15u);
        if(!nreg)nreg=16;
        size_t bytes;
        if(flg==0)bytes=(size_t)nloop*nreg*16u;
        else if(flg==1)bytes=((size_t)nloop*nreg+1u)/2u*16u;
        else if(flg==2)bytes=(size_t)nloop*16u;
        else if(flg==3)bytes=(size_t)nloop*16u; /* IMAGE mode */
        else bytes=0;
        if(p+16u+bytes>size)return 0;
        p+=16u+bytes;
        if(eop)return p-start;
    }
    return 0;
}

static void lower_exec(TsFpVuState *s,uint32_t lo,uint32_t next_pc,const uint32_t *vi_branch){
    unsigned op=(lo>>25)&0x7fu,it=(lo>>16)&31u,is=(lo>>11)&31u,id=(lo>>6)&31u;
    unsigned dest=(lo>>21)&15u;
    int32_t imm=sx11(lo);

    /*
     * XGKICK is a lower-pipeline opcode (0x6c), not one of the
     * special1 low-six-bit opcodes below.  Handle it before the
     * special1 gate so GIF transfers cannot be silently counted as
     * unsupported instructions.
     */
    if(op==0x6cu){
        size_t a=((size_t)s->vi[is]&0x3ffu)*16u;
        s->xgkick_pc=s->pc;
        if(a<s->memory_size&&s->gif&&s->gif_used<s->gif_size){
            size_t n=gif_packet_size(s->memory,s->memory_size,a);
            if(!n)n=s->memory_size-a;
            if(n>s->gif_size-s->gif_used)n=s->gif_size-s->gif_used;
            memcpy(s->gif+s->gif_used,s->memory+a,n);
            s->gif_used+=n;
        }
        return;
    }

    /* Lower "special1" instructions are selected by bit 31 plus the
       low six-bit opcode.  The 4-bit destination mask is at bits 21..24. */
    if((lo&0x80000000u) && (lo&0x3fu)>=0x3cu){
        unsigned sop=(lo&3u)|((lo>>4)&0x7cu);
        size_t a;
        switch(sop){
        case 0x30: /* MOVE */
            if(it) write_mask(s,it,dest,(float[4]){
                f32(s->vf[is][0]),f32(s->vf[is][1]),f32(s->vf[is][2]),f32(s->vf[is][3])});
            return;
        case 0x31: /* MR32 VF[it], VF[is] */
            if(it) for(unsigned i=0;i<4;i++) if(mask_has(dest,i))
                s->vf[it][i]=s->vf[is][(i+3u)&3u];
            return;
        case 0x34: /* LQI VF[ft], (VI[is]++) */
            a=((size_t)s->vi[is]&0x3ffu)*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            if(it)for(unsigned i=0;i<4;i++)if(mask_has(dest,i))s->vf[it][i]=rd32(s->memory+a+i*4u);
            s->vi[is]=(s->vi[is]+1u)&0x3ffu;
            return;
        case 0x35: /* SQI VF[fs], (VI[it]++) */
            a=((size_t)s->vi[it]&0x3ffu)*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            for(unsigned i=0;i<4;i++)if(mask_has(dest,i))wr32(s->memory+a+i*4u,s->vf[is][i]);
            s->vi[it]=(s->vi[it]+1u)&0x3ffu;
            return;
        case 0x36: { /* LQD VF[ft], (--VI[is]) */
            uint32_t qaddr=(s->vi[is]-1u)&0x3ffu;
            if(is)s->vi[is]=qaddr;
            a=(size_t)qaddr*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            if(it)for(unsigned i=0;i<4;i++)if(mask_has(dest,i))s->vf[it][i]=rd32(s->memory+a+i*4u);
            return;
        }
        case 0x37: { /* SQD VF[fs], (--VI[it]) */
            uint32_t qaddr=(s->vi[it]-1u)&0x3ffu;
            if(it)s->vi[it]=qaddr;
            a=(size_t)qaddr*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            for(unsigned i=0;i<4;i++)if(mask_has(dest,i))wr32(s->memory+a+i*4u,s->vf[is][i]);
            return;
        }
        case 0x38: { /* DIV Q, VF[fs]fsf, VF[ft]ftf */
            unsigned ftf=(lo>>23)&3u,fsf=(lo>>21)&3u;
            float num=f32(s->vf[is][fsf]),den=f32(s->vf[it][ftf]);
            int invalid=(num==0.0f&&den==0.0f), divzero=(den==0.0f&&!invalid);
            float qv;
            if(den==0.0f){
                uint32_t sign=((u32(num)^u32(den))&0x80000000u);
                qv=f32(sign|0x7f7fffffu);
            } else qv=num/den;
            q_schedule(s,u32(qv),invalid,divzero,7);
            return;
        }
        case 0x39: { /* SQRT Q, VF[ft]ftf */
            unsigned ftf=(lo>>23)&3u;
            float x=f32(s->vf[it][ftf]);
            q_schedule(s,u32(sqrtf(fabsf(x))),x<0.0f,0,7);
            return;
        }
        case 0x3a: { /* RSQRT Q, VF[fs]fsf / sqrt(abs(VF[ft]ftf)) */
            unsigned ftf=(lo>>23)&3u,fsf=(lo>>21)&3u;
            float num=f32(s->vf[is][fsf]),den=f32(s->vf[it][ftf]);
            int invalid=(den<0.0f)||(den==0.0f&&num==0.0f);
            int divzero=(den==0.0f);
            float qv;
            if(den==0.0f){
                uint32_t sign=((u32(num)^u32(den))&0x80000000u);
                qv=(num==0.0f)?f32(sign):f32(sign|0x7f7fffffu);
            } else qv=num/sqrtf(fabsf(den));
            q_schedule(s,u32(qv),invalid,divzero,13);
            return;
        }
        case 0x3b: /* WAITQ: interlock until the pending Q result is visible. */
            q_commit(s);
            return;
        case 0x3c: { /* MTIR VI[it], VF[fs]field */
            unsigned fsf=(lo>>21)&3u;
            if(it)s->vi[it]=s->vf[is][fsf]&0xffffu;
            return;
        }
        case 0x3d: { /* MFIR VF[ft]field, VI[is] */
            if(it)for(unsigned i=0;i<4;i++)if(mask_has(dest,i))
                s->vf[it][i]=(uint32_t)(int32_t)(int16_t)(s->vi[is]&0xffffu);
            return;
        }
        case 0x3e: { /* ILWR VI[it], (VI[is])field */
            unsigned field;
            if(dest&2u) field=2u;
            else if(dest&4u) field=1u;
            else if(dest&8u) field=0u;
            else field=3u;
            a=((size_t)s->vi[is]&0x3ffu)*16u+(size_t)field*4u;
            if(a+4>s->memory_size){s->unsupported++;return;}
            if(it)s->vi[it]=rd32(s->memory+a)&0xffffu;
            return;
        }
        case 0x3f: { /* ISWR VI[it], (VI[is])field */
            a=((size_t)s->vi[is]&0x3ffu)*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            uint32_t v=it?(s->vi[it]&0xffffu):0u;
            for(unsigned i=0;i<4;i++)if(mask_has(dest,i))wr32(s->memory+a+i*4u,v);
            return;
        }
        case 0x40: /* RNEXT.dest VF[ft], R */
            if(it){
                uint32_t x=(s->r>>4)&1u, y=(s->r>>22)&1u;
                s->r=0x3f800000u|(((s->r<<1)^(x^y))&0x007fffffu);
                for(unsigned i=0;i<4;i++)if(mask_has(dest,i))s->vf[it][i]=s->r;
            }
            return;
        case 0x41: /* RGET.dest VF[ft], R */
            if(it)for(unsigned i=0;i<4;i++)if(mask_has(dest,i))s->vf[it][i]=s->r;
            return;
        case 0x42: { /* RINIT R, VF[fs]field */
            unsigned fsf=(lo>>21)&3u;
            s->r=0x3f800000u|(s->vf[is][fsf]&0x007fffffu);
            return;
        }
        case 0x43: { /* RXOR R, VF[fs]field */
            unsigned fsf=(lo>>21)&3u;
            s->r=0x3f800000u|((s->r^s->vf[is][fsf])&0x007fffffu);
            return;
        }
        case 100: /* MFP.dest VF[ft], P */
            if(it)for(unsigned i=0;i<4;i++)if(mask_has(dest,i))s->vf[it][i]=s->p;
            return;
        case 123: /* WAITP */
            p_wait(s);
            return;
        default:
            s->unsupported++;
            return;
        }
    }

    if(op==0x00u||op==0x01u){
        size_t a=mem_addr(s,is,imm);
        if(a+16>s->memory_size){s->unsupported++;return;}
        unsigned dm=dest?dest:0xfu;
        if(op==0){
            uint32_t v[4]; for(unsigned i=0;i<4;i++)v[i]=rd32(s->memory+a+i*4u);
            if(it)write_mask(s,it,dm,(float[4]){f32(v[0]),f32(v[1]),f32(v[2]),f32(v[3])});
        }else{
            for(unsigned i=0;i<4;i++)if(mask_has(dm,i))wr32(s->memory+a+i*4u,s->vf[it][i]);
        }
        return;
    }
    if(op==0x04u||op==0x05u){
        unsigned field;
        if(dest&2u) field=2u;       /* Z */
        else if(dest&4u) field=1u;  /* Y */
        else if(dest&8u) field=0u;  /* X */
        else field=3u;              /* W, including an empty field */
        size_t a=mem_addr(s,is,imm)+(size_t)field*4u;
        if(a+4>s->memory_size){s->unsupported++;return;}
        if(op==4)s->vi[it]=rd32(s->memory+a)&0xffffu;else wr32(s->memory+a,s->vi[it]&0xffffu);
        return;
    }
    /* VI00-VI15 are 16-bit hardware registers. Reserved pseudo-registers
       used by this interpreter (notably vi21 for the I-bit path) retain their
       established full-width representation. */
    #define VI_WRITE16(reg,val) do { if((reg)!=0u) s->vi[(reg)]=(uint32_t)(val); if((reg)<16u) s->vi[(reg)]&=0xffffu; } while(0)
    #define VI_WRITE_ARITH(reg,val,wide) do { if((reg)!=0u) s->vi[(reg)]=(uint32_t)(val); if((reg)<16u && !(wide)) s->vi[(reg)]&=0xffffu; } while(0)
    if(op==0x08u||op==0x09u){
        uint32_t uimm=((lo>>21)&0xfu)<<11 | (lo&0x7ffu);
        if(it)VI_WRITE_ARITH(it,s->vi[is]+(op==0x08u?uimm:0u-uimm),is==21u);
        return;
    }
    if(op==0x30u){if(id)VI_WRITE_ARITH(id,s->vi[it]+s->vi[is],is==21u||it==21u);return;}
    if(op==0x31u){if(id)VI_WRITE_ARITH(id,s->vi[it]-s->vi[is],is==21u||it==21u);return;}
    if(op==0x32u){
        int32_t imm5=(int32_t)((lo>>6)&0x1fu);
        if(imm5&0x10)imm5-=0x20;
        if(it)VI_WRITE_ARITH(it,s->vi[is]+imm5,is==21u);
        return;
    }
    if(op==0x34u){if(id)VI_WRITE_ARITH(id,s->vi[it]&s->vi[is],is==21u||it==21u);return;}
    if(op==0x35u){if(id)VI_WRITE_ARITH(id,s->vi[it]|s->vi[is],is==21u||it==21u);return;}
    /*
     * Register-form integer ALU instructions are normal lower opcodes
     * (0x40+), not special1 opcodes. VI00-VI15 are 16-bit registers.
     */
    if(op==0x40u){if(id)s->vi[id]=(s->vi[is]+s->vi[it])&0xffffu;return;} /* IADD */
    if(op==0x41u){if(id)s->vi[id]=(s->vi[is]-s->vi[it])&0xffffu;return;} /* ISUB */
    if(op==0x42u){if(it){uint32_t imm5=(lo>>6)&0x1fu;int32_t v=(int32_t)(int16_t)(s->vi[is]&0xffffu)+(int32_t)(int8_t)(imm5|((imm5&0x10u)?0xe0u:0u));s->vi[it]=(uint32_t)v&0xffffu;}return;} /* IADDI */
    if(op==0x44u){if(id)s->vi[id]=(s->vi[is]&s->vi[it])&0xffffu;return;} /* IAND */
    if(op==0x45u){if(id)s->vi[id]=(s->vi[is]|s->vi[it])&0xffffu;return;} /* IOR */
    #undef VI_WRITE_ARITH
    #undef VI_WRITE16
    if(op>=0x10u&&op<=0x1cu){
        uint32_t imm12=(((lo>>21)&1u)<<11)|(lo&0x7ffu);
        switch(op){
        case 0x10u: s->vi[1]=((s->clip_flag&0x00ffffffu)==(lo&0x00ffffffu))?1u:0u; return; /* FCEQ */
        case 0x11u: s->clip_flag=lo&0x00ffffffu; return; /* FCSET */
        case 0x12u: s->vi[1]=((s->clip_flag&0x00ffffffu)&(lo&0x00ffffffu))?1u:0u; return; /* FCAND */
        case 0x13u: s->vi[1]=(((s->clip_flag&0x00ffffffu)|(lo&0x00ffffffu))==0x00ffffffu)?1u:0u; return; /* FCOR */
        case 0x14u: if(it)s->vi[it]=((s->status_flag&0xfffu)==imm12)?1u:0u; return; /* FSEQ */
        case 0x15u: s->status_flag=(imm12&0xfc0u)|(s->status_flag&0x3fu); return; /* FSSET */
        case 0x16u: if(it)s->vi[it]=(s->status_flag&0xfffu)&imm12; return; /* FSAND */
        case 0x17u: if(it)s->vi[it]=(s->status_flag&0xfffu)|imm12; return; /* FSOR */
        case 0x18u: if(it)s->vi[it]=((s->mac_flag&0xffffu)==(s->vi[is]&0xffffu))?1u:0u; return; /* FMEQ */
        case 0x1au: if(it)s->vi[it]=(s->mac_flag&0xffffu)&(s->vi[is]&0xffffu); return; /* FMAND */
        case 0x1bu: if(it)s->vi[it]=(s->mac_flag&0xffffu)|(s->vi[is]&0xffffu); return; /* FMOR */
        case 0x1cu: if(it)s->vi[it]=s->clip_flag&0xfffu; return; /* FCGET */
        default: return;
        }
    }
    /* VU1 lower branch opcodes follow the hardware table:
       B/BAL, JR/JALR, then IBEQ/IBNE/IBLTZ/IBGTZ/IBLEZ/IBGEZ. */
    if(op==0x20u){
        s->branch_pending=1;
        s->branch_target=(uint32_t)((int32_t)next_pc+imm);
        return;
    }
    if(op==0x21u){
        s->vi[15]=next_pc+1u;
        s->branch_pending=1;
        s->branch_target=(uint32_t)((int32_t)next_pc+imm);
        return;
    }
    if(op==0x24u){
        s->branch_pending=1;
        s->branch_target=s->vi[is];
        return;
    }
    if(op==0x25u){
        if(it)s->vi[it]=next_pc+1u;
        s->branch_pending=1;
        s->branch_target=s->vi[is];
        return;
    }
    if(op==0x28u||op==0x29u){
        int32_t a=(int16_t)((vi_branch?vi_branch[is]:s->vi[is])&0xffffu),b=(int16_t)((vi_branch?vi_branch[it]:s->vi[it])&0xffffu);
        int take=(op==0x28u)?(a==b):(a!=b);
        if(take){
            s->branch_pending=1;
            s->branch_target=(uint32_t)((int32_t)next_pc+imm);
        }
        return;
    }
    if(op>=0x2cu&&op<=0x2fu){
        int32_t a=(int16_t)((vi_branch?vi_branch[is]:s->vi[is])&0xffffu);
        int take=0;
        switch(op){
        case 0x2c:take=a<0;break;
        case 0x2d:take=a>0;break;
        case 0x2e:take=a<=0;break;
        case 0x2f:take=a>=0;break;
        default:break;
        }
        if(take){
            s->branch_pending=1;
            s->branch_target=(uint32_t)((int32_t)next_pc+imm);
        }
        return;
    }
    /* FCEQ/FCAND/FCOR use a fixed VI01 destination; bits 23..0 are Imm24. */
    if(op==0x10u){s->vi[1]=((s->clip_flag&0xffffffu)==(lo&0xffffffu))?1u:0u;return;}
    if(op==0x11u){s->clip_flag=lo&0xffffffu;return;}
    if(op==0x12u){s->vi[1]=((s->clip_flag&(lo&0xffffffu))!=0u)?1u:0u;return;}
    if(op==0x13u){s->vi[1]=(((s->clip_flag|(lo&0xffffffu))&0xffffffu)==0xffffffu)?1u:0u;return;}
    if(op==0x14u){if(it)s->vi[it]=((s->status_flag&0xfffu)==(lo&0xfffu))?1u:0u;return;}
    if(op==0x15u){s->status_flag=(s->status_flag&0x3fu)|(lo&0xfc0u);return;}
    if(op==0x16u){if(it)s->vi[it]=((s->status_flag&(lo&0xfffu))!=0u)?1u:0u;return;}
    if(op==0x17u){if(it)s->vi[it]=(((s->status_flag|(lo&0xfffu))&0xfffu)==0xfffu)?1u:0u;return;}
    if(op==0x18u){if(it)s->vi[it]=((s->mac_flag&0xffffu)==(s->vi[is]&0xffffu))?1u:0u;return;}
    if(op==0x1au){if(it)s->vi[it]=(s->mac_flag&s->vi[is])&0xffffu;return;}
    if(op==0x1bu){if(it)s->vi[it]=(s->mac_flag|s->vi[is])&0xffffu;return;}
    if(op==0x1cu){if(it)s->vi[it]=s->clip_flag&0xfffu;return;}
    if(op==0x7cu||op==0x7du||op==0x7eu||op==0x7fu){
        unsigned ftf=(lo>>23)&3u,fsf=(lo>>21)&3u;
        float num,den,qv; int invalid=0,divzero=0;
        if(op==0x7cu){
            num=f32(s->vf[is][fsf]); den=f32(s->vf[it][ftf]);
            invalid=(num==0.0f&&den==0.0f); divzero=(den==0.0f&&!invalid);
            if(den==0.0f) qv=f32(((u32(num)^u32(den))&0x80000000u)|0x7f7fffffu);
            else qv=num/den;
            q_schedule(s,u32(qv),invalid,divzero,7);
        } else if(op==0x7du){
            den=f32(s->vf[it][ftf]);
            q_schedule(s,u32(sqrtf(fabsf(den))),den<0.0f,0,7);
        } else {
            num=f32(s->vf[is][fsf]); den=f32(s->vf[it][ftf]);
            invalid=(den<0.0f)||(den==0.0f&&num==0.0f);
            divzero=(den==0.0f);
            if(den==0.0f){
                uint32_t sign=((u32(num)^u32(den))&0x80000000u);
                qv=(num==0.0f)?f32(sign):f32(sign|0x7f7fffffu);
            } else qv=num/sqrtf(fabsf(den));
            q_schedule(s,u32(qv),invalid,divzero,13);
        }
        return;
    }
    if(op==0x68u){if(it)s->vi[it]=s->top;return;}
    if(op==0x69u){if(it)s->vi[it]=s->itop;return;}
    s->unsupported++;
}

void tsfp_vu_state_init(TsFpVuState *state,uint8_t *memory,size_t memory_size,uint8_t *gif,size_t gif_size){
    if(!state)return;
    memset(state,0,sizeof(*state));
    state->memory=memory; state->memory_size=memory_size;
    state->vf[0][3]=u32(1.0f);
    state->gif=gif; state->gif_size=gif_size; state->xgkick_pc=UINT32_MAX;
}
static void flush_flag_pipeline(TsFpVuState *s){
    unsigned base=s->flag_pipe_pos;
    for(unsigned n=0;n<4;n++){
        unsigned slot=(base+n)&3u;
        {
            uint8_t valid=s->flag_pipe_valid[slot];
            if(valid&1u) s->mac_flag=s->mac_pipe[slot];
            if(valid&2u) s->status_flag=s->status_pipe[slot];
            if(valid&4u) s->clip_flag=s->clip_pipe[slot];
            s->flag_pipe_valid[slot]=0;
        }
    }
}
int tsfp_vu_execute(const uint8_t *micro,size_t size,uint32_t start,TsFpVuState *state,uint32_t max_steps){
    if(!micro||!state||(size&7u)||start>=size/8u)return -1;
    state->pc=start;
    for(state->steps=0;state->steps<max_steps&&state->pc<size/8u;state->steps++){
        uint32_t pc=state->pc,lo=rd32(micro+pc*8u),up=rd32(micro+pc*8u+4u);
        uint32_t delayed=state->branch_pending, delayed_target=state->branch_target;
        if(is_efu_upper(up) && state->p_pending) p_wait_producer(state);
        else p_tick(state);
        /*
         * FDIV is a single shared resource. A second DIV/SQRT/RSQRT cannot
         * start while the previous result is in flight; hardware stalls the
         * instruction stream instead of replacing the pending result.
         */
        if(is_fdiv(lo) && !is_waitq(lo) && state->q_pending){
            q_wait(state);
        } else if(state->q_pending && state->q_pending_cycles){
            state->q_pending_cycles--;
            if(state->q_pending_cycles==0) q_commit(state);
        }
        {
            unsigned slot=state->flag_pipe_pos;
            uint8_t valid=state->flag_pipe_valid[slot];
            if(valid&1u) state->mac_flag=state->mac_pipe[slot];
            if(valid&2u) state->status_flag=state->status_pipe[slot];
            if(valid&4u) state->clip_flag=state->clip_pipe[slot];
            state->flag_pipe_valid[slot]=0;
        }
        state->branch_pending=0;
        state->pc=pc+1u;
        /*
         * The two halves of a VU instruction execute in parallel.  The upper
         * half must see the state from the beginning of the cycle, and the
         * lower half must not see a VF value produced by the upper half in
         * that same cycle.  Preserve the upper result while executing lower,
         * then commit it back.  This also makes an upper/lower write collision
         * deterministic in favor of the upper pipeline, matching hardware.
         *
         * The I bit is likewise latched after the upper instruction executes;
         * loading VI21 before upper_exec would incorrectly make an ADDi/etc.
         * in the same LIW observe the newly loaded immediate.
         */
        /*
         * WAITQ interlocks the pair, so its upper instruction observes the
         * completed Q result. This is the exposed VU synchronization rule.
         */
        if(is_waitq(lo) && state->q_pending) q_wait(state);
        if(is_waitp(lo) && state->p_pending) p_wait(state);
        uint32_t vi_before[32];
        uint32_t vf_before[32][4];
        uint32_t vf_upper[32][4];
        uint16_t mac_before=(uint16_t)state->mac_flag;
        uint16_t mac_upper;
        uint32_t status_before=state->status_flag, status_upper;
        uint32_t clip_before=state->clip_flag, clip_upper;
        memcpy(vi_before,state->vi,sizeof(vi_before));
        memcpy(vf_before,state->vf,sizeof(vf_before));
        upper_exec(state,up);
        mac_upper=(uint16_t)state->mac_flag;
        status_upper=state->status_flag;
        clip_upper=state->clip_flag;
        memcpy(vf_upper,state->vf,sizeof(vf_upper));
        state->mac_flag=mac_before;
        state->status_flag=status_before;
        state->clip_flag=clip_before;
        if(up&0x80000000u) {
            memcpy(state->vf,vf_before,sizeof(vf_before));
            state->vi[21]=lo;
            memcpy(state->vf,vf_upper,sizeof(vf_upper));
        } else {
            memcpy(state->vf,vf_before,sizeof(vf_before));
            lower_exec(state,lo,state->pc,vi_before);
            /* Register-level write priority: if the upper pipeline writes
               a VF register, the lower result for that whole register is
               discarded, even when the destination fields differ. */
            {
                unsigned upper_dest=upper_vf_dest(up);
                if(upper_dest) memcpy(state->vf[upper_dest],vf_upper[upper_dest],16);
            }
        }
        if(is_waitq(lo)) q_commit(state);
        /* VF0 is hardwired to (0,0,0,1) on the VU; direct opcode paths
           must not be able to leave a modified value behind. */
        state->vf[0][0]=0u; state->vf[0][1]=0u; state->vf[0][2]=0u; state->vf[0][3]=u32(1.0f);
        {
            unsigned slot=state->flag_pipe_pos;
            unsigned lower_op=(lo>>25)&0x7fu;
            uint8_t valid=0;
            if(mac_upper!=(uint16_t)mac_before){
                state->mac_pipe[slot]=(uint16_t)mac_upper;
                valid|=1u;
            }
            if(status_upper!=status_before && lower_op!=0x15u){
                state->status_pipe[slot]=status_upper;
                valid|=2u;
            }
            if(clip_upper!=clip_before && lower_op!=0x11u){
                state->clip_pipe[slot]=clip_upper;
                valid|=4u;
            }
            state->flag_pipe_valid[slot]=valid;
            state->flag_pipe_pos=(slot+1u)&3u;
        }
        if(delayed && !state->branch_pending) state->pc=delayed_target;
        /* E terminates after one delay-slot instruction. The E-bit
           instruction itself completes before the slot executes. */
        if(state->end_pending){
            state->end_pending=0;
            flush_flag_pipeline(state);
            q_commit(state);
            return 0;
        }
        if(up&0x40000000u) state->end_pending=1;
    }
    return state->steps>=max_steps?-2:0;
}
