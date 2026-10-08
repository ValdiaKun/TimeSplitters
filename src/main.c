#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <vita2d.h>
#include "p5ck.h"
#include "fp_resource.h"
#include "fp_model.h"
#include "fp_geometry.h"
#include "fp_vif.h"
#include "fp_vu.h"
#include "fp_gif.h"
#include "fp_gs_view.h"

#define DATA_PATH "ux0:data/TimeSplitters/PAK/CHR.PAK"
#define BOOT_PATH "ux0:data/TimeSplitters/SLED_530.66"
#define VU_MEMORY_SIZE (16u*1024u)
#define GIF_MEMORY_SIZE (256u*1024u)
#define PREVIEW_CAPACITY 8192u

static vita2d_color_vertex preview[PREVIEW_CAPACITY];
static vita2d_color_vertex transformed_preview[PREVIEW_CAPACITY];
static size_t preview_count=0;
static TsFpGifVertex gif_local[1024];
static TsFpGifVertex scene_vertices[PREVIEW_CAPACITY];
static TsFpGsView preview_view;
static float preview_zoom=1.0f,preview_pan_x=0.0f,preview_pan_y=0.0f;
static int preview_ready=0;
typedef struct { vita2d_color_vertex v[3]; float depth; } TsFpDrawTriangle;
static TsFpDrawTriangle draw_triangles[PREVIEW_CAPACITY/3u];
static int compare_draw_triangles(const void *a,const void *b){
    const TsFpDrawTriangle *x=(const TsFpDrawTriangle*)a,*y=(const TsFpDrawTriangle*)b;
    return x->depth<y->depth?1:(x->depth>y->depth?-1:0);
}

static void update_preview_controls(const SceCtrlData *pad){
    const float pan_step=4.0f,zoom_step=0.025f;
    if(pad->buttons&SCE_CTRL_LEFT) preview_pan_x-=pan_step;
    if(pad->buttons&SCE_CTRL_RIGHT) preview_pan_x+=pan_step;
    if(pad->buttons&SCE_CTRL_UP) preview_pan_y-=pan_step;
    if(pad->buttons&SCE_CTRL_DOWN) preview_pan_y+=pan_step;
    if(pad->buttons&SCE_CTRL_LTRIGGER) preview_zoom*=1.0f-zoom_step;
    if(pad->buttons&SCE_CTRL_RTRIGGER) preview_zoom*=1.0f+zoom_step;
    if(preview_zoom<0.1f) preview_zoom=0.1f;
    if(preview_zoom>8.0f) preview_zoom=8.0f;
}

static int project_gs_vertex(const TsFpGifVertex *v,float *x,float *y){
    return tsfp_gs_view_project(&preview_view,v,preview_zoom,preview_pan_x,preview_pan_y,x,y);
}

static int load_file(const char *path,uint8_t **out,size_t *size_out){
    FILE *fp=fopen(path,"rb"); long end; uint8_t *buf; size_t n;
    if(!fp)return -1;
    if(fseek(fp,0,SEEK_END)!=0){fclose(fp);return -2;}
    end=ftell(fp); if(end<=0 || end>32L*1024L*1024L){fclose(fp);return -3;}
    if(fseek(fp,0,SEEK_SET)!=0){fclose(fp);return -4;}
    buf=(uint8_t*)malloc((size_t)end); if(!buf){fclose(fp);return -5;}
    n=fread(buf,1,(size_t)end,fp); fclose(fp);
    if(n!=(size_t)end){free(buf);return -6;}
    *out=buf; *size_out=n; return 0;
}

static int read_first_model_entry(TsP5ckInfo *info,TsP5ckEntry *selected,uint8_t **out,size_t *size_out,TsFpModelHeader *model){
    FILE *fp=fopen(DATA_PATH,"rb");
    if(!fp)return -1;
    if(ts_p5ck_read_info(fp,info)!=0){fclose(fp);return -2;}
    for(uint32_t i=0;i<info->entry_count;i++){
        TsP5ckEntry entry;
        uint8_t *buf=NULL;
        size_t size=0;
        if(ts_p5ck_read_entry(fp,info,i,&entry)!=0)continue;
        if(ts_p5ck_read_payload(fp,&entry,&buf,&size)!=0)continue;
        if(tsfp_model_probe(buf,size,model)==0){
            *selected=entry;
            *out=buf;
            *size_out=size;
            fclose(fp);
            return 0;
        }
        free(buf);
    }
    fclose(fp);
    return -3;
}

static int read_first_chr_entry(uint8_t **out,size_t *size_out){
    TsP5ckInfo info;
    TsP5ckEntry entry;
    TsFpModelHeader model;
    return read_first_model_entry(&info,&entry,out,size_out,&model);
}

