#ifndef OS2SYS_RAID0_1
#define OS2SYS_RAID0_1
#include "os2sys.h"

uint64 sys_init_raid_0_1(void);

uint64 sys_read_raid_0_1(int,uint64);

uint64 sys_write_raid_0_1(int,uint64);

uint64 sys_disk_fail_raid_0_1(int);

uint64 sys_disk_repaired_raid_0_1(int);

uint64 sys_info_raid_0_1(uint *, uint *, uint *);

uint64 sys_destroy_raid_0_1(void);

#endif