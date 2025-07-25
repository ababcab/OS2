#ifndef OS2SYS_RAID5
#define OS2SYS_RAID5
#include "os2sys.h"

uint64 sys_init_raid_5(void);

uint64 sys_read_raid_5(int,uint64);

uint64 sys_write_raid_5(int,uint64);

uint64 sys_disk_fail_raid_5(int);

uint64 sys_disk_repaired_raid_5(int);

uint64 sys_info_raid_5(uint *, uint *, uint *);

uint64 sys_destroy_raid_5(void);
#endif