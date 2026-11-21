#include "device.h"
#include "drivers.h"
#include "../kernel/kernel.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define RESETS_RESET    REG32(0x4000c000u)
#define RESETS_DONE     REG32(0x4000c008u)
#define SIO_GPIO_IN     REG32(0xd0000004u)
#define SIO_GPIO_SET    REG32(0xd0000014u)
#define SIO_GPIO_CLR    REG32(0xd0000018u)
#define SIO_GPIO_XOR    REG32(0xd000001cu)
#define SIO_GPIO_OE_SET REG32(0xd0000024u)
#define SIO_GPIO_OE_CLR REG32(0xd0000028u)
#define IO_BANK0_BASE   0x40014000u

static int gpio_init_fn(const struct device *dev) {
    (void)dev;
    RESETS_RESET &= ~(1u << 5);
    while (!(RESETS_DONE & (1u << 5)));
    k_log("GPIO_0", "30-pin SIO surucusu hazir");
    return 0;
}
static struct device dev_gpio = { .name = "GPIO_0", .init = gpio_init_fn };
void gpio_register_driver(void) { device_register(&dev_gpio); }

void drv_gpio_dir(uint32_t pin, bool out) {
    if (pin > 29) return;
    REG32(IO_BANK0_BASE + (pin * 8u) + 4u) = 5u;
    if (out) SIO_GPIO_OE_SET = (1u << pin); else SIO_GPIO_OE_CLR = (1u << pin);
}
void drv_gpio_write(uint32_t pin, int v) {
    if (pin > 29) return;
    drv_gpio_dir(pin, true);
    if (v) SIO_GPIO_SET = (1u << pin); else SIO_GPIO_CLR = (1u << pin);
}
void drv_gpio_toggle(uint32_t pin) { if (pin <= 29) { drv_gpio_dir(pin, true); SIO_GPIO_XOR = (1u << pin); } }
int drv_gpio_read(uint32_t pin) { return (pin <= 29 && (SIO_GPIO_IN & (1u << pin))) ? 1 : 0; }
