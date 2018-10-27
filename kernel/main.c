#include <vitasdkkern.h>
#include <taihen.h>

#include "include/koc.h"

int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, void *addr);
int module_get_export_func(SceUID pid, const char *modname, uint32_t libnid, uint32_t funcnid, void *func);

kocGenericClockTable_t g_genericClockTable[] = {
    {55},
    {111},
    {166},
    {222},
    {333}
};

kocArmClockTable_t g_armClockTable[] = {
    { 2, 8,  16},
    { 2, 7,  19},
    { 2, 6,  22},
    { 2, 5,  24},
    { 2, 4,  27},
    {16, 3,  29},
    { 2, 3,  30},
    { 2, 2,  32},
    { 2, 1,  35},
    { 2, 0,  37},
    { 3, 7,  43},
    { 3, 6,  48},
    { 4, 8,  51},
    { 3, 5,  53},
    { 3, 4,  58},
    { 3, 3,  63},
    { 4, 6,  65},
    {12, 7,  66},
    { 3, 2,  69},
    { 4, 5,  72},
    { 3, 1,  74},
    { 3, 0,  79},
    {12, 5,  82},
    { 4, 3,  86},
    { 5, 7,  90},
    { 4, 2,  93},
    {12, 3,  97},
    { 4, 1, 100},
    {12, 2, 105},
    { 4, 0, 107},
    { 5, 5, 110},
    {12, 1, 113},
    { 5, 4, 121},
    { 5, 3, 131},
    { 6, 6, 135},
    {13, 7, 136},
    { 5, 2, 142},
    { 6, 5, 148},
    { 5, 1, 152},
    { 5, 0, 162},
    { 6, 4, 163},
    {13, 5, 168},
    { 6, 3, 176},
    { 7, 7, 183},
    { 6, 2, 190},
    {13, 3, 199},
    { 6, 1, 204},
    {13, 2, 214},
    { 6, 0, 218},
    { 7, 5, 225},
    {13, 1, 230},
    { 9, 8, 245},
    { 7, 4, 246},
    { 7, 3, 266},
    { 8, 6, 273},
    { 9, 7, 277},
    { 7, 2, 287},
    { 8, 5, 301},
    { 7, 1, 308},
    {10, 0, 328},
    { 7, 0, 329},
    { 9, 5, 339},
    { 8, 3, 356},
    { 9, 4, 370},
    { 8, 2, 384},
    { 9, 3, 401},
    { 8, 1, 411},
    {14, 1, 412},
    { 9, 2, 432},
    {15, 2, 433},
    { 8, 0, 439},
    { 9, 1, 463},
    { 9, 0, 494}
};

uint16_t *g_ScePervasiveBaseClk_r1;
uint8_t  *g_ScePervasiveBaseClk_r2;
uint32_t *g_ScePower_clockSpeed;

tai_hook_ref_t g_hookrefs[6];
SceUID         g_hooks[6];

uint8_t g_kocCurrentStep[KOC_DEVICE_MAX] = {
    61, 3, 2, 2, 1 /* CPU = 339, BUS = 222, GPU = 166, ES4 = 166, XBAR = 111 */
};
uint8_t g_kocEnabled = 0;
SceUID g_lock;

int (*_kscePowerGetArmClockFrequency)(void);
int (*_kscePowerGetBusClockFrequency)(void);
int (*_kscePowerGetGpuClockFrequency)(void);
int (*_kscePowerGetGpuEs4ClockFrequency)(int *, int *);
int (*_kscePowerGetGpuXbarClockFrequency)(void);

int (*_kscePowerSetArmClockFrequency)(int);
int (*_kscePowerSetBusClockFrequency)(int);
int (*_kscePowerSetGpuClockFrequency)(int);
int (*_kscePowerSetGpuEs4ClockFrequency)(int, int);
int (*_kscePowerSetGpuXbarClockFrequency)(int);

void *pa2va(uint32_t pa) {
    uint32_t vaddr, paddr, i, va = 0;
    for (i = 0; i < 0x100000; i++) {
        vaddr = i << 12;
        __asm__("mcr p15,0,%1,c7,c8,0\n\t"
                "mrc p15,0,%0,c7,c4,0\n\t" : "=r" (paddr) : "r" (vaddr));
        if ((pa & 0xFFFFF000) == (paddr & 0xFFFFF000)) {
            va = vaddr + (pa & 0xFFF);
            break;
        }
    }
    return (void *)va;
}

static int kocFindArmClockTableIndex(uint16_t mul, uint8_t div) {
    for (int i = 0; i <= KOC_MAX_CPU_STEP; i++) {
        if (mul == g_armClockTable[i].mul &&
                div == g_armClockTable[i].div) {
            return i;
        }
    }
    return -1;
}

static int kocFindArmClockTableIndexByClock(uint32_t clock) {
    for (int i = 0; i <= KOC_MAX_CPU_STEP; i++) {
        if (clock == g_armClockTable[i].clock) {
            return i;
        }
    }
    return -1;
}

