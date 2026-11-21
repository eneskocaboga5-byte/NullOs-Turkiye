#include "autoconf.h"
#include "usbcdc.h"
#include "subsys.h"
#include "../kernel/kernel.h"
#include "../drivers/device.h"
#include "../drivers/drivers.h"

static void s_putc(char c) { while (!usbcdc_putchar(c)); }
static void s_puts(const char *s) { while (*s) { if (*s == '\n') s_putc('\r'); s_putc(*s++); } }
static void s_uint(uint32_t v) {
    char b[16]; int i = 0;
    if (!v) { s_putc('0'); return; }
    while (v) { *(b + i++) = (char)('0' + (v % 10)); v /= 10; }
    while (i) s_putc(*(b + --i));
}
static void s_hex(uint32_t v, int d) {
    const char *h = "0123456789abcdef";
    for (int i = d - 1; i >= 0; i--) s_putc(*(h + ((v >> (i * 4)) & 0xfu)));
}

#define MAX_HIST 10
static char hist[MAX_HIST][128];
static int h_cnt = 0;

static void prompt(void) {
    s_puts("\033[1;32mroot@nullos\033[0m:\033[1;34m");
#ifdef CONFIG_RAMFS
    s_puts(fs_cwd());
#else
    s_puts("/");
#endif
    s_puts("\033[0m# ");
}

