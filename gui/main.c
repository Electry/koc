#include <vitasdk.h>
#include <taihen.h>

#include "display.h"
#include "../kernel/include/koc.h"

#define SECOND 1000000

typedef enum {
    KOC_GUI_MENU_HIDDEN,
    KOC_GUI_MENU_FPS,
    KOC_GUI_MENU_FULL,
    KOC_GUI_MENU_MAX
} kocGuiMenu_t;

typedef enum {
    KOC_GUI_MENUCTRL_CPU,
    KOC_GUI_MENUCTRL_BUS,
    KOC_GUI_MENUCTRL_GPU,
    KOC_GUI_MENUCTRL_GPU_ES4,
    KOC_GUI_MENUCTRL_GPU_XBAR,
    KOC_GUI_MENUCTRL_MAX
} kocGuiMenuControl_t;

#define KOC_GUI_BUTTONS_FAST_MOVE_DELAY 500000
#define KOC_GUI_FPS_N 5

int g_genericClocks[] = {
    55, 111, 166, 222, 333
};

int g_cpuClocks[] = {
     16,  19,  22,  24,  27,  29,  30,  32,  35,  37,
     43,  48,  51,  53,  58,  63,  65,  66,  69,  72,
     74,  79,  82,  86,  90,  93,  97, 100, 105, 107,
    110, 113, 121, 131, 135, 136, 142, 148, 152, 162,
    163, 168, 176, 183, 190, 199, 204, 214, 218, 225,
    230, 245, 246, 266, 273, 277, 287, 301, 308, 328,
    329, 339, 356, 370, 384, 401, 411, 412, 432, 433,
    439, 463, 494
};

static SceUID g_hook[1];
static tai_hook_ref_t g_hook_ref[1];

static kocGuiMenu_t g_menu = KOC_GUI_MENU_HIDDEN;
static kocGuiMenuControl_t g_menuControl = KOC_GUI_MENUCTRL_CPU;
static uint32_t g_buttonsPressed = 0;
static SceUInt32 g_buttonsLastChange = 0;

static SceUInt32 g_tick_last = 0;
static int g_fps = 0;
static uint32_t g_frametime_sum = 0;
static uint8_t g_frametime_n = 0;

static uint8_t g_clocks[KOC_DEVICE_MAX] = {
    61, 3, 2, 2, 1 /* CPU = 339, BUS = 222, GPU = 166, ES4 = 166, XBAR = 111 */
};

void kocGuiDrawMenuFreqItem(int x, int y, const char *label, int freq, kocGuiMenuControl_t item) {
    setTextColor(255, 255, 255);
    drawStringF(x, y, "%-9s [     ]", label);

    if (g_menuControl == item) {
        setTextColor(0, 155, 255);
    }

    drawStringF(x + 144, y, "%3d", freq);
}

void kocGuiMoveRight() {
    switch (g_menuControl) {
        case KOC_GUI_MENUCTRL_CPU:
            if (g_clocks[KOC_DEVICE_CPU] < KOC_MAX_CPU_STEP)
                g_clocks[KOC_DEVICE_CPU]++;
            kocSetArmClockFrequency(g_cpuClocks[g_clocks[KOC_DEVICE_CPU]]);
            break;
        case KOC_GUI_MENUCTRL_BUS:
            if (g_clocks[KOC_DEVICE_BUS] < KOC_MAX_BUS_STEP)
                g_clocks[KOC_DEVICE_BUS]++;
            kocSetBusClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_BUS]]);
            break;
        case KOC_GUI_MENUCTRL_GPU:
            if (g_clocks[KOC_DEVICE_GPU] < KOC_MAX_GPU_STEP)
                g_clocks[KOC_DEVICE_GPU]++;
            kocSetGpuClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_GPU]]);
            break;
        case KOC_GUI_MENUCTRL_GPU_ES4:
            if (g_clocks[KOC_DEVICE_GPU_ES4] < KOC_MAX_GPU_ES4_STEP)
                g_clocks[KOC_DEVICE_GPU_ES4]++;
            kocSetGpuEs4ClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_GPU_ES4]]);
            break;
        case KOC_GUI_MENUCTRL_GPU_XBAR:
            if (g_clocks[KOC_DEVICE_GPU_XBAR] < KOC_MAX_GPU_XBAR_STEP)
                g_clocks[KOC_DEVICE_GPU_XBAR]++;
            kocSetGpuXbarClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_GPU_XBAR]]);
            break;
        default:
            break;
    }
}

void kocGuiMoveLeft() {
    switch (g_menuControl) {
        case KOC_GUI_MENUCTRL_CPU:
            if (g_clocks[KOC_DEVICE_CPU] > 0)
                g_clocks[KOC_DEVICE_CPU]--;
            kocSetArmClockFrequency(g_cpuClocks[g_clocks[KOC_DEVICE_CPU]]);
            break;
        case KOC_GUI_MENUCTRL_BUS:
            if (g_clocks[KOC_DEVICE_BUS] > 0)
                g_clocks[KOC_DEVICE_BUS]--;
            kocSetBusClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_BUS]]);
            break;
        case KOC_GUI_MENUCTRL_GPU:
            if (g_clocks[KOC_DEVICE_GPU] > 0)
                g_clocks[KOC_DEVICE_GPU]--;
            kocSetGpuClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_GPU]]);
            break;
        case KOC_GUI_MENUCTRL_GPU_ES4:
            if (g_clocks[KOC_DEVICE_GPU_ES4] > 0)
                g_clocks[KOC_DEVICE_GPU_ES4]--;
            kocSetGpuEs4ClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_GPU_ES4]]);
            break;
        case KOC_GUI_MENUCTRL_GPU_XBAR:
            if (g_clocks[KOC_DEVICE_GPU_XBAR] > 0)
                g_clocks[KOC_DEVICE_GPU_XBAR]--;
            kocSetGpuXbarClockFrequency(g_genericClocks[g_clocks[KOC_DEVICE_GPU_XBAR]]);
            break;
        default:
            break;
    }
}

