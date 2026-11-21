#include "autoconf.h"
#include "usbcdc.h"
#include "kernel/kernel.h"
#include "subsys/subsys.h"

extern void gpio_register_driver(void);
extern void adc_register_driver(void);
extern void pwm_register_driver(void);
extern void i2c_register_driver(void);

int main(void) {
    // 1. Ilk is USB CDC seri portu baslat (boot garantisi)
    configure_usbcdc();

    k_log("KERNEL", "NullOS v0.3 baslatiliyor...");
#ifdef CONFIG_GPIO
    gpio_register_driver();
#endif
#ifdef CONFIG_ADC
    adc_register_driver();
#endif
#ifdef CONFIG_PWM
    pwm_register_driver();
#endif
#ifdef CONFIG_I2C
    i2c_register_driver();
#endif
#ifdef CONFIG_RAMFS
    fs_init();
#endif

    shell_run();
    return 0;
}