static size_t append_triangles(TsFpGifVertex *dst,size_t cap,const TsFpGifVertex *src,size_t n,uint32_t prim){
    size_t w=0;
    if(prim==3u){
        for(size_t i=0;i+2<n && w+3<=cap;i+=3){dst[w++]=src[i];dst[w++]=src[i+1];dst[w++]=src[i+2];}
    } else if(prim==4u){
        for(size_t i=2;i<n && w+3<=cap;i++){
            if(i&1u){dst[w++]=src[i-1];dst[w++]=src[i-2];dst[w++]=src[i];}
            else {dst[w++]=src[i-2];dst[w++]=src[i-1];dst[w++]=src[i];}
        }
    } else if(prim==5u){
        for(size_t i=2;i<n && w+3<=cap;i++){dst[w++]=src[0];dst[w++]=src[i-1];dst[w++]=src[i];}
    } else if(prim==6u){
        for(size_t i=0;i+1<n && w+6<=cap;i+=2){
            TsFpGifVertex a=src[i],b=src[i+1],c=a,d=b;
            c.y=b.y; d.x=a.x;
            dst[w++]=a;dst[w++]=b;dst[w++]=c;dst[w++]=c;dst[w++]=b;dst[w++]=d;
        }
    }
    return w;
}


typedef struct {
    const uint8_t *micro;
    size_t micro_size;
    TsFpVuState *vu;
    uint8_t *gif_memory;
    TsFpGifVertex *triangles;
    size_t triangle_capacity;
    size_t *triangle_count;
    TsFpGifState *gif_state;
} TsFpVifRenderContext;

static int render_mscal(uint16_t address,uint8_t *vu_memory,size_t vu_size,
                        uint32_t top,uint32_t itop,void *user){
    TsFpVifRenderContext *ctx=(TsFpVifRenderContext*)user;
    TsFpGifSummary gif;
    size_t before;
    if(!ctx||!ctx->vu||!ctx->micro)return -1;
    uint32_t start=(address==0xffffu)?ctx->vu->pc:address;
    if(((size_t)start*8u)>=ctx->micro_size)return -1;
    ctx->vu->memory=vu_memory;
    ctx->vu->memory_size=vu_size;
    ctx->vu->top=top;
    ctx->vu->itop=itop;
    ctx->vu->branch_pending=0;
    before=ctx->vu->gif_used;
    if(tsfp_vu_execute(ctx->micro,ctx->micro_size,start,ctx->vu,8192)!=0)return -2;
    if(ctx->vu->gif_used>before){
        memset(gif_local,0,sizeof(gif_local));
        if(tsfp_gif_parse_state(ctx->gif_memory+before,ctx->vu->gif_used-before,
                                &gif,gif_local,1024,ctx->gif_state)!=0)return -3;
        if(ctx->triangle_count && *ctx->triangle_count<ctx->triangle_capacity){
            size_t room=ctx->triangle_capacity-*ctx->triangle_count;
            size_t local_vertices=gif.vertices<1024u?gif.vertices:1024u;
            size_t wrote=append_triangles(ctx->triangles+*ctx->triangle_count,room,
                                          gif_local,local_vertices,gif.primitive&7u);
            *ctx->triangle_count+=wrote;
        }
    }
    /* The captured GIF packet has been consumed by the host renderer. */
    ctx->vu->gif_used=0;
    return 0;
}

