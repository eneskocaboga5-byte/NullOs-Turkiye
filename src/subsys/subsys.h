#ifndef NULLOS_SUBSYS_H
#define NULLOS_SUBSYS_H
#include <stdint.h>
#include <stdbool.h>

void smp_init(void);
void smp_set_mode(int mode);
int  smp_get_mode(void);
uint32_t smp_get_cycles(void);

void fs_init(void);
const char *fs_cwd(void);
bool fs_cd(const char *arg);
void fs_ls(const char *arg, void (*out)(const char *));
void fs_mkdir(const char *arg);
void fs_touch(const char *arg);
void fs_write(const char *arg, const char *data);
void fs_rm(const char *arg, void (*out)(const char *));
void fs_cat(const char *arg, void (*out)(const char *));

void shell_run(void);
#endif
