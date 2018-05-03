#ifndef CPUUSAGE_H
#define CPUUSAGE_H

#include <rtthread.h>
#include <rthw.h>

static void cpu_usage_idle_hook(void);

void cpu_usage_get(rt_uint8_t *major, rt_uint8_t *minor);

void cpu_usage_init(void);

#endif




 









