#ifndef _KOC_H_
#define _KOC_H_

#define KOC_MAX_BUS_STEP      3
#define KOC_MAX_GPU_STEP      4
#define KOC_MAX_GPU_ES4_STEP  3
#define KOC_MAX_GPU_XBAR_STEP 2

#define KOC_MAX_GENERIC_STEP  4
#define KOC_MAX_CPU_STEP      72

typedef struct {
    int clock;
} kocGenericClockTable_t;

typedef struct {
    uint16_t mul;
    uint8_t div;
    uint32_t clock;
} kocArmClockTable_t;

typedef enum {
    KOC_DEVICE_CPU,
    KOC_DEVICE_BUS,
    KOC_DEVICE_GPU,
    KOC_DEVICE_GPU_ES4,
    KOC_DEVICE_GPU_XBAR,
    KOC_DEVICE_MAX
} kocDevice_t;

void kocEnable();
void kocDisable();
int kocIsEnabled();

int kocGetArmClockFrequency();
int kocGetBusClockFrequency();
int kocGetGpuClockFrequency();
int kocGetGpuEs4ClockFrequency();
int kocGetGpuXbarClockFrequency();

int kocSetArmClockFrequency(int freq);
int kocSetBusClockFrequency(int freq);
int kocSetGpuClockFrequency(int freq);
int kocSetGpuEs4ClockFrequency(int freq);
int kocSetGpuXbarClockFrequency(int freq);

int kocGetArmMul();
int kocGetArmDiv();

#endif