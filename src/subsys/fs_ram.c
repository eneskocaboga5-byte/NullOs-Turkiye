#include "subsys.h"
#include "../kernel/kernel.h"

#define MAX_FILES 24
typedef struct { char path[64]; char content[256]; bool is_dir; bool used; } node_t;
static node_t nodes[MAX_FILES];
static char cwd[64] = "/";

static void resolve(const char *arg, char *out) {
    if (!arg || !arg || k_strcmp(arg, ".") == 0) { k_strcpy(out, cwd, 64); return; }
    if (k_strcmp(arg, "..") == 0) {
        k_strcpy(out, cwd, 64);
        size_t len = k_strlen(out);
        if (len > 1) { while (len > 1 && out[len - 1] != '/') len--; if (len > 1) len--; out[len] = '\0'; }
        return;
    }
    if (arg == '/') k_strcpy(out, arg, 64);
    else { k_strcpy(out, cwd, 64); if (k_strcmp(cwd, "/") != 0) k_strcat(out, "/", 64); k_strcat(out, arg, 64); }
}

static node_t *find(const char *p) {
    for (int i = 0; i < MAX_FILES; i++) if (nodes[i].used && k_strcmp(nodes[i].path, p) == 0) return &nodes[i];
    return 0;
}

static void create(const char *p, bool is_dir, const char *data) {
    node_t *ex = find(p);
    if (ex) { if (!is_dir && data) k_strcpy(ex->content, data, 256); return; }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = true; nodes[i].is_dir = is_dir;
            k_strcpy(nodes[i].path, p, 64); k_strcpy(nodes[i].content, data ? data : "", 256);
            return;
        }
    }
}

void fs_init(void) {
    create("/", true, 0); create("/proc", true, 0); create("/dev", true, 0);
    create("/etc", true, 0); create("/home", true, 0);
    create("/etc/hostname", false, "nullos-zephyr-rp2040\n");
    create("/etc/motd", false, "NullOS Modular Kernel v0.3 (prj.conf enabled)\n");
    create("/home/readme.txt", false, "Use 'device list', 'adc temp', 'pwm 15 50', 'i2c scan', 'core1 heartbeat'.\n");
    k_log("RAMFS", "Sanal dosya sistemi / dizinine baglandi");
}

const char *fs_cwd(void) { return cwd; }
bool fs_cd(const char *arg) {
    char t[64]; resolve(arg, t);
    node_t *d = find(t);
    if (d && d->is_dir) { k_strcpy(cwd, t, 64); return true; }
    return false;
}

void fs_ls(const char *arg, void (*out)(const char *)) {
    char t[64]; resolve(arg, t);
    node_t *d = find(t);
    if (!d || !d->is_dir) { out("ls: dizin bulunamadi\n"); return; }
    size_t tlen = k_strlen(t);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!nodes[i].used || k_strcmp(nodes[i].path, t) == 0) continue;
        if (k_strncmp(nodes[i].path, t, tlen) == 0) {
            const char *rest = nodes[i].path + (tlen == 1 ? 1 : tlen + 1);
            if (tlen > 1 && nodes[i].path[tlen] != '/') continue;
            bool sub = false;
            for (size_t j = 0; rest[j]; j++) if (rest[j] == '/') sub = true;
            if (!sub && rest) { out(nodes[i].is_dir ? "drwxr-xr-x  " : "-rw-r--r--  "); out(rest); if (nodes[i].is_dir) out("/"); out("\n"); }
        }
    }
}

void fs_mkdir(const char *arg) { char t[64]; resolve(arg, t); create(t, true, 0); }
void fs_touch(const char *arg) { char t[64]; resolve(arg, t); create(t, false, ""); }
void fs_write(const char *arg, const char *data) { char t[64]; resolve(arg, t); create(t, false, data); }
void fs_rm(const char *arg, void (*out)(const char *)) {
    char t[64]; resolve(arg, t); node_t *f = find(t);
    if (f && k_strcmp(t, "/") != 0) f->used = false; else out("rm: dosya silinemedi\n");
}
void fs_cat(const char *arg, void (*out)(const char *)) {
    char t[64]; resolve(arg, t);
    if (k_strcmp(t, "/proc/cpuinfo") == 0) { out("processor\t: 0, 1 (Dual ARM Cortex-M0+ @ 125MHz)\narchitecture\t: armv6-m RP2040\n"); return; }
    node_t *f = find(t);
    if (!f || f->is_dir) { out("cat: dosya okunamadi\n"); return; }
    out(f->content);
    if (k_strlen(f->content) > 0 && f->content[k_strlen(f->content) - 1] != '\n') out("\n");
}
