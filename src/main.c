#include <stdio.h>
#include <string.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <vita2d.h>
#include "p5ck.h"
#define DATA_PATH "ux0:data/TimeSplitters/PAK/CHR.PAK"
static int probe(TsP5ckInfo *info) {
    FILE *fp=fopen(DATA_PATH,"rb"); int r;
    if(!fp) return -10;
    r=ts_p5ck_read_info(fp,info); fclose(fp); return r;
}
static void draw(int result,const TsP5ckInfo *info) {
    unsigned int c=0xFF505050;
    if(result==0) c=0xFF20C060; else if(result==-10) c=0xFFE0A020; else c=0xFFE03030;
    vita2d_clear_screen();
    vita2d_draw_rectangle(40,40,880,480,0xFF202020);
    vita2d_draw_rectangle(80,100,800,80,c);
    if(result==0) {
        float w=(float)(info->entry_count>1000?800:(info->entry_count*800)/1000);
        vita2d_draw_rectangle(80,240,w,50,0xFF40A0FF);
        vita2d_draw_rectangle(80,330,800,50,0xFF8040FF);
    }
}
int main(void) {
    SceCtrlData pad; TsP5ckInfo info; int result;
    memset(&pad,0,sizeof(pad)); result=probe(&info);
    vita2d_init();
    for(;;) {
        sceCtrlPeekBufferPositive(0,&pad,1);
        if(pad.buttons&SCE_CTRL_START) break;
        vita2d_start_drawing(); draw(result,&info); vita2d_end_drawing();
        vita2d_swap_buffers(); sceDisplayWaitVblankStart();
    }
    vita2d_fini(); sceKernelExitProcess(0); return 0;
}
