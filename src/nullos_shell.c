#include "usbcdc.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// RP2040 Donanım Adresleri
#define TIMERAWL        REG32(0x40054028u)
#define SIO_GPIO_IN     REG32(0xd0000004u)
#define SIO_GPIO_SET    REG32(0xd0000014u)
#define SIO_GPIO_CLR    REG32(0xd0000018u)
#define SIO_GPIO_XOR    REG32(0xd000001cu)
#define SIO_GPIO_OE_SET REG32(0xd0000024u)
#define SIO_GPIO_OE_CLR REG32(0xd0000028u)
#define IO_BANK0_BASE   0x40014000u
#define RESETS_RESET    REG32(0x4000c000u)
#define RESETS_DONE     REG32(0x4000c008u)
#define WATCHDOG_CTRL   REG32(0x40058000u)

// --- Temel Standart Kütüphane Yardımcıları (Sıfır Harici Bağımlılık) ---
static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i] || !b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

static void k_strcpy(char *dst, const char *src, size_t max) {
    size_t i = 0;
    while (i + 1 < max && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void k_strcat(char *dst, const char *src, size_t max) {
    size_t d = k_strlen(dst), i = 0;
    while (d + i + 1 < max && src[i]) { dst[d + i] = src[i]; i++; }
    dst[d + i] = '\0';
}

static int k_atoi(const char *s) {
    int sign = 1, val = 0;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
    return val * sign;
}

static uint32_t k_htoi(const char *s) {
    uint32_t val = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9') val = (val << 4) | (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') val = (val << 4) | (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val = (val << 4) | (uint32_t)(c - 'A' + 10);
        else break;
    }
    return val;
}

// --- Seri Çıktı Fonksiyonları ---
static void k_putc(char c) {
    while (!usbcdc_putchar(c));
}

static void k_puts(const char *s) {
    while (*s) {
        if (*s == '\n') k_putc('\r');
        k_putc(*s++);
    }
}

static void k_put_uint(uint32_t v) {
    char buf[12];
    int i = 0;
    if (v == 0) { k_putc('0'); return; }
    while (v > 0) { buf[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i > 0) k_putc(buf[--i]);
}

static void k_put_int(int v) {
    if (v < 0) { k_putc('-'); k_put_uint((uint32_t)(-v)); }
    else k_put_uint((uint32_t)v);
}

static void k_put_hex(uint32_t v, int digits) {
    const char *hex = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; i--) {
        k_putc(hex[(v >> (i * 4)) & 0xfu]);
    }
}

// --- Ortam Değişkenleri & Komut Geçmişi ---
#define MAX_ENV 16
typedef struct { char key[24]; char val[64]; bool used; } env_var_t;
static env_var_t env_vars[MAX_ENV];

#define MAX_HIST 10
static char history[MAX_HIST][128];
static int hist_count = 0;

static void env_set(const char *k, const char *v) {
    for (int i = 0; i < MAX_ENV; i++) {
        if (env_vars[i].used && k_strcmp(env_vars[i].key, k) == 0) {
            k_strcpy(env_vars[i].val, v, 64);
            return;
        }
    }
    for (int i = 0; i < MAX_ENV; i++) {
        if (!env_vars[i].used) {
            env_vars[i].used = true;
            k_strcpy(env_vars[i].key, k, 24);
            k_strcpy(env_vars[i].val, v, 64);
            return;
        }
    }
}

static const char *env_get(const char *k) {
    for (int i = 0; i < MAX_ENV; i++) {
        if (env_vars[i].used && k_strcmp(env_vars[i].key, k) == 0) return env_vars[i].val;
    }
    return "";
}

// --- Sanal Dosya Sistemi (RAMFS) ---
#define MAX_FILES 24
typedef struct {
    char path[64];
    char content[256];
    bool is_dir;
    bool used;
} ramfs_node_t;

static ramfs_node_t ramfs[MAX_FILES];
static char cwd[64] = "/";

static void fs_resolve(const char *arg, char *out) {
    if (!arg || !arg[0] || k_strcmp(arg, ".") == 0) {
        k_strcpy(out, cwd, 64);
        return;
    }
    if (k_strcmp(arg, "..") == 0) {
        k_strcpy(out, cwd, 64);
        size_t len = k_strlen(out);
        if (len > 1) {
            while (len > 1 && out[len - 1] != '/') len--;
            if (len > 1) len--;
            out[len] = '\0';
        }
        return;
    }
    if (arg[0] == '/') {
        k_strcpy(out, arg, 64);
    } else {
        k_strcpy(out, cwd, 64);
        if (k_strcmp(cwd, "/") != 0) k_strcat(out, "/", 64);
        k_strcat(out, arg, 64);
    }
}

static ramfs_node_t *fs_find(const char *full_path) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (ramfs[i].used && k_strcmp(ramfs[i].path, full_path) == 0) return &ramfs[i];
    }
    return NULL;
}