static int kocFindClockTableIndexByClock(uint32_t clock) {
    for (int i = 0; i <= KOC_MAX_GENERIC_STEP; i++) {
        if (clock == g_genericClockTable[i].clock) {
            return i;
        }
    }
    return -1;
}


void kocEnable() {
    g_kocEnabled = 1;
}

void kocDisable() {
    kocSetArmClockFrequency(339);
    kocSetBusClockFrequency(222);
    kocSetGpuClockFrequency(166);
    kocSetGpuEs4ClockFrequency(166);
    kocSetGpuXbarClockFrequency(111);

    g_kocEnabled = 0;
}

int kocIsEnabled() {
    return g_kocEnabled;
}

int kocGetArmMul() {
    return *g_ScePervasiveBaseClk_r1;
}
int kocGetArmDiv() {
    return *g_ScePervasiveBaseClk_r2;
}

int kocGetArmClockFrequency() {
    if (g_kocEnabled) {
        ksceKernelLockMutex(g_lock, 1, NULL);
        uint16_t mul = *g_ScePervasiveBaseClk_r1;
        uint8_t div = *g_ScePervasiveBaseClk_r2;

        int i = kocFindArmClockTableIndex(mul, div);
        ksceKernelUnlockMutex(g_lock, 1);

        return i >= 0 ? g_armClockTable[i].clock : 0;
    }
    return _kscePowerGetArmClockFrequency();
}

int kocGetBusClockFrequency() {
    return _kscePowerGetBusClockFrequency();
}

int kocGetGpuClockFrequency() {
    return _kscePowerGetGpuClockFrequency();
}

int kocGetGpuEs4ClockFrequency() {
    int freq = 0, freq2;
    _kscePowerGetGpuEs4ClockFrequency(&freq, &freq2);
    return freq;
}

int kocGetGpuXbarClockFrequency() {
    return _kscePowerGetGpuXbarClockFrequency();
}



int kocSetArmClockFrequency(int freq) {
    int i = kocFindArmClockTableIndexByClock(freq);
    if (i < 0 || i > KOC_MAX_CPU_STEP)
        return i;

    g_kocCurrentStep[KOC_DEVICE_CPU] = i;
    _kscePowerSetArmClockFrequency(freq);
    return 0;
}

int kocSetBusClockFrequency(int freq) {
    int i = kocFindClockTableIndexByClock(freq);
    if (i < 0 || i > KOC_MAX_BUS_STEP)
        return i;

    g_kocCurrentStep[KOC_DEVICE_BUS] = i;
    _kscePowerSetBusClockFrequency(freq);
    return 0;
}

int kocSetGpuClockFrequency(int freq) {
    int i = kocFindClockTableIndexByClock(freq);
    if (i < 0 || i > KOC_MAX_GPU_STEP)
        return i;

    g_kocCurrentStep[KOC_DEVICE_GPU] = i;
    _kscePowerSetGpuClockFrequency(freq);
    return 0;
}

int kocSetGpuEs4ClockFrequency(int freq) {
    int i = kocFindClockTableIndexByClock(freq);
    if (i < 0 || i > KOC_MAX_GPU_ES4_STEP)
        return i;

    g_kocCurrentStep[KOC_DEVICE_GPU_ES4] = i;
    _kscePowerSetGpuEs4ClockFrequency(freq, freq);
    return 0;
}

int kocSetGpuXbarClockFrequency(int freq) {
    int i = kocFindClockTableIndexByClock(freq);
    if (i < 0 || i > KOC_MAX_GPU_XBAR_STEP)
        return i;

    g_kocCurrentStep[KOC_DEVICE_GPU_XBAR] = i;
    _kscePowerSetGpuXbarClockFrequency(freq);
    return 0;
}


static int kscePowerSetArmClockFrequency_patched(int freq) {
    int ret = TAI_CONTINUE(int, g_hookrefs[0], g_kocEnabled ? 444 : freq);
    if (g_kocEnabled) {
        ksceKernelLockMutex(g_lock, 1, NULL);

        *g_ScePower_clockSpeed = g_armClockTable[g_kocCurrentStep[KOC_DEVICE_CPU]].clock;
        while (*g_ScePervasiveBaseClk_r1 != g_armClockTable[g_kocCurrentStep[KOC_DEVICE_CPU]].mul ||
                *g_ScePervasiveBaseClk_r2 != g_armClockTable[g_kocCurrentStep[KOC_DEVICE_CPU]].div) {
            ksceKernelDelayThread(5000);
            *g_ScePervasiveBaseClk_r1 = g_armClockTable[g_kocCurrentStep[KOC_DEVICE_CPU]].mul;
            *g_ScePervasiveBaseClk_r2 = g_armClockTable[g_kocCurrentStep[KOC_DEVICE_CPU]].div;
        }

        ksceKernelUnlockMutex(g_lock, 1);
    }
    return ret;
}

