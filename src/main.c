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
#define DATA_PATH "ux0:data/TimeSplitters/PAK/CHR.PAK"
#define BOOT_PATH "ux0:data/TimeSplitters/SLED_530.66"
static int load_probe(TsP5ckInfo *info, TsP5ckEntry *entry, TsFpResourceSummary *resource, TsFpModelHeader *model, TsFpGeometrySummary *geometry, TsFpVifSummary *vif) {
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
        else { n=fread(buf,1,size,fp); if(n<size) r=-15; else { r=tsfp_resource_probe(buf,n,resource); if(r!=0) { r=tsfp_model_probe(buf,n,model); if(r==0) { r=tsfp_geometry_probe(buf,n,model->mesh_table_offset,model->mesh_count,geometry); if(r==0 && model->mesh_count) { uint32_t p=0; for(uint32_t i=0;i<model->mesh_count;i++){ p=(uint32_t)buf[model->mesh_table_offset+i*4u]|((uint32_t)buf[model->mesh_table_offset+i*4u+1]<<8)|((uint32_t)buf[model->mesh_table_offset+i*4u+2]<<16)|((uint32_t)buf[model->mesh_table_offset+i*4u+3]<<24); if(p) break; } if(p && p+8u<=n) { uint32_t off=(uint32_t)buf[p]|((uint32_t)buf[p+1]<<8)|((uint32_t)buf[p+2]<<16)|((uint32_t)buf[p+3]<<24); uint16_t cnt=(uint16_t)buf[p+4]|((uint16_t)buf[p+5]<<8); if(off<n && cnt && cnt<=(n-off)/16u) r=tsfp_vif_scan(buf+off,(size_t)cnt*16u,vif); } } } } } }
    }
    free(buf); fclose(fp); return r;
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
static void draw(int result,const TsP5ckInfo *info,const TsP5ckEntry *entry,const TsFpResourceSummary *resource,const TsFpModelHeader *model,const TsFpGeometrySummary *geometry,const TsFpVifSummary *vif,uint32_t xgkick_pc) {
    unsigned int status=0xFFE03030; if(result==0) status=0xFF20C060; else if(result==-10) status=0xFFE0A020;
    vita2d_clear_screen(); vita2d_draw_rectangle(40,40,880,480,0xFF181818); vita2d_draw_rectangle(80,90,800,64,status);
    if(info->entry_count) { float w=(float)(info->entry_count>1000?800:(info->entry_count*800u)/1000u); vita2d_draw_rectangle(80,190,w,42,0xFF40A0FF); }
    if(entry->length) { float w=(float)(entry->length>200000?800:(entry->length*800u)/200000u); vita2d_draw_rectangle(80,270,w,42,0xFF8040FF); }
    if(resource->string_count) { float w=(float)(resource->string_count>32?800:(resource->string_count*800u)/32u); vita2d_draw_rectangle(80,350,w,42,0xFFFFA040); }
    if(resource->metadata_offset) { float w=(float)(resource->metadata_offset>64?800:(resource->metadata_offset*800u)/64u); vita2d_draw_rectangle(80,430,w,30,0xFF40C0C0); }
    if(model->mesh_count) { float w=(float)(model->mesh_count>100?800:(model->mesh_count*800u)/100u); vita2d_draw_rectangle(80,390,w,20,0xFFC040A0); }
    if(model->material_count) { float w=(float)(model->material_count>100?800:(model->material_count*800u)/100u); vita2d_draw_rectangle(80,420,w,20,0xFF40C080); }
    if(vif->payload_bytes) { float w=(float)(vif->payload_bytes>64?800:(vif->payload_bytes*800u)/64u); vita2d_draw_rectangle(80,470,w,12,0xFF80C060); }\n    if(xgkick_pc!=UINT32_MAX) vita2d_draw_rectangle(80,500,800,10,0xFFC08040);
    if(geometry->submesh_count) { float w=(float)(geometry->submesh_count>256?800:(geometry->submesh_count*800u)/256u); vita2d_draw_rectangle(80,450,w,18,0xFF60A0E0); }
}
int main(void) {
    SceCtrlData pad; TsP5ckInfo info; TsP5ckEntry entry; TsFpResourceSummary resource; TsFpModelHeader model; TsFpGeometrySummary geometry; TsFpVifSummary vif; int result;
    memset(&pad,0,sizeof(pad)); memset(&info,0,sizeof(info)); memset(&entry,0,sizeof(entry)); memset(&resource,0,sizeof(resource)); memset(&model,0,sizeof(model)); memset(&geometry,0,sizeof(geometry)); memset(&vif,0,sizeof(vif));
    result=load_probe(&info,&entry,&resource,&model,&geometry,&vif); xgkick_pc=probe_boot_vu(); vita2d_init();
    for(;;) { sceCtrlPeekBufferPositive(0,&pad,1); if(pad.buttons&SCE_CTRL_START) break; vita2d_start_drawing(); draw(result,&info,&entry,&resource,&model,&geometry,&vif,xgkick_pc); vita2d_end_drawing(); vita2d_swap_buffers(); sceDisplayWaitVblankStart(); }
    vita2d_fini(); sceKernelExitProcess(0); return 0;
}
