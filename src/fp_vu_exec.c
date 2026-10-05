#include "fp_vu.h"
#include <math.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void wr32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static float f32(uint32_t v){float f;memcpy(&f,&v,4);return f;}
static uint32_t u32(float f){uint32_t v;memcpy(&v,&f,4);return v;}
static int32_t sx11(uint32_t v){v&=0x7ffu;return (v&0x400u)?(int32_t)(v|0xfffff800u):(int32_t)v;}

static float qf(const TsFpVuState *s){return f32(s->q);}
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
static float fpmax(float a,float b){return a>b?a:b;}
static float fpmin(float a,float b){return a<b?a:b;}

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
        if(acc){if(mask&(1u<<i))s->acc[i]=u32(r[i]);}
    }
    if(!acc)write_mask(s,fd,mask,r);
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
        if(acc){if(mask&(1u<<i))s->acc[i]=u32(r[i]);}
    }
    if(!acc)write_mask(s,fd,mask,r);
}

static void special_upper(TsFpVuState *s,uint32_t up){
    unsigned ft=(up>>16)&31u,fs=(up>>11)&31u,mask=(up>>21)&15u,fd=(up>>6)&31u;
    unsigned sop=(up&3u)|(fd<<2);
    switch(sop){
    case 0:case 1:case 2:case 3: for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])+bc(s,ft,sop));return;
    case 4:case 5:case 6:case 7: for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])-bc(s,ft,sop-4));return;
    case 8:case 9:case 10:case 11: for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->acc[i])+f32(s->vf[fs][i])*bc(s,ft,sop-8));return;
    case 12:case 13:case 14:case 15: for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->acc[i])-f32(s->vf[fs][i])*bc(s,ft,sop-12));return;
    case 16:case 17:case 18:case 19:case 20:case 21:case 22:case 23:{
        unsigned sh=(sop==16||sop==20)?0:(sop==17||sop==21)?4:(sop==18||sop==22)?12:15;
        int toint=sop>=20;
        for(unsigned i=0;i<4;i++)if(mask_has(mask,i)){
            float x=f32(s->vf[fs][i]);
            s->vf[ft][i]=toint?(uint32_t)(int32_t)(x*(float)(1u<<sh)):u32((float)(int32_t)s->vf[fs][i]/(float)(1u<<sh));
        } return;}
    case 24:case 25:case 26:case 27: for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])*bc(s,ft,sop-24));return;
    case 28:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])*qf(s));return;
    case 29:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->vf[ft][i]=s->vf[fs][i]&0x7fffffffu;return;
    case 30:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])*if_(s));return;
    case 31:return;
    case 32:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])+qf(s));return;
    case 33:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->acc[i])+f32(s->vf[fs][i])*qf(s));return;
    case 34:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])+if_(s));return;
    case 35:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->acc[i])+f32(s->vf[fs][i])*if_(s));return;
    case 36:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])-qf(s));return;
    case 37:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->acc[i])-f32(s->vf[fs][i])*qf(s));return;
    case 38:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->vf[fs][i])-if_(s));return;
    case 39:for(unsigned i=0;i<4;i++)if(mask_has(mask,i))s->acc[i]=u32(f32(s->acc[i])-f32(s->vf[fs][i])*if_(s));return;
    case 40:mac3(s,0,fs,ft,mask,0,1);return;
    case 41:mac3(s,0,fs,ft,mask,3,1);return;
    case 42:mac3(s,0,fs,ft,mask,2,1);return;
    case 44:mac3(s,0,fs,ft,mask,1,1);return;
    case 45:mac3(s,0,fs,ft,mask,4,1);return;
    case 46:s->acc[0]=u32(f32(s->vf[fs][1])*f32(s->vf[ft][2]));s->acc[1]=u32(f32(s->vf[fs][2])*f32(s->vf[ft][0]));s->acc[2]=u32(f32(s->vf[fs][0])*f32(s->vf[ft][1]));return;
    case 47:return;
    default:s->unsupported++;return;
    }
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
        float r[4]={f32(s->acc[0])-f32(s->vf[fs][1])*f32(s->vf[ft][2]),
                    f32(s->acc[1])-f32(s->vf[fs][2])*f32(s->vf[ft][0]),
                    f32(s->acc[2])-f32(s->vf[fs][0])*f32(s->vf[ft][1]),0.0f};
        write_mask(s,fd,mask,r); break;
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
        else bytes=0;
        if(p+16u+bytes>size)return 0;
        p+=16u+bytes;
        if(eop)return p-start;
    }
    return 0;
}

