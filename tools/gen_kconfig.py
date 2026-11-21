#!/usr/bin/env python3
import os
os.makedirs("build", exist_ok=True)
cfg = {}
with open("nul.conf") as f:
    for line in f:
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, v = [x.strip() for x in line.split("=", 1)]
            cfg[k] = (v == "y")

with open("build/autoconf.h", "w") as h:
    h.write("#ifndef AUTOCONF_H\n#define AUTOCONF_H\n")
    for k, en in cfg.items():
        if en: h.write(f"#define {k} 1\n")
    h.write("#endif\n")

mod_map = {
    "CONFIG_GPIO": "gpio_rp2040.o",
    "CONFIG_ADC": "adc_rp2040.o",
    "CONFIG_PWM": "pwm_rp2040.o",
    "CONFIG_I2C": "i2c_rp2040.o",
    "CONFIG_SMP": "smp_core1.o",
    "CONFIG_RAMFS": "fs_ram.o",
}
objs = ["main.o", "kernel.o", "shell.o"] + [obj for k, obj in mod_map.items() if cfg.get(k)]
with open("build/kconfig.mk", "w") as mk:
    mk.write("NULLOS_OBJS = " + " ".join(objs) + "\n")
