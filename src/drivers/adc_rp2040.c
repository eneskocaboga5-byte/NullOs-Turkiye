#include "device.h"
#include "drivers.h"
#include "../kernel/kernel.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define RESETS_RESET    REG32(0x4000c000u)
#define RESETS_DONE     REG32(0x4000c008u)
#define CLK_ADC_CTRL    REG32(0x40008060u)
#define ADC_CS          REG32(0x4004c000u)
#define ADC_RESULT      REG32(0x4004c004u)

static int adc_init_fn(const struct device *dev) {
    (void)dev;
    CLK_ADC_CTRL = (1u << 11);
    RESETS_RESET &= ~(1u << 0);
    while (!(RESETS_DONE & (1u << 0)));
    ADC_CS = (1u << 0) | (1u << 1);
    while (!(ADC_CS & (1u << 8)));
    k_log("ADC_0", "12-bit ADC ve dahili sicaklik sensoru hazir");
    return 0;
}
static struct device dev_adc = { .name = "ADC_0", .init = adc_init_fn };
void adc_register_driver(void) { device_register(&dev_adc); }

int drv_adc_read_raw(uint32_t ch) {
    if (ch > 4) return 0;
    ADC_CS = (1u << 0) | (1u << 1) | (ch << 12);
    ADC_CS |= (1u << 2);
    while (!(ADC_CS & (1u << 8)));
    return (int)(ADC_RESULT & 0xfffu);
}
int drv_adc_read_temp_c10(void) {
    int mv = (drv_adc_read_raw(4) * 3300) / 4096;
    return 270 - (((mv - 706) * 1000) / 1721);
}
