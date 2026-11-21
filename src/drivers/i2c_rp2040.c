#include "device.h"
#include "drivers.h"
#include "../kernel/kernel.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define RESETS_RESET    REG32(0x4000c000u)
#define RESETS_DONE     REG32(0x4000c008u)
#define IO_BANK0_BASE   0x40014000u
#define PADS_BANK0_BASE 0x4001c000u
#define I2C0_BASE       0x40044000u

static int i2c_init_fn(const struct device *dev) {
    (void)dev;
    RESETS_RESET &= ~(1u << 3);
    while (!(RESETS_DONE & (1u << 3)));

    REG32(IO_BANK0_BASE + (4u * 8u) + 4u) = 3u;
    REG32(IO_BANK0_BASE + (5u * 8u) + 4u) = 3u;
    REG32(PADS_BANK0_BASE + 0x14u) = (1u << 6) | (1u << 3) | (1u << 1);
    REG32(PADS_BANK0_BASE + 0x18u) = (1u << 6) | (1u << 3) | (1u << 1);

    REG32(I2C0_BASE + 0x6cu) = 0u;
    REG32(I2C0_BASE + 0x00u) = (1u << 0) | (2u << 1) | (1u << 5) | (1u << 6);
    REG32(I2C0_BASE + 0x1cu) = 150u;
    REG32(I2C0_BASE + 0x20u) = 150u;
    REG32(I2C0_BASE + 0x6cu) = 1u;

    k_log("I2C_0", "I2C0 veri yolu (GP4=SDA, GP5=SCL) hazir");
    return 0;
}
static struct device dev_i2c = { .name = "I2C_0", .init = i2c_init_fn };
void i2c_register_driver(void) { device_register(&dev_i2c); }

bool drv_i2c_probe(uint8_t addr) {
    REG32(I2C0_BASE + 0x6cu) = 0u;
    REG32(I2C0_BASE + 0x04u) = addr;
    REG32(I2C0_BASE + 0x6cu) = 1u;
    (void)REG32(I2C0_BASE + 0x54u);
    REG32(I2C0_BASE + 0x10u) = (1u << 8) | (1u << 9);
    k_usleep(300);
    uint32_t abrt = REG32(I2C0_BASE + 0x80u);
    (void)REG32(I2C0_BASE + 0x40u);
    if (!(abrt & 0x1fu)) {
        while (REG32(I2C0_BASE + 0x78u)) (void)REG32(I2C0_BASE + 0x10u);
        return true;
    }
    return false;
}