static void exec_cmd(char *line) {
    if (*line) { k_strcpy(*(hist + (h_cnt % MAX_HIST)), line, 128); h_cnt++; }
    char *argv[12]; int argc = 0; char *p = line;
    while (*p && argc < 10) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (!*p) break;
        *(argv + argc++) = p;
        if (argc == 2 && k_strcmp(*argv, "write") == 0) {
            while (*p && *p != ' ') p++;
            while (*p == ' ') *p++ = '\0';
            if (*p) *(argv + argc++) = p;
            break;
        }
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    if (!argc) return;

    char *a0 = *(argv + 0);
    char *a1 = argc > 1 ? *(argv + 1) : "";
    char *a2 = argc > 2 ? *(argv + 2) : "";

    if (k_strcmp(a0, "help") == 0) {
        s_puts("NullOS Zephyr-Style Modular Shell (prj.conf aktif moduller):\n");
        s_puts("  device list               - Yuklu donanim suruculerini gosterir\n");
        s_puts("  uname / uptime / dmesg    - Cekirdek durumu ve acilis gunlugu\n");
        s_puts("  peek <hex> / poke <h> <v> - Dogrudan bellek/register erisimi\n");
        s_puts("  clear / reboot            - Ekrani temizler / yeniden baslatir\n");
#ifdef CONFIG_BOOTSEL_CMD
        s_puts("  bootsel                   - Dugmeye basmadan USB yukleme moduna gecer\n");
#endif
#ifdef CONFIG_GPIO
        s_puts("  gpio <pin> <on|off|toggle|read> - Canli GPIO kontrolu\n");
#endif
#ifdef CONFIG_ADC
        s_puts("  adc temp | adc read <0-3> - Cip sicakligi ve analog okuma\n");
#endif
#ifdef CONFIG_PWM
        s_puts("  pwm <pin> <0-100>         - Donanimsal PWM sinyal uretici\n");
#endif
#ifdef CONFIG_I2C
        s_puts("  i2c scan                  - GP4/GP5 I2C veri yolunu tarar\n");
#endif
#ifdef CONFIG_SMP
        s_puts("  core1 <idle|heartbeat|stress|stat> - 2. Cekirdek (Core 1) yonetimi\n");
#endif
#ifdef CONFIG_RAMFS
        s_puts("  ls, cd, pwd, mkdir, touch, write, cat, rm - Sanal dosya sistemi\n");
#endif
    } else if (k_strcmp(a0, "device") == 0 && argc > 1 && k_strcmp(a1, "list") == 0) {
        s_puts("Surucu Adi      Durum\n-------------------------\n");
        for (int i = 0; i < device_get_count(); i++) {
            const struct device *d = device_get_by_index(i);
            s_puts(d->name); s_puts("\t\t");
            s_puts(d->ready ? "\033[32mREADY\033[0m\n" : "\033[31mERROR\033[0m\n");
        }
    } else if (k_strcmp(a0, "uname") == 0) {
        s_puts("NullOS nullos-rp2040 0.3.0-modular armv6-m Zephyr-Arch\n");
    } else if (k_strcmp(a0, "uptime") == 0) {
        s_puts("up "); s_uint(k_uptime_get_32() / 1000u); s_puts(" seconds\n");
    } else if (k_strcmp(a0, "dmesg") == 0) {
        s_puts(k_log_buffer());
    } else if (k_strcmp(a0, "clear") == 0) {
        s_puts("\033[2J\033[H");
    } else if (k_strcmp(a0, "reboot") == 0) {
        s_puts("Yeniden baslatiliyor...\n"); k_reboot();
#ifdef CONFIG_BOOTSEL_CMD
    } else if (k_strcmp(a0, "bootsel") == 0) {
        s_puts("BOOTSEL (RPI-RP2) moduna geciliyor...\n"); k_msleep(50); k_reboot_bootsel();
#endif
    } else if (k_strcmp(a0, "peek") == 0 && argc > 1) {
        uint32_t a = k_htoi(a1);
        s_puts("0x"); s_hex(a, 8); s_puts(" = 0x"); s_hex(*(volatile uint32_t *)a, 8); s_putc('\n');
    } else if (k_strcmp(a0, "poke") == 0 && argc > 2) {
        uint32_t a = k_htoi(a1), v = k_htoi(a2);
        *(volatile uint32_t *)a = v;
        s_puts("0x"); s_hex(a, 8); s_puts(" <- 0x"); s_hex(v, 8); s_putc('\n');
#ifdef CONFIG_GPIO
    } else if (k_strcmp(a0, "gpio") == 0 && argc >= 3) {
        uint32_t pin = (uint32_t)k_atoi(a1);
        if (k_strcmp(a2, "on") == 0) drv_gpio_write(pin, 1);
        else if (k_strcmp(a2, "off") == 0) drv_gpio_write(pin, 0);
        else if (k_strcmp(a2, "toggle") == 0) drv_gpio_toggle(pin);
        else if (k_strcmp(a2, "read") == 0) { s_uint((uint32_t)drv_gpio_read(pin)); s_putc('\n'); }
#endif
#ifdef CONFIG_ADC
    } else if (k_strcmp(a0, "adc") == 0 && argc > 1) {
        if (k_strcmp(a1, "temp") == 0) {
            int t = drv_adc_read_temp_c10();
            s_puts("RP2040 Cip Sicakligi: "); s_uint((uint32_t)(t / 10)); s_putc('.'); s_uint((uint32_t)(t % 10)); s_puts(" C\n");
        } else if (k_strcmp(a1, "read") == 0 && argc > 2) {
            s_puts("ADC = "); s_uint((uint32_t)drv_adc_read_raw((uint32_t)k_atoi(a2))); s_putc('\n');
        }
#endif
#ifdef CONFIG_PWM
    } else if (k_strcmp(a0, "pwm") == 0 && argc >= 3) {
        drv_pwm_set((uint32_t)k_atoi(a1), (uint32_t)k_atoi(a2));
        s_puts("PWM ayarlandi\n");
#endif
#ifdef CONFIG_I2C
    } else if (k_strcmp(a0, "i2c") == 0 && argc > 1 && k_strcmp(a1, "scan") == 0) {
        s_puts("I2C0 (GP4/GP5) taraniyor...\n");
        int found = 0;
        for (uint8_t a = 0x08; a <= 0x77; a++) {
            if (drv_i2c_probe(a)) { s_puts("  Cihaz bulundu: 0x"); s_hex(a, 2); s_putc('\n'); found++; }
        }
        if (!found) s_puts("  Bagli I2C cihazi bulunamadi.\n");
#endif
#ifdef CONFIG_SMP
    } else if (k_strcmp(a0, "core1") == 0 && argc > 1) {
        static bool smp_started = false;
        if (!smp_started) { smp_init(); smp_started = true; }
        if (k_strcmp(a1, "idle") == 0) smp_set_mode(0);
        else if (k_strcmp(a1, "heartbeat") == 0) smp_set_mode(1);
        else if (k_strcmp(a1, "stress") == 0) smp_set_mode(2);
        else if (k_strcmp(a1, "stat") == 0) {
            s_puts("Core 1 Mod: "); s_uint((uint32_t)smp_get_mode());
            s_puts(" | Dongu: "); s_uint(smp_get_cycles()); s_putc('\n');
        }
#endif
#ifdef CONFIG_RAMFS
    } else if (k_strcmp(a0, "pwd") == 0) { s_puts(fs_cwd()); s_putc('\n'); }
    else if (k_strcmp(a0, "ls") == 0) { fs_ls(argc > 1 ? a1 : ".", s_puts); }
    else if (k_strcmp(a0, "cd") == 0) { if (!fs_cd(argc > 1 ? a1 : "/")) s_puts("cd: dizin yok\n"); }
    else if (k_strcmp(a0, "mkdir") == 0 && argc > 1) { fs_mkdir(a1); }
    else if (k_strcmp(a0, "touch") == 0 && argc > 1) { fs_touch(a1); }
    else if (k_strcmp(a0, "write") == 0 && argc > 2) { fs_write(a1, a2); }
    else if (k_strcmp(a0, "rm") == 0 && argc > 1) { fs_rm(a1, s_puts); }
    else if (k_strcmp(a0, "cat") == 0 && argc > 1) { fs_cat(a1, s_puts); }
#endif
    else {
        s_puts("nsh: bilinmeyen komut: "); s_puts(a0); s_puts(" ('help' yazin)\n");
    }
}