static int build_model_preview(void){
    uint8_t *model_data=NULL,*elf=NULL,*vu_mem=NULL,*gif_mem=NULL;
    size_t model_size=0,elf_size=0,submesh_count=0;
    TsFpModelHeader model; TsFpSubmesh submeshes[256];
    TsFpGifVertex *triangles=NULL;
    TsFpGifState gif_state={0,0,255,255,255,255,0};
    int result=-1;

    if(read_first_chr_entry(&model_data,&model_size)!=0) goto done;
    if(tsfp_model_probe(model_data,model_size,&model)!=0) goto done;
    submesh_count=tsfp_geometry_collect(model_data,model_size,model.mesh_table_offset,
                                         model.mesh_count,submeshes,256);
    if(!submesh_count) goto done;
    if(load_file(BOOT_PATH,&elf,&elf_size)!=0) goto done;
    vu_mem=(uint8_t*)malloc(VU_MEMORY_SIZE); gif_mem=(uint8_t*)malloc(GIF_MEMORY_SIZE);
    if(!vu_mem||!gif_mem) goto done;

    triangles=(TsFpGifVertex*)malloc(sizeof(*triangles)*PREVIEW_CAPACITY);
    size_t tri_count=0;
    if(!triangles) goto done;
    size_t voff,vlen;
    if(tsfp_vu_find_vutext(elf,elf_size,&voff,&vlen)!=0) goto done;
    for(size_t si=0;si<submesh_count && tri_count<PREVIEW_CAPACITY;si++){
        uint32_t off=submeshes[si].data_offset;
        size_t vif_size=(size_t)submeshes[si].vertex_count*16u;
        if(off>model_size || vif_size>model_size-off) continue;

        TsFpVifSummary vif;
        if(tsfp_vif_scan(model_data+off,vif_size,&vif)!=0) continue;
        TsFpVuState vs;
        TsFpVifRenderContext ctx;
        tsfp_vu_state_init(&vs,vu_mem,VU_MEMORY_SIZE,gif_mem,GIF_MEMORY_SIZE);
        ctx.micro=elf+voff; ctx.micro_size=vlen; ctx.vu=&vs;
        ctx.gif_memory=gif_mem;
        ctx.triangles=triangles; ctx.triangle_capacity=PREVIEW_CAPACITY;
        ctx.triangle_count=&tri_count;
        ctx.gif_state=&gif_state;
        TsFpVifMemorySummary vm;
        if(tsfp_vif_unpack_memory_ex(model_data+off,vif_size,vu_mem,VU_MEMORY_SIZE,
                                     &vm,render_mscal,&ctx)!=0) continue;
    }

    if(tri_count>=3){
        if(tri_count>PREVIEW_CAPACITY)tri_count=PREVIEW_CAPACITY;
        memcpy(scene_vertices,triangles,sizeof(*scene_vertices)*tri_count);
        preview_count=tri_count;
        if(tsfp_gs_view_fit(&preview_view,scene_vertices,tri_count,960.0f,544.0f)!=0)goto done;
        preview_zoom=1.0f;preview_pan_x=preview_pan_y=0.0f;
        preview_ready=1;
        for(size_t i=0;i<tri_count;i++){
            preview[i].x=scene_vertices[i].x;
            preview[i].y=scene_vertices[i].y;
            preview[i].z=0.5f;
            preview[i].color=((unsigned)scene_vertices[i].a<<24)|((unsigned)scene_vertices[i].b<<16)|
                             ((unsigned)scene_vertices[i].g<<8)|scene_vertices[i].r;
        }
        result=(int)tri_count;
    }

done:
    free(triangles); free(gif_mem); free(vu_mem); free(elf); free(model_data);
    return result;
}

static int load_probe(TsP5ckInfo *info,TsP5ckEntry *entry,TsFpResourceSummary *resource,
                      TsFpModelHeader *model,TsFpGeometrySummary *geometry,TsFpVifSummary *vif){
    FILE *fp=fopen(DATA_PATH,"rb"); uint8_t *buf=NULL; int r=0;
    if(!fp)return -10;
    memset(resource,0,sizeof(*resource));
    r=ts_p5ck_read_info(fp,info);
    if(r==0 && info->entry_count==0)r=-11;
    if(r==0){
        for(uint32_t i=0;i<info->entry_count;i++){
            TsP5ckEntry candidate;
            uint8_t *candidate_buf=NULL;
            size_t candidate_size=0;
            if(ts_p5ck_read_entry(fp,info,i,&candidate)!=0)continue;
            if(ts_p5ck_read_payload(fp,&candidate,&candidate_buf,&candidate_size)!=0)continue;
            if(tsfp_model_probe(candidate_buf,candidate_size,model)==0){
                *entry=candidate;
                buf=candidate_buf;
                r=tsfp_geometry_probe(candidate_buf,candidate_size,model->mesh_table_offset,
                                       model->mesh_count,geometry);
                if(r==0 && geometry->submesh_count){
                    TsFpSubmesh sm;
                    if(tsfp_geometry_collect(candidate_buf,candidate_size,model->mesh_table_offset,
                                             model->mesh_count,&sm,1)==1){
                        uint32_t bytes=(uint32_t)sm.vertex_count*16u;
                        if(sm.data_offset<=candidate_size && bytes<=candidate_size-sm.data_offset)
                            r=tsfp_vif_scan(candidate_buf+sm.data_offset,bytes,vif);
                    }
                }
                break;
            }
            free(candidate_buf);
        }
        if(!buf && r==0)r=-12;
    }
    free(buf); fclose(fp); return r;
}

static uint32_t probe_boot_vu(void){
    uint8_t *buf=NULL; size_t n,off,len; uint32_t xg=UINT32_MAX;
    if(load_file(BOOT_PATH,&buf,&n)!=0)return xg;
    if(tsfp_vu_find_vutext(buf,n,&off,&len)==0){
        uint32_t pc=UINT32_MAX;
        if(len&&tsfp_vu_probe(buf+off,len,0x683u,&pc)==0)xg=pc;
    }
    free(buf); return xg;
}

