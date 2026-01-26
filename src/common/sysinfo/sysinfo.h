#ifndef ZOITECHAT_SYSINFO_H
#define ZOITECHAT_SYSINFO_H

#include <stdint.h>

int sysinfo_get_cpu_arch (void);
int sysinfo_get_build_arch (void);
char *sysinfo_get_cpu (void);
char *sysinfo_get_os (void);
void sysinfo_get_hdd_info (uint64_t *hdd_capacity_out, uint64_t *hdd_free_space_out);
char *sysinfo_get_gpu (void);

#endif
