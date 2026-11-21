#ifndef NULLOS_KERNEL_H
#define NULLOS_KERNEL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

uint64_t k_uptime_get(void);
uint32_t k_uptime_get_32(void);
void k_msleep(uint32_t ms);
void k_usleep(uint32_t us);
void k_reboot(void);
void k_reboot_bootsel(void);
void k_log(const char *tag, const char *msg);
const char *k_log_buffer(void);

size_t k_strlen(const char *s);
int k_strcmp(const char *a, const char *b);
int k_strncmp(const char *a, const char *b, size_t n);
void k_strcpy(char *dst, const char *src, size_t max);
void k_strcat(char *dst, const char *src, size_t max);
int k_atoi(const char *s);
uint32_t k_htoi(const char *s);
#endif