static ramfs_node_t *fs_create(const char *full_path, bool is_dir, const char *data) {
    ramfs_node_t *existing = fs_find(full_path);
    if (existing) {
        if (!is_dir && data) k_strcpy(existing->content, data, 256);
        return existing;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!ramfs[i].used) {
            ramfs[i].used = true;
            ramfs[i].is_dir = is_dir;
            k_strcpy(ramfs[i].path, full_path, 64);
            k_strcpy(ramfs[i].content, data ? data : "", 256);
            return &ramfs[i];
        }
    }
    return NULL;
}

static void fs_init(void) {
    fs_create("/", true, NULL);
    fs_create("/proc", true, NULL);
    fs_create("/dev", true, NULL);
    fs_create("/etc", true, NULL);
    fs_create("/home", true, NULL);
    fs_create("/etc/hostname", false, "nullos-rp2040\n");
    fs_create("/etc/motd", false, "NullOS POSIX-like Bare-Metal Kernel v0.2\nType 'help' for full command list.\n");
    fs_create("/home/readme.txt", false, "Welcome to NullOS! Use 'write', 'cat', 'gpio', 'peek', 'poke'.\n");
    fs_create("/dev/ttyACM0", false, "[USB CDC Serial Character Device]\n");
    fs_create("/dev/gpio", false, "[RP2040 SIO 30-Pin GPIO Controller]\n");
}

// --- Komut İşleyicileri ---
static void cmd_help(void) {
    k_puts("NullOS POSIX Shell v0.2 - Komut Listesi:\n");
    k_puts("  [Dosya Sistemi (RAMFS)]\n");
    k_puts("    ls [dizin]             - Dizin icerigini listeler\n");
    k_puts("    cd <dizin>             - Calisma dizinini degistirir (/, /proc, /etc, /home)\n");
    k_puts("    pwd                    - Mevcut dizini gosterir\n");
    k_puts("    mkdir <dizin>          - Yeni sanal dizin olusturur\n");
    k_puts("    touch <dosya>          - Yeni bos dosya olusturur\n");
    k_puts("    cat <dosya>            - Dosya icerigini okur (/proc/cpuinfo, /proc/meminfo vb.)\n");
    k_puts("    write <dosya> <metin>  - Dosyaya metin yazar\n");
    k_puts("    rm <dosya>             - Dosyayi siler\n");
    k_puts("  [Sistem & Kabuk]\n");
    k_puts("    uname [-a]             - Cekirdek ve islemci mimari bilgisi\n");
    k_puts("    uptime                 - Sistemin acik kalma suresi\n");
    k_puts("    free                   - SRAM (264 KB) ve Flash (2 MB) bellek durumu\n");
    k_puts("    ps                     - Cekirdek gorev tablosu\n");
    k_puts("    dmesg                  - Cekirdek donanim acilis gunlugu\n");
    k_puts("    env                    - Ortam degiskenlerini listeler\n");
    k_puts("    export <K>=<V>         - Yeni ortam degiskeni tanimlar\n");
    k_puts("    echo <metin|$VAR>      - Ekrana metin veya degisken basar\n");
    k_puts("    calc <s1> <op> <s2>    - Hesap makinesi (+, -, *, /)\n");
    k_puts("    sleep <ms>             - Milisaniye cinsinden donanim beklemesi\n");
    k_puts("    history                - Son komut gecmisini gosterir\n");
    k_puts("    clear                  - Terminali temizler\n");
    k_puts("    reboot                 - Islemciyi yeniden baslatir\n");
    k_puts("  [RP2040 Donanim & Bellek]\n");
    k_puts("    gpio <pin> <in|out|on|off|toggle|read> - Canli GPIO kontrolu\n");
    k_puts("    peek <hex_addr>        - 32-bit bellek/register degerini okur\n");
    k_puts("    poke <hex_addr> <val>  - 32-bit bellek/register adresine yazar\n");
    k_puts("    hexdump <hex_addr> <n> - Bellek bolgesini Hex + ASCII tablo olarak doker\n");
}

