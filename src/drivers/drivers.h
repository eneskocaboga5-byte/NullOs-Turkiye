#ifndef NULLOS_DRIVERS_H
#define NULLOS_DRIVERS_H
#include <stdint.h>
#include <stdbool.h>

void drv_gpio_dir(uint32_t pin, bool output);
void drv_gpio_write(uint32_t pin, int val);
void drv_gpio_toggle(uint32_t pin);
int  drv_gpio_read(uint32_t pin);
int  drv_adc_read_raw(uint32_t ch);
int  drv_adc_read_temp_c10(void);
void drv_pwm_set(uint32_t pin, uint32_t duty_percent);
bool drv_i2c_probe(uint8_t addr);
#endif
