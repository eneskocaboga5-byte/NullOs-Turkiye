#include "device.h"
#include "drivers.h"
#include "../kernel/kernel.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define RESETS_RESET    REG32(0x4000c000u)
#define RESETS_DONE     REG32(0x4000c008u)
#define IO_BANK0_BASE   0x40014000u
#define PWM_BASE        0x40050000u

static int pwm_init_fn(const struct device *dev) {
    (void)dev;
    RESETS_RESET &= ~(1u << 14);
    while (!(RESETS_DONE & (1u << 14)));
    k_log("PWM_0", "8-dilimli donanimsal PWM surucusu hazir");
    return 0;
}
static struct device dev_pwm = { .name = "PWM_0", .init = pwm_init_fn };
void pwm_register_driver(void) { device_register(&dev_pwm); }

void drv_pwm_set(uint32_t pin, uint32_t duty_percent) {
    if (pin > 29) return;
    if (duty_percent > 100u) duty_percent = 100u;
    uint32_t slice = (pin >> 1) & 7u;
    uint32_t chan = pin & 1u;
    uint32_t slice_off = slice * 0x14u;

    REG32(IO_BANK0_BASE + (pin * 8u) + 4u) = 4u;
    REG32(PWM_BASE + slice_off + 0x04u) = (125u << 4);
    REG32(PWM_BASE + slice_off + 0x10u) = 99u;

    uint32_t cc = REG32(PWM_BASE + slice_off + 0x0cu);
    if (chan == 0u) cc = (cc & 0xffff0000u) | duty_percent;
    else cc = (cc & 0x0000ffffu) | (duty_percent << 16);
    REG32(PWM_BASE + slice_off + 0x0cu) = cc;
    REG32(PWM_BASE + slice_off + 0x00u) = 1u;
}
