#include "autoconf.h"
#include "kernel.h"
#include "../drivers/device.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define TIMERAWL        REG32(0x40054028u)
#define WATCHDOG_CTRL   REG32(0x40058000u)

static struct device *dev_list[MAX_DEVICES];
static int dev_count = 0;

void device_register(struct device *dev) {
    if (dev && dev_count < MAX_DEVICES) {
        dev_list[dev_count++] = dev;
        dev->ready = dev->init ? (dev->init(dev) == 0) : true;
    }
}

const struct device *device_get_binding(const char *name) {
    for (int i = 0; i < dev_count; i++) {
        if (dev_list[i]->ready && k_strcmp(dev_list[i]->name, name) == 0) return dev_list[i];
    }
    return 0;
}

int device_get_count(void) { return dev_count; }
const struct device *device_get_by_index(int idx) { return (idx >= 0 && idx < dev_count) ? dev_list[idx] : 0; }

uint32_t k_uptime_get_32(void) { return TIMERAWL / 1000u; }
uint64_t k_uptime_get(void) { return (uint64_t)(TIMERAWL / 1000u); }
void k_usleep(uint32_t us) { uint32_t s = TIMERAWL; while ((TIMERAWL - s) < us); }
void k_msleep(uint32_t ms) { k_usleep(ms * 1000u); }
void k_reboot(void) { WATCHDOG_CTRL = (1u << 31); while (1); }

typedef void *(*rom_lookup_fn)(uint16_t *t, uint32_t c);
typedef void (*rom_boot_fn)(uint32_t m1, uint32_t m2);
void k_reboot_bootsel(void) {
    uint16_t *ft = (uint16_t *)(uintptr_t)(*(uint16_t *)0x00000014u);
    rom_lookup_fn fn = (rom_lookup_fn)(uintptr_t)(*(uint16_t *)0x00000018u);
    rom_boot_fn rb = (rom_boot_fn)fn(ft, ('B' << 8) | 'U');
    if (rb) rb(0, 0);
    while (1);
}

static char log_buf[1024];
static size_t log_pos = 0;
void k_log(const char *tag, const char *msg) {
    if (log_pos > 900) return;
    log_buf[log_pos++] = '[';
    while (*tag) log_buf[log_pos++] = *tag++;
    log_buf[log_pos++] = ']'; log_buf[log_pos++] = ' ';
    while (*msg) log_buf[log_pos++] = *msg++;
    log_buf[log_pos++] = '\n'; log_buf[log_pos] = '\0';
}
const char *k_log_buffer(void) { return log_buf; }

size_t k_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int k_strcmp(const char *a, const char *b) { while (*a && (*a == *b)) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
int k_strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) if (a[i] != b[i] || !a[i] || !b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
    return 0;
}
void k_strcpy(char *d, const char *s, size_t m) { size_t i = 0; while (i + 1 < m && s[i]) { d[i] = s[i]; i++; } d[i] = '\0'; }
void k_strcat(char *d, const char *s, size_t m) { size_t l = k_strlen(d), i = 0; while (l + i + 1 < m && s[i]) { d[l + i] = s[i]; i++; } d[l + i] = '\0'; }
int k_atoi(const char *s) { int sg = 1, v = 0; if (*s == '-') { sg = -1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return v * sg; }
uint32_t k_htoi(const char *s) {
    uint32_t v = 0; if (s == '0' && (s == 'x' || s == 'X')) s += 2;
    while (*s) { char c = *s++; if (c >= '0' && c <= '9') v = (v << 4) | (c - '0'); else if (c >= 'a' && c <= 'f') v = (v << 4) | (c - 'a' + 10); else if (c >= 'A' && c <= 'F') v = (v << 4) | (c - 'A' + 10); else break; }
    return v;
}
