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
#define DATA_PATH "ux0:data/TimeSplitters/PAK/CHR.PAK"
#define BOOT_PATH "ux0:data/TimeSplitters/SLED_530.66"
static vita2d_color_vertex preview[1024];
static size_t preview_count=0;
static void run_vu_path(const uint8_t *vif_data,size_t vif_size,TsFpGifSummary *gif);
static int load_probe(TsP5ckInfo *info, TsP5ckEntry *entry, TsFpResourceSummary *resource, TsFpModelHeader *model, TsFpGeometrySummary *geometry, TsFpVifSummary *vif, TsFpGifSummary *gif) {
    FILE *fp=fopen(DATA_PATH,"rb"); uint8_t *buf=NULL; size_t n; int r;
    if(!fp) return -10;
    r=ts_p5ck_read_info(fp,info);
    if(r==0 && info->entry_count==0) r=-11;
    if(r==0) r=ts_p5ck_read_entry(fp,info,0,entry);
    if(r==0) {
        uint32_t size=entry->compressed_length ? entry->compressed_length : entry->length;
        if(size==0 || size>64u*1024u*1024u) r=-12;
        else if(fseek(fp,(long)entry->offset,SEEK_SET)!=0) r=-13;
        else if(!(buf=(uint8_t*)malloc(size))) r=-14;
        else { n=fread(buf,1,size,fp); if(n<size) r=-15; else { r=tsfp_resource_probe(buf,n,resource); if(r!=0) { r=tsfp_model_probe(buf,n,model); if(r==0) { r=tsfp_geometry_probe(buf,n,model->mesh_table_offset,model->mesh_count,geometry); if(r==0 && model->mesh_count) { uint32_t p=0; for(uint32_t i=0;i<model->mesh_count;i++){ p=(uint32_t)buf[model->mesh_table_offset+i*4u]|((uint32_t)buf[model->mesh_table_offset+i*4u+1]<<8)|((uint32_t)buf[model->mesh_table_offset+i*4u+2]<<16)|((uint32_t)buf[model->mesh_table_offset+i*4u+3]<<24); if(p) break; } if(p && p+8u<=n) { uint32_t off=(uint32_t)buf[p]|((uint32_t)buf[p+1]<<8)|((uint32_t)buf[p+2]<<16)|((uint32_t)buf[p+3]<<24); uint16_t cnt=(uint16_t)buf[p+4]|((uint16_t)buf[p+5]<<8); if(off<n && cnt && cnt<=(n-off)/16u) { size_t vsz=(size_t)cnt*16u; r=tsfp_vif_scan(buf+off,vsz,vif); if(r==0) run_vu_path(buf+off,vsz,gif); } } } } } } }
    }
    free(buf); fclose(fp); return r;
}
static void run_vu_path(const uint8_t *vif_data,size_t vif_size,TsFpGifSummary *gif) {
    uint8_t *vu_mem=(uint8_t*)malloc(16u*1024u), *gif_mem=(uint8_t*)malloc(64u*1024u), *elf=NULL;
    FILE *fp=NULL; size_t n,off,len; long end;
    TsFpVifMemorySummary vm; TsFpVuState vs; TsFpGifVertex verts[512];
    if(!vu_mem||!gif_mem) goto done;
    if(tsfp_vif_unpack_memory(vif_data,vif_size,vu_mem,16u*1024u,&vm)!=0) goto done;
    fp=fopen(BOOT_PATH,"rb"); if(!fp) goto done;
    if(fseek(fp,0,SEEK_END)!=0) goto done;
    end=ftell(fp); if(end<=0||end>32L*1024L*1024L) goto done;
    if(fseek(fp,0,SEEK_SET)!=0) goto done;
    elf=(uint8_t*)malloc((size_t)end); if(!elf) goto done;
    n=fread(elf,1,(size_t)end,fp); if(n!=(size_t)end) goto done;
    if(tsfp_vu_find_vutext(elf,n,&off,&len)!=0) goto done;
    tsfp_vu_state_init(&vs,vu_mem,16u*1024u,gif_mem,64u*1024u);
    if(tsfp_vu_execute(elf+off,len,0x683u,&vs,4096)!=0) goto done;
    if(tsfp_gif_parse(gif_mem,vs.gif_used,gif,verts,512)==0){
        preview_count=gif->vertices<512?gif->vertices:512;
        float minx=1e30f,miny=1e30f,maxx=-1e30f,maxy=-1e30f;
        for(size_t i=0;i<preview_count;i++){if(verts[i].x<minx)minx=verts[i].x;if(verts[i].x>maxx)maxx=verts[i].x;if(verts[i].y<miny)miny=verts[i].y;if(verts[i].y>maxy)maxy=verts[i].y;}
        float sx=(maxx-minx)>0?760.0f/(maxx-minx):1.0f,sy=(maxy-miny)>0?400.0f/(maxy-miny):1.0f,s=sx<sy?sx:sy;
        for(size_t i=0;i<preview_count;i++){preview[i].x=100.0f+(verts[i].x-minx)*s;preview[i].y=120.0f+(verts[i].y-miny)*s;preview[i].z=0.5f;preview[i].color=((unsigned)verts[i].a<<24)|((unsigned)verts[i].b<<16)|((unsigned)verts[i].g<<8)|verts[i].r;}
    }
done:
    if(fp) fclose(fp);
    free(elf); free(gif_mem); free(vu_mem);
}
static uint32_t probe_boot_vu(void) {
    FILE *fp=fopen(BOOT_PATH,"rb");
    uint8_t *buf=NULL;
    size_t n,off,len;
    uint32_t xg=UINT32_MAX;
    if(!fp) return xg;
    if(fseek(fp,0,SEEK_END)!=0) { fclose(fp); return xg; }
    long end=ftell(fp);
    if(end<=0 || end>32L*1024L*1024L) { fclose(fp); return xg; }
    if(fseek(fp,0,SEEK_SET)!=0) { fclose(fp); return xg; }
    buf=(uint8_t*)malloc((size_t)end);
    if(!buf) { fclose(fp); return xg; }
    n=fread(buf,1,(size_t)end,fp);
    fclose(fp);
    if(n==(size_t)end && tsfp_vu_find_vutext(buf,n,&off,&len)==0) {
        uint32_t pc=UINT32_MAX;
        if(len && tsfp_vu_probe(buf+off,len,0x683u,&pc)==0) xg=pc;
    }
    free(buf);
    return xg;
}
static void draw(int result,const TsP5ckInfo *info,const TsP5ckEntry *entry,const TsFpResourceSummary *resource,const TsFpModelHeader *model,const TsFpGeometrySummary *geometry,const TsFpVifSummary *vif,const TsFpGifSummary *gif,uint32_t xgkick_pc) {
    unsigned int status=0xFFE03030; if(result==0) status=0xFF20C060; else if(result==-10) status=0xFFE0A020;
    vita2d_clear_screen(); vita2d_draw_rectangle(40,40,880,480,0xFF181818); vita2d_draw_rectangle(80,90,800,64,status);
    if(info->entry_count) { float w=(float)(info->entry_count>1000?800:(info->entry_count*800u)/1000u); vita2d_draw_rectangle(80,190,w,42,0xFF40A0FF); }
    if(entry->length) { float w=(float)(entry->length>200000?800:(entry->length*800u)/200000u); vita2d_draw_rectangle(80,270,w,42,0xFF8040FF); }
    if(resource->string_count) { float w=(float)(resource->string_count>32?800:(resource->string_count*800u)/32u); vita2d_draw_rectangle(80,350,w,42,0xFFFFA040); }
    if(resource->metadata_offset) { float w=(float)(resource->metadata_offset>64?800:(resource->metadata_offset*800u)/64u); vita2d_draw_rectangle(80,430,w,30,0xFF40C0C0); }
    if(model->mesh_count) { float w=(float)(model->mesh_count>100?800:(model->mesh_count*800u)/100u); vita2d_draw_rectangle(80,390,w,20,0xFFC040A0); }
    if(model->material_count) { float w=(float)(model->material_count>100?800:(model->material_count*800u)/100u); vita2d_draw_rectangle(80,420,w,20,0xFF40C080); }
    if(vif->payload_bytes) { float w=(float)(vif->payload_bytes>64?800:(vif->payload_bytes*800u)/64u); vita2d_draw_rectangle(80,470,w,12,0xFF80C060); }
    if(preview_count>=3){ size_t n=preview_count-(preview_count%3); vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES,preview,n); }
    if(xgkick_pc!=UINT32_MAX) vita2d_draw_rectangle(80,500,800,10,0xFFC08040);
    if(geometry->submesh_count) { float w=(float)(geometry->submesh_count>256?800:(geometry->submesh_count*800u)/256u); vita2d_draw_rectangle(80,450,w,18,0xFF60A0E0); }
}
int main(void) {
    SceCtrlData pad; TsP5ckInfo info; TsP5ckEntry entry; TsFpResourceSummary resource; TsFpModelHeader model; TsFpGeometrySummary geometry; TsFpVifSummary vif; TsFpGifSummary gif; int result; uint32_t xgkick_pc;
    memset(&pad,0,sizeof(pad)); memset(&info,0,sizeof(info)); memset(&entry,0,sizeof(entry)); memset(&resource,0,sizeof(resource)); memset(&model,0,sizeof(model)); memset(&geometry,0,sizeof(geometry)); memset(&vif,0,sizeof(vif)); memset(&gif,0,sizeof(gif));
    result=load_probe(&info,&entry,&resource,&model,&geometry,&vif,&gif); xgkick_pc=probe_boot_vu(); vita2d_init();
    for(;;) { sceCtrlPeekBufferPositive(0,&pad,1); if(pad.buttons&SCE_CTRL_START) break; vita2d_start_drawing(); draw(result,&info,&entry,&resource,&model,&geometry,&vif,&gif,xgkick_pc); vita2d_end_drawing(); vita2d_swap_buffers(); sceDisplayWaitVblankStart(); }
    vita2d_fini(); sceKernelExitProcess(0); return 0;
}