void shell_run(void) {
    char line[128]; int pos = 0, h_idx = 0, esc = 0;
    bool greeted = false; char c;

    while (1) {
        if (!usbcdc_getchar(&c)) continue;
        if (!greeted) {
            greeted = true;
            s_puts("\r\n================================================\r\n");
            s_puts("  NullOS v0.3 - Zephyr-Style Modular Kernel\r\n");
            s_puts("  'help' veya 'device list' yazin\r\n");
            s_puts("================================================\r\n");
            prompt();
            if (c == '\r' || c == '\n') continue;
        }

        if (esc == 1) { esc = (c == '[') ? 2 : 0; continue; }
        if (esc == 2) {
            esc = 0;
            if (c == 'A' && h_cnt > 0) {
                while (pos > 0) { pos--; s_puts("\b \b"); }
                int idx = (h_cnt - 1 - h_idx);
                if (idx < 0) idx = 0;
                k_strcpy(line, *(hist + (idx % MAX_HIST)), 128);
                pos = (int)k_strlen(line);
                s_puts(line);
                if (h_idx + 1 < h_cnt && h_idx + 1 < MAX_HIST) h_idx++;
            }
            continue;
        }
        if (c == 0x1b) { esc = 1; continue; }

        if (c == '\r' || c == '\n') {
            s_puts("\r\n"); *(line + pos) = '\0';
            exec_cmd(line); pos = 0; h_idx = 0; prompt();
        } else if (c == 0x08 || c == 0x7f) {
            if (pos > 0) { pos--; s_puts("\b \b"); }
        } else if (c == 0x03) {
            pos = 0; s_puts("^C\r\n"); prompt();
        } else if (c >= 32 && c <= 126 && pos < 126) {
            *(line + pos++) = c; s_putc(c);
        }
    }
}