static void cmd_ls(const char *arg) {
    char target[64];
    fs_resolve(arg, target);
    ramfs_node_t *dir = fs_find(target);
    if (!dir || !dir->is_dir) {
        k_puts("ls: dizin bulunamadi: "); k_puts(target); k_putc('\n');
        return;
    }
    size_t tlen = k_strlen(target);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!ramfs[i].used || k_strcmp(ramfs[i].path, target) == 0) continue;
        if (k_strncmp(ramfs[i].path, target, tlen) == 0) {
            const char *rest = ramfs[i].path + (tlen == 1 ? 1 : tlen + 1);
            if (tlen > 1 && ramfs[i].path[tlen] != '/') continue;
            bool has_sub = false;
            for (size_t j = 0; rest[j]; j++) if (rest[j] == '/') has_sub = true;
            if (!has_sub && rest[0]) {
                k_puts(ramfs[i].is_dir ? "drwxr-xr-x  " : "-rw-r--r--  ");
                k_put_uint((uint32_t)k_strlen(ramfs[i].content));
                k_puts("\t");
                k_puts(rest);
                if (ramfs[i].is_dir) k_putc('/');
                k_putc('\n');
            }
        }
    }
}

static void cmd_cat(const char *arg) {
    if (!arg || !arg[0]) { k_puts("kullanim: cat <dosya>\n"); return; }
    char target[64];
    fs_resolve(arg, target);

    if (k_strcmp(target, "/proc/cpuinfo") == 0) {
        k_puts("processor\t: 0, 1\nmodel name\t: ARM Cortex-M0+ (RP2040 B2)\nBogoMIPS\t: 125.00\nFeatures\t: thumb armv6-m sio usbcdc\n");
        return;
    }
    if (k_strcmp(target, "/proc/meminfo") == 0) {
        k_puts("MemTotal:\t 264 kB\nMemFree:\t 248 kB\nFlashTotal:\t2048 kB\n");
        return;
    }
    if (k_strcmp(target, "/proc/uptime") == 0) {
        uint32_t sec = TIMERAWL / 1000000u;
        k_put_uint(sec); k_puts(".00 seconds\n");
        return;
    }

    ramfs_node_t *f = fs_find(target);
    if (!f) { k_puts("cat: dosya yok: "); k_puts(target); k_putc('\n'); return; }
    if (f->is_dir) { k_puts("cat: bir dizindir: "); k_puts(target); k_putc('\n'); return; }
    k_puts(f->content);
    if (k_strlen(f->content) > 0 && f->content[k_strlen(f->content) - 1] != '\n') k_putc('\n');
}

