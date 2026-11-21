#ifndef NULLOS_DEVICE_H
#define NULLOS_DEVICE_H
#include <stdint.h>
#include <stdbool.h>

struct device {
    const char *name;
    int (*init)(const struct device *dev);
    const void *api;
    void *data;
    bool ready;
};

#define MAX_DEVICES 8
void device_register(struct device *dev);
const struct device *device_get_binding(const char *name);
int device_get_count(void);
const struct device *device_get_by_index(int idx);
#endif