static void lower_exec(TsFpVuState *s,uint32_t lo,uint32_t next_pc){
    unsigned op=(lo>>25)&0x7fu,it=(lo>>16)&31u,is=(lo>>11)&31u,id=(lo>>6)&31u;
    unsigned dest=(lo>>21)&15u;
    int32_t imm=sx11(lo);

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
        case 0x34: /* LQI VF[ft], (VI[is]++) */
            a=((size_t)s->vi[is]&0x3ffu)*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            for(unsigned i=0;i<4;i++)if(mask_has(dest,i))s->vf[it][i]=rd32(s->memory+a+i*4u);
            s->vi[is]=(s->vi[is]+1u)&0x3ffu;
            return;
        case 0x35: /* SQI VF[fs], (VI[it]++) */
            a=((size_t)s->vi[it]&0x3ffu)*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            for(unsigned i=0;i<4;i++)if(mask_has(dest,i))wr32(s->memory+a+i*4u,s->vf[is][i]);
            s->vi[it]=(s->vi[it]+1u)&0x3ffu;
            return;
        case 0x36: /* LQD VF[ft], (--VI[is]) */
            s->vi[is]=(s->vi[is]-1u)&0x3ffu;
            a=(size_t)s->vi[is]*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            for(unsigned i=0;i<4;i++)if(mask_has(dest,i))s->vf[it][i]=rd32(s->memory+a+i*4u);
            return;
        case 0x37: /* SQD VF[fs], (--VI[it]) */
            s->vi[it]=(s->vi[it]-1u)&0x3ffu;
            a=(size_t)s->vi[it]*16u;
            if(a+16>s->memory_size){s->unsupported++;return;}
            for(unsigned i=0;i<4;i++)if(mask_has(dest,i))wr32(s->memory+a+i*4u,s->vf[is][i]);
            return;
        case 0x38: { /* DIV Q, VF[fs]fsf, VF[ft]ftf */
            unsigned ftf=(lo>>23)&3u,fsf=(lo>>21)&3u;
            s->q=u32(f32(s->vf[is][fsf])/f32(s->vf[it][ftf]));
            return;
        }
        case 0x39: { /* SQRT Q, VF[ft]ftf */
            unsigned ftf=(lo>>23)&3u;
            s->q=u32(sqrtf(fabsf(f32(s->vf[it][ftf]))));
            return;
        }
        case 0x3a: { /* RSQRT Q, VF[fs]fsf / sqrt(abs(VF[ft]ftf)) */
            unsigned ftf=(lo>>23)&3u,fsf=(lo>>21)&3u;
            s->q=u32(f32(s->vf[is][fsf])/sqrtf(fabsf(f32(s->vf[it][ftf]))));
            return;
        }
        case 0x3b: /* WAITQ: timing is not cycle-accurate here. */
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
        case 0x3e: /* ILWR VI[it], (VI[is])field */
            a=((size_t)s->vi[is]&0x3ffu)*16u+(dest?((dest==1u)?0u:(dest==2u)?4u:(dest==4u)?8u:12u):0u);
            if(a+4>s->memory_size){s->unsupported++;return;}
            if(it)s->vi[it]=rd32(s->memory+a)&0xffffu;
            return;
        case 0x3f: /* ISWR VI[it], (VI[is])field */
            a=((size_t)s->vi[is]&0x3ffu)*16u+(dest?((dest==1u)?0u:(dest==2u)?4u:(dest==4u)?8u:12u):0u);
            if(a+4>s->memory_size){s->unsupported++;return;}
            wr32(s->memory+a,it?s->vi[it]:0u);
            return;
        case 0x6c: { /* XGKICK VI[is] */
            a=((size_t)s->vi[is]&0x3ffu)*16u;
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
        default:
            s->unsupported++;
            return;
        }
    }

    if(op==0x00u||op==0x01u){
        size_t a=mem_addr(s,is,imm);
        if(a+16>s->memory_size){s->unsupported++;return;}
        if(op==0)memcpy(s->vf[it],s->memory+a,16);else memcpy(s->memory+a,s->vf[it],16);
        return;
    }
    if(op==0x04u||op==0x05u){
        size_t a=mem_addr(s,is,imm)+(id&3u)*4u;
        if(a+4>s->memory_size){s->unsupported++;return;}
        if(op==4)s->vi[it]=rd32(s->memory+a)&0xffffu;else wr32(s->memory+a,s->vi[it]&0xffffu);
        return;
    }
    if(op==0x08u||op==0x09u){if(it)s->vi[it]=(uint32_t)((int32_t)s->vi[is]+imm*(op==8?1:-1));return;}
    if(op==0x30u){if(id)s->vi[id]=s->vi[it]+s->vi[is];return;}
    if(op==0x31u){if(id)s->vi[id]=s->vi[it]-s->vi[is];return;}
    if(op==0x32u){if(id)s->vi[id]=s->vi[it]+sx11(lo);return;}
    if(op==0x34u){if(id)s->vi[id]=s->vi[it]&s->vi[is];return;}
    if(op==0x35u){if(id)s->vi[id]=s->vi[it]|s->vi[is];return;}
    if(op==0x20u){s->branch_pending=1;s->branch_target=(uint32_t)((int32_t)next_pc+imm);return;}
    if(op==0x21u){if(it)s->vi[it]=next_pc+1u;s->branch_pending=1;s->branch_target=(uint32_t)((int32_t)next_pc+imm);return;}
    if(op==0x22u){s->branch_pending=1;s->branch_target=s->vi[is];return;}
    if(op==0x23u){if(it)s->vi[it]=next_pc+1u;s->branch_pending=1;s->branch_target=s->vi[is];return;}
    if(op>=0x24u&&op<=0x2bu){
        int32_t a=(int32_t)s->vi[it],b=(int32_t)s->vi[is];int take=0;
        switch(op){case 0x24:take=a==b;break;case 0x25:take=a!=b;break;case 0x28:take=a<0;break;case 0x29:take=a>0;break;case 0x2a:take=a<=0;break;case 0x2b:take=a>=0;break;default:break;}
        if(take){s->branch_pending=1;s->branch_target=(uint32_t)((int32_t)next_pc+imm);}
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
        if(op==0x7cu) s->q=u32(f32(s->vf[is][fsf])/f32(s->vf[it][ftf]));
        else if(op==0x7du) s->q=u32(sqrtf(fabsf(f32(s->vf[it][ftf]))));
        else if(op==0x7eu) s->q=u32(f32(s->vf[is][fsf])/sqrtf(fabsf(f32(s->vf[it][ftf]))));
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
int tsfp_vu_execute(const uint8_t *micro,size_t size,uint32_t start,TsFpVuState *state,uint32_t max_steps){
    if(!micro||!state||(size&7u)||start>=size/8u)return -1;
    state->pc=start;
    for(state->steps=0;state->steps<max_steps&&state->pc<size/8u;state->steps++){
        uint32_t pc=state->pc,lo=rd32(micro+pc*8u),up=rd32(micro+pc*8u+4u);
        uint32_t delayed=state->branch_pending, delayed_target=state->branch_target;
        state->branch_pending=0;
        state->pc=pc+1u;
        if(up&0x80000000u) state->vi[21]=lo; else lower_exec(state,lo,state->pc);
        upper_exec(state,up);
        if(delayed && !state->branch_pending) state->pc=delayed_target;
        if(up&0x40000000u)return 0;
    }
    return state->steps>=max_steps?-2:0;
}