static void draw(int result,int preview_result,const TsP5ckInfo *info,const TsP5ckEntry *entry,const TsFpResourceSummary *resource,
                 const TsFpModelHeader *model,const TsFpGeometrySummary *geometry,const TsFpVifSummary *vif,uint32_t xgkick_pc){
    unsigned status=0xFFE03030;
    if(result==0 && preview_result>=3)status=0xFF20C060;
    else if(result==-10)status=0xFFE0A020;
    vita2d_clear_screen();
    vita2d_draw_rectangle(40,40,880,480,0xFF181818);
    vita2d_draw_rectangle(80,90,800,64,status);
    if(info->entry_count){float w=(float)(info->entry_count>1000?800:(info->entry_count*800u)/1000u);vita2d_draw_rectangle(80,190,w,42,0xFF40A0FF);}
    if(entry->length){float w=(float)(entry->length>200000?800:(entry->length*800u)/200000u);vita2d_draw_rectangle(80,270,w,42,0xFF8040FF);}
    if(resource->string_count){float w=(float)(resource->string_count>32?800:(resource->string_count*800u)/32u);vita2d_draw_rectangle(80,350,w,42,0xFFFFA040);}
    if(model->mesh_count){float w=(float)(model->mesh_count>100?800:(model->mesh_count*800u)/100u);vita2d_draw_rectangle(80,390,w,20,0xFFC040A0);}
    if(model->material_count){float w=(float)(model->material_count>100?800:(model->material_count*800u)/100u);vita2d_draw_rectangle(80,420,w,20,0xFF40C080);}
    if(geometry->submesh_count){float w=(float)(geometry->submesh_count>256?800:(geometry->submesh_count*800u)/256u);vita2d_draw_rectangle(80,450,w,18,0xFF60A0E0);}
    if(vif->payload_bytes){float w=(float)(vif->payload_bytes>64?800:(vif->payload_bytes*800u)/64u);vita2d_draw_rectangle(80,470,w,12,0xFF80C060);}
    if(preview_count>=3 && preview_ready){
        size_t n=preview_count-(preview_count%3),tc=0;
        for(size_t i=0;i<n && tc<PREVIEW_CAPACITY/3u;i+=3){
            float x0,y0,x1,y1,x2,y2;
            if(!project_gs_vertex(&scene_vertices[i],&x0,&y0)||
               !project_gs_vertex(&scene_vertices[i+1],&x1,&y1)||
               !project_gs_vertex(&scene_vertices[i+2],&x2,&y2))continue;
            TsFpDrawTriangle *t=&draw_triangles[tc++];
            t->v[0]=preview[i];t->v[1]=preview[i+1];t->v[2]=preview[i+2];
            t->v[0].x=x0;t->v[0].y=y0;
            t->v[1].x=x1;t->v[1].y=y1;
            t->v[2].x=x2;t->v[2].y=y2;
            t->depth=(scene_vertices[i].z+scene_vertices[i+1].z+scene_vertices[i+2].z)*(1.0f/3.0f);
        }
        qsort(draw_triangles,tc,sizeof(draw_triangles[0]),compare_draw_triangles);
        for(size_t i=0;i<tc;i++){
            transformed_preview[i*3u]=draw_triangles[i].v[0];
            transformed_preview[i*3u+1u]=draw_triangles[i].v[1];
            transformed_preview[i*3u+2u]=draw_triangles[i].v[2];
        }
        vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES,transformed_preview,tc*3u);
    }
    if(xgkick_pc!=UINT32_MAX)vita2d_draw_rectangle(80,500,800,10,0xFFC08040);
}

int main(void){
    SceCtrlData pad; TsP5ckInfo info; TsP5ckEntry entry; TsFpResourceSummary resource;
    TsFpModelHeader model; TsFpGeometrySummary geometry; TsFpVifSummary vif;
    memset(&pad,0,sizeof(pad));memset(&info,0,sizeof(info));memset(&entry,0,sizeof(entry));
    memset(&resource,0,sizeof(resource));memset(&model,0,sizeof(model));memset(&geometry,0,sizeof(geometry));memset(&vif,0,sizeof(vif));
    int result=load_probe(&info,&entry,&resource,&model,&geometry,&vif);
    int preview_result=-1;
    uint32_t xgkick_pc=probe_boot_vu();
    if(result==0)preview_result=build_model_preview();
    vita2d_init();
    for(;;){
        sceCtrlPeekBufferPositive(0,&pad,1);if(pad.buttons&SCE_CTRL_START)break;
        update_preview_controls(&pad);
        vita2d_start_drawing();draw(result,preview_result,&info,&entry,&resource,&model,&geometry,&vif,xgkick_pc);
        vita2d_end_drawing();vita2d_swap_buffers();sceDisplayWaitVblankStart();
    }
    vita2d_fini();sceKernelExitProcess(0);return 0;
}