static void cmd_gpio(int argc, char **argv) {
    if (argc < 3) { k_puts("kullanim: gpio <0-29> <in|out|on|off|toggle|read>\n"); return; }
    uint32_t pin = (uint32_t)k_atoi(argv[1]);
    if (pin > 29) { k_puts("hata: pin 0-29 arasinda olmali\n"); return; }
    uint32_t mask = (1u << pin);
    volatile uint32_t *ctrl = (volatile uint32_t *)(IO_BANK0_BASE + (pin * 8u) + 4u);

    if (k_strcmp(argv[2], "out") == 0) {
        *ctrl = 5u; // SIO fonksiyonu
        SIO_GPIO_OE_SET = mask;
        k_puts("GPIO "); k_put_uint(pin); k_puts(" -> OUTPUT\n");
    } else if (k_strcmp(argv[2], "in") == 0) {
        *ctrl = 5u;
        SIO_GPIO_OE_CLR = mask;
        k_puts("GPIO "); k_put_uint(pin); k_puts(" -> INPUT\n");
    } else if (k_strcmp(argv[2], "on") == 0) {
        *ctrl = 5u;
        SIO_GPIO_OE_SET = mask;
        SIO_GPIO_SET = mask;
        k_puts("GPIO "); k_put_uint(pin); k_puts(" -> HIGH (1)\n");
    } else if (k_strcmp(argv[2], "off") == 0) {
        *ctrl = 5u;
        SIO_GPIO_OE_SET = mask;
        SIO_GPIO_CLR = mask;
        k_puts("GPIO "); k_put_uint(pin); k_puts(" -> LOW (0)\n");
    } else if (k_strcmp(argv[2], "toggle") == 0) {
        *ctrl = 5u;
        SIO_GPIO_OE_SET = mask;
        SIO_GPIO_XOR = mask;
        k_puts("GPIO "); k_put_uint(pin); k_puts(" -> TOGGLED\n");
    } else if (k_strcmp(argv[2], "read") == 0) {
        k_puts("GPIO "); k_put_uint(pin); k_puts(" = ");
        k_put_uint((SIO_GPIO_IN & mask) ? 1u : 0u);
        k_putc('\n');
    } else {
        k_puts("bilinmeyen alt komut. Kullanin: in, out, on, off, toggle, read\n");
    }
}

static void cmd_hexdump(uint32_t addr, uint32_t len) {
    if (len > 256) len = 256;
    for (uint32_t off = 0; off < len; off += 16) {
        k_puts("0x"); k_put_hex(addr + off, 8); k_puts(": ");
        for (uint32_t i = 0; i < 16; i++) {
            if (off + i < len) {
                uint8_t b = *(volatile uint8_t *)(addr + off + i);
                k_put_hex(b, 2); k_putc(' ');
            } else {
                k_puts("   ");
            }
        }
        k_puts(" |");
        for (uint32_t i = 0; i < 16 && (off + i) < len; i++) {
            uint8_t b = *(volatile uint8_t *)(addr + off + i);
            k_putc((b >= 32 && b <= 126) ? (char)b : '.');
        }
        k_puts("|\n");
    }
}

