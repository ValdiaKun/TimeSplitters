#include <stdio.h>
#include <string.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>

#include <vita2d.h>

static void wait_for_exit(void)
{
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START)
            break;

        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceDisplayWaitVblankStart();
    }
}

int main(void)
{
    vita2d_init();
    wait_for_exit();
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