void kocGuiCheckButtons() {
    SceCtrlData ctrl;
    sceCtrlPeekBufferPositive(0, &ctrl, 1);
    uint32_t buttons_new = ctrl.buttons & ~g_buttonsPressed;
    SceUInt32 time_now = sceKernelGetProcessTimeLow();

    // Toggle menu
    if (ctrl.buttons & SCE_CTRL_SELECT) {
        // Show menu
        if (g_menu < KOC_GUI_MENU_MAX && buttons_new & SCE_CTRL_UP) {
            g_menu++;
        }
        // Hide menu
        else if (g_menu > 0 && buttons_new & SCE_CTRL_DOWN) {
            g_menu--;
        }
    }
    // Inside full menu
    else if (g_menu == KOC_GUI_MENU_FULL) {
        // Move /\ | \/ in menu
        if (g_menuControl < KOC_GUI_MENUCTRL_MAX && buttons_new & SCE_CTRL_DOWN) {
            g_menuControl++;
        } else if (g_menuControl > 0 && buttons_new & SCE_CTRL_UP) {
            g_menuControl--;
        }

        // Move <- | -> in menu control

        // Simple move
        if (buttons_new & SCE_CTRL_RIGHT) {
            g_buttonsLastChange = time_now;
            kocGuiMoveRight();
        } else if (buttons_new & SCE_CTRL_LEFT) {
            g_buttonsLastChange = time_now;
            kocGuiMoveLeft();
        }

        // Fast move
        if (ctrl.buttons & SCE_CTRL_RIGHT &&
                time_now - g_buttonsLastChange > KOC_GUI_BUTTONS_FAST_MOVE_DELAY) {
            kocGuiMoveRight();
        } else if (ctrl.buttons & SCE_CTRL_LEFT &&
                time_now - g_buttonsLastChange > KOC_GUI_BUTTONS_FAST_MOVE_DELAY) {
            kocGuiMoveLeft();
        }
    }

    g_buttonsPressed = ctrl.buttons;
}


int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {
    SceUInt32 tick_now = sceKernelGetProcessTimeLow();

    // Calculate FPS and frametime
    uint32_t frametime = tick_now - g_tick_last;
    if (g_frametime_n > KOC_GUI_FPS_N) {
        uint32_t frametime_avg = g_frametime_sum / g_frametime_n;
        g_fps = (SECOND + frametime_avg - 1) / frametime_avg;
        g_frametime_n = 0;
        g_frametime_sum = 0;
    }
    g_frametime_n++;
    g_frametime_sum += frametime;
    g_tick_last = tick_now;

    updateFrameBuf(pParam);
    kocGuiCheckButtons();

    setTextColor(255, 255, 255);
    
    if (g_menu >= KOC_GUI_MENU_FPS) {
        drawStringF(0,  0,  "%2d", g_fps);
    }
    
    if (g_menu >= KOC_GUI_MENU_FULL) {
        drawStringF(0, 20,  "%-17s", "");
        kocGuiDrawMenuFreqItem(0, 40,  "CPU", kocGetArmClockFrequency(), KOC_GUI_MENUCTRL_CPU);
        drawStringF(240, 40, "%d : %d", kocGetArmMul(), kocGetArmDiv());
        kocGuiDrawMenuFreqItem(0, 62,  "BUS", kocGetBusClockFrequency(), KOC_GUI_MENUCTRL_BUS);
        kocGuiDrawMenuFreqItem(0, 84,  "GPU", kocGetGpuClockFrequency(), KOC_GUI_MENUCTRL_GPU);
        kocGuiDrawMenuFreqItem(0, 106, "GPU ES4", kocGetGpuEs4ClockFrequency(), KOC_GUI_MENUCTRL_GPU_ES4);
        kocGuiDrawMenuFreqItem(0, 128, "GPU XBAR", kocGetGpuXbarClockFrequency(), KOC_GUI_MENUCTRL_GPU_XBAR);
    }

    return TAI_CONTINUE(int, g_hook_ref[0], pParam, sync);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
    kocEnable();
    
    g_hook[0] = taiHookFunctionImport(&g_hook_ref[0],
                                      TAI_MAIN_MODULE,
                                      TAI_ANY_LIBRARY,
                                      0x7A410B64,
                                      sceDisplaySetFrameBuf_patched);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    if (g_hook[0] >= 0)
        taiHookRelease(g_hook[0], g_hook_ref[0]);

    kocDisable();

    return SCE_KERNEL_STOP_SUCCESS;
}
