#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <vita2d.h>
#include "p5ck.h"
#include "fp_resource.h"
#include "fp_model.h"
#define DATA_PATH "ux0:data/TimeSplitters/PAK/CHR.PAK"
static int load_probe(TsP5ckInfo *info, TsP5ckEntry *entry, TsFpResourceSummary *resource, TsFpModelHeader *model) {
    FILE *fp=fopen(DATA_PATH,"rb"); uint8_t buf[64]; size_t n; int r;
    if(!fp) return -10;
    r=ts_p5ck_read_info(fp,info);
    if(r==0 && info->entry_count==0) r=-11;
    if(r==0) r=ts_p5ck_read_entry(fp,info,0,entry);
    if(r==0) {
        uint32_t size=entry->compressed_length ? entry->compressed_length : entry->length;
        if(size>sizeof(buf)) size=sizeof(buf);
        if(fseek(fp,(long)entry->offset,SEEK_SET)!=0) r=-12;
        else { n=fread(buf,1,size,fp); if(n<8) r=-13; else { r=tsfp_resource_probe(buf,n,resource); if(r!=0) r=tsfp_model_probe(buf,n,model); } }
    }
    fclose(fp); return r;
}
static void draw(int result,const TsP5ckInfo *info,const TsP5ckEntry *entry,const TsFpResourceSummary *resource,const TsFpModelHeader *model) {
    unsigned int status=0xFFE03030; if(result==0) status=0xFF20C060; else if(result==-10) status=0xFFE0A020;
    vita2d_clear_screen(); vita2d_draw_rectangle(40,40,880,480,0xFF181818); vita2d_draw_rectangle(80,90,800,64,status);
    if(info->entry_count) { float w=(float)(info->entry_count>1000?800:(info->entry_count*800u)/1000u); vita2d_draw_rectangle(80,190,w,42,0xFF40A0FF); }
    if(entry->length) { float w=(float)(entry->length>200000?800:(entry->length*800u)/200000u); vita2d_draw_rectangle(80,270,w,42,0xFF8040FF); }
    if(resource->string_count) { float w=(float)(resource->string_count>32?800:(resource->string_count*800u)/32u); vita2d_draw_rectangle(80,350,w,42,0xFFFFA040); }
    if(resource->metadata_offset) { float w=(float)(resource->metadata_offset>64?800:(resource->metadata_offset*800u)/64u); vita2d_draw_rectangle(80,430,w,30,0xFF40C0C0); }
    if(model->mesh_count) { float w=(float)(model->mesh_count>100?800:(model->mesh_count*800u)/100u); vita2d_draw_rectangle(80,390,w,20,0xFFC040A0); }
}
int main(void) {
    SceCtrlData pad; TsP5ckInfo info; TsP5ckEntry entry; TsFpResourceSummary resource; TsFpModelHeader model; int result;
    memset(&pad,0,sizeof(pad)); memset(&info,0,sizeof(info)); memset(&entry,0,sizeof(entry)); memset(&resource,0,sizeof(resource)); memset(&model,0,sizeof(model));
    result=load_probe(&info,&entry,&resource,&model); vita2d_init();
    for(;;) { sceCtrlPeekBufferPositive(0,&pad,1); if(pad.buttons&SCE_CTRL_START) break; vita2d_start_drawing(); draw(result,&info,&entry,&resource,&model); vita2d_end_drawing(); vita2d_swap_buffers(); sceDisplayWaitVblankStart(); }
    vita2d_fini(); sceKernelExitProcess(0); return 0;
}