static int kscePowerSetBusClockFrequency_patched(int freq) {
    return TAI_CONTINUE(int, g_hookrefs[1],
        g_kocEnabled ? g_genericClockTable[g_kocCurrentStep[KOC_DEVICE_BUS]].clock : freq);
}

static int kscePowerSetGpuClockFrequency_patched(int freq) {
    return TAI_CONTINUE(int, g_hookrefs[2],
        g_kocEnabled ? g_genericClockTable[g_kocCurrentStep[KOC_DEVICE_GPU]].clock : freq);
}

static int kscePowerSetGpuEs4ClockFrequency_patched(int freq, int freq2) {
    return TAI_CONTINUE(int, g_hookrefs[3],
        g_kocEnabled ? g_genericClockTable[g_kocCurrentStep[KOC_DEVICE_GPU_ES4]].clock : freq,
        g_kocEnabled ? g_genericClockTable[g_kocCurrentStep[KOC_DEVICE_GPU_ES4]].clock : freq2);
}

static int kscePowerSetGpuXbarClockFrequency_patched(int freq) {
    return TAI_CONTINUE(int, g_hookrefs[4],
        g_kocEnabled ? g_genericClockTable[g_kocCurrentStep[KOC_DEVICE_GPU_XBAR]].clock : freq);
}

static int kscePowerGetArmClockFrequency_patched() {
    int ret = TAI_CONTINUE(int, g_hookrefs[5]);
    return g_kocEnabled ? kocGetArmClockFrequency() : ret;
}


void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
    
    g_lock = ksceKernelCreateMutex("kocLock", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);

    tai_module_info_t tai_ScePower;
    tai_ScePower.size = sizeof(tai_module_info_t);
    taiGetModuleInfoForKernel(KERNEL_PID, "ScePower", &tai_ScePower);
    module_get_offset(KERNEL_PID, tai_ScePower.modid, 1, 0x4124 + 0xA4, &g_ScePower_clockSpeed);

    g_ScePervasiveBaseClk_r1 = pa2va(0xE3103000);
    g_ScePervasiveBaseClk_r2 = pa2va(0xE3103004);
    
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0xABC6F88F, &_kscePowerGetArmClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0x478FE6F5, &_kscePowerGetBusClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0x64641E6A, &_kscePowerGetGpuClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0x475BCC82, &_kscePowerGetGpuEs4ClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0x0A750DEE, &_kscePowerGetGpuXbarClockFrequency);

    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0x74DB5AE5, &_kscePowerSetArmClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0xB8D7B3FB, &_kscePowerSetBusClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0x621BD8FD, &_kscePowerSetGpuClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0x264C24FC, &_kscePowerSetGpuEs4ClockFrequency);
    module_get_export_func(KERNEL_PID, "ScePower", 0x1590166F, 0xA7739DBE, &_kscePowerSetGpuXbarClockFrequency);

    // ScePowerForDriver
    g_hooks[0] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[0],
            "ScePower", 0x1590166F, 0x74DB5AE5, kscePowerSetArmClockFrequency_patched);
    g_hooks[1] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[1],
            "ScePower", 0x1590166F, 0xB8D7B3FB, kscePowerSetBusClockFrequency_patched);
    g_hooks[2] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[2],
            "ScePower", 0x1590166F, 0x621BD8FD, kscePowerSetGpuClockFrequency_patched);
    g_hooks[3] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[3],
            "ScePower", 0x1590166F, 0x264C24FC, kscePowerSetGpuEs4ClockFrequency_patched);
    g_hooks[4] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[4],
            "ScePower", 0x1590166F, 0xA7739DBE, kscePowerSetGpuXbarClockFrequency_patched);
            
    // Return correct arm clock speed
    g_hooks[5] = taiHookFunctionExportForKernel(KERNEL_PID, &g_hookrefs[5],
            "ScePower", 0x1590166F, 0xABC6F88F, kscePowerGetArmClockFrequency_patched);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {

    if (g_hooks[0] >= 0)
        taiHookReleaseForKernel(g_hooks[0], g_hookrefs[0]);
    if (g_hooks[1] >= 0)
        taiHookReleaseForKernel(g_hooks[1], g_hookrefs[1]);
    if (g_hooks[2] >= 0)
        taiHookReleaseForKernel(g_hooks[2], g_hookrefs[2]);
    if (g_hooks[3] >= 0)
        taiHookReleaseForKernel(g_hooks[3], g_hookrefs[3]);
    if (g_hooks[4] >= 0)
        taiHookReleaseForKernel(g_hooks[4], g_hookrefs[4]);
    if (g_hooks[5] >= 0)
        taiHookReleaseForKernel(g_hooks[5], g_hookrefs[5]);

    ksceKernelDeleteMutex(g_lock);

    return SCE_KERNEL_STOP_SUCCESS;
}