static void execute_line(char *line) {
    // Komut geçmişine ekle
    if (line[0]) {
        int idx = hist_count % MAX_HIST;
        k_strcpy(history[idx], line, 128);
        hist_count++;
    }

    char *argv[12];
    int argc = 0;
    char *p = line;
    while (*p && argc < 12) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (!*p) break;
        argv[argc++] = p;
        // 'write <dosya> <metin>' komutunda 3. argümanı boşluklarıyla tek parça al
        if (argc == 2 && k_strcmp(argv[0], "write") == 0) {
            while (*p && *p != ' ') p++;
            while (*p == ' ') *p++ = '\0';
            if (*p) argv[argc++] = p;
            break;
        }
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    if (argc == 0) return;

    if (k_strcmp(argv[0], "help") == 0) {
        cmd_help();
    } else if (k_strcmp(argv[0], "clear") == 0) {
        k_puts("\033[2J\033[H");
    } else if (k_strcmp(argv[0], "pwd") == 0) {
        k_puts(cwd); k_putc('\n');
    } else if (k_strcmp(argv[0], "ls") == 0 || k_strcmp(argv[0], "ll") == 0) {
        cmd_ls(argc > 1 ? argv[1] : ".");
    } else if (k_strcmp(argv[0], "cd") == 0) {
        char target[64];
        fs_resolve(argc > 1 ? argv[1] : "/", target);
        ramfs_node_t *d = fs_find(target);
        if (d && d->is_dir) k_strcpy(cwd, target, 64);
        else { k_puts("cd: gecersiz dizin: "); k_puts(target); k_putc('\n'); }
    } else if (k_strcmp(argv[0], "mkdir") == 0 && argc > 1) {
        char target[64];
        fs_resolve(argv[1], target);
        fs_create(target, true, NULL);
    } else if (k_strcmp(argv[0], "touch") == 0 && argc > 1) {
        char target[64];
        fs_resolve(argv[1], target);
        fs_create(target, false, "");
    } else if (k_strcmp(argv[0], "write") == 0 && argc > 2) {
        char target[64];
        fs_resolve(argv[1], target);
        fs_create(target, false, argv[2]);
    } else if (k_strcmp(argv[0], "cat") == 0) {
        cmd_cat(argc > 1 ? argv[1] : NULL);
    } else if (k_strcmp(argv[0], "rm") == 0 && argc > 1) {
        char target[64];
        fs_resolve(argv[1], target);
        ramfs_node_t *f = fs_find(target);
        if (f && k_strcmp(target, "/") != 0) f->used = false;
        else k_puts("rm: dosya silinemedi\n");
    } else if (k_strcmp(argv[0], "uname") == 0) {
        if (argc > 1 && k_strcmp(argv[1], "-a") == 0)
            k_puts("NullOS nullos-rp2040 0.2.0-baremetal #1 SMP armv6-m Cortex-M0+ GNU/NullOS\n");
        else
            k_puts("NullOS\n");
    } else if (k_strcmp(argv[0], "uptime") == 0) {
        uint32_t s = TIMERAWL / 1000000u;
        uint32_t m = s / 60u;
        uint32_t h = m / 60u;
        k_puts("up "); k_put_uint(h); k_puts("h "); k_put_uint(m % 60u); k_puts("m "); k_put_uint(s % 60u); k_puts("s\n");
    } else if (k_strcmp(argv[0], "free") == 0) {
        k_puts("              total        used        free\n");
        k_puts("SRAM:        264 KB       16 KB      248 KB\n");
        k_puts("Flash:      2048 KB       24 KB     2024 KB\n");
    } else if (k_strcmp(argv[0], "ps") == 0) {
        k_puts("  PID TTY          TIME CMD\n");
        k_puts("    1 ttyACM0  00:00:00 init\n");
        k_puts("    2 ttyACM0  00:00:00 usbcdc_poll\n");
        k_puts("    3 ttyACM0  00:00:00 nsh (NullOS Shell)\n");
    } else if (k_strcmp(argv[0], "dmesg") == 0) {
        k_puts("[    0.000] Booting NullOS v0.2 on RP2040 Dual Cortex-M0+\n");
        k_puts("[    0.002] XOSC 12MHz & PLL_SYS 125MHz locked\n");
        k_puts("[    0.005] USB CDC Serial Console (/dev/ttyACM0) ready\n");
        k_puts("[    0.006] RAMFS mounted on /\n");
    } else if (k_strcmp(argv[0], "env") == 0) {
        for (int i = 0; i < MAX_ENV; i++) {
            if (env_vars[i].used) {
                k_puts(env_vars[i].key); k_putc('='); k_puts(env_vars[i].val); k_putc('\n');
            }
        }
    } else if (k_strcmp(argv[0], "export") == 0 && argc > 1) {
        char *eq = argv[1];
        while (*eq && *eq != '=') eq++;
        if (*eq == '=') { *eq = '\0'; env_set(argv[1], eq + 1); }
    } else if (k_strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '$') k_puts(env_get(argv[i] + 1));
            else k_puts(argv[i]);
            if (i + 1 < argc) k_putc(' ');
        }
        k_putc('\n');
    } else if (k_strcmp(argv[0], "calc") == 0 && argc >= 4) {
        int a = k_atoi(argv[1]), b = k_atoi(argv[3]);
        if (argv[2][0] == '+') k_put_int(a + b);
        else if (argv[2][0] == '-') k_put_int(a - b);
        else if (argv[2][0] == '*') k_put_int(a * b);
        else if (argv[2][0] == '/' && b != 0) k_put_int(a / b);
        else k_puts("Hata");
        k_putc('\n');
    } else if (k_strcmp(argv[0], "sleep") == 0 && argc > 1) {
        uint32_t us = (uint32_t)k_atoi(argv[1]) * 1000u;
        uint32_t start = TIMERAWL;
        while ((TIMERAWL - start) < us);
    } else if (k_strcmp(argv[0], "history") == 0) {
        int start = hist_count > MAX_HIST ? hist_count - MAX_HIST : 0;
        for (int i = start; i < hist_count; i++) {
            k_puts("  "); k_put_uint((uint32_t)(i + 1)); k_puts("  ");
            k_puts(history[i % MAX_HIST]); k_putc('\n');
        }
    } else if (k_strcmp(argv[0], "gpio") == 0) {
        cmd_gpio(argc, argv);
    } else if (k_strcmp(argv[0], "peek") == 0 && argc > 1) {
        uint32_t addr = k_htoi(argv[1]);
        k_puts("0x"); k_put_hex(addr, 8); k_puts(" = 0x"); k_put_hex(REG32(addr), 8); k_putc('\n');
    } else if (k_strcmp(argv[0], "poke") == 0 && argc > 2) {
        uint32_t addr = k_htoi(argv[1]), val = k_htoi(argv[2]);
        REG32(addr) = val;
        k_puts("0x"); k_put_hex(addr, 8); k_puts(" <- 0x"); k_put_hex(val, 8); k_putc('\n');
    } else if (k_strcmp(argv[0], "hexdump") == 0 && argc > 1) {
        uint32_t addr = k_htoi(argv[1]);
        uint32_t len = argc > 2 ? (uint32_t)k_atoi(argv[2]) : 64u;
        cmd_hexdump(addr, len);
    } else if (k_strcmp(argv[0], "reboot") == 0) {
        k_puts("Sistem yeniden baslatiliyor...\n");
        WATCHDOG_CTRL = (1u << 31); // Watchdog trigger
        while (1);
    } else {
        k_puts("nsh: komut bulunamadi: "); k_puts(argv[0]); k_puts(" ('help' yazin)\n");
    }
}

static void print_prompt(void) {
    k_puts("\033[1;32mroot@nullos\033[0m:\033[1;34m");
    k_puts(cwd);
    k_puts("\033[0m# ");
}

int main(void) {
    // RESETS IO_BANK0 (Bit 5) kaldır
    RESETS_RESET &= ~(1u << 5);
    while (!(RESETS_DONE & (1u << 5)));

    fs_init();
    env_set("USER", "root");
    env_set("HOSTNAME", "nullos-rp2040");
    env_set("SHELL", "/bin/nsh");
    env_set("PATH", "/bin:/sbin:/usr/bin");

    configure_usbcdc();

    char line[128];
    int pos = 0;
    bool greeted = false;
    char c;

    while (1) {
        if (usbcdc_getchar(&c)) {
            if (!greeted) {
                greeted = true;
                k_puts("\r\n==============================================\r\n");
                k_puts("  NullOS v0.2 POSIX-like Bare-Metal Kernel\r\n");
                k_puts("  RP2040 ARM Cortex-M0+ | 'help' yazin\r\n");
                k_puts("==============================================\r\n");
                print_prompt();
                if (c == '\r' || c == '\n') continue;
            }

            if (c == '\r' || c == '\n') {
                k_puts("\r\n");
                line[pos] = '\0';
                execute_line(line);
                pos = 0;
                print_prompt();
            } else if (c == 0x08 || c == 0x7f) { // Backspace
                if (pos > 0) {
                    pos--;
                    k_puts("\b \b");
                }
            } else if (c == 0x03) { // Ctrl+C
                pos = 0;
                k_puts("^C\r\n");
                print_prompt();
            } else if (c == 0x0c) { // Ctrl+L
                k_puts("\033[2J\033[H");
                print_prompt();
                line[pos] = '\0';
                k_puts(line);
            } else if (c >= 32 && c <= 126 && pos < 126) {
                line[pos++] = c;
                k_putc(c);
            }
        }
    }
    return 0;
}
