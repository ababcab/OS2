#ifndef OS2SYS_RAID4
#define OS2SYS_RAID4
#include "os2sys.h"

uint64 sys_init_raid_4(void);

uint64 sys_read_raid_4(int,uint64);

uint64 sys_write_raid_4(int,uint64);

uint64 sys_disk_fail_raid_4(int);

uint64 sys_disk_repaired_raid_4(int);

uint64 sys_info_raid_4(uint *, uint *, uint *);

uint64 sys_destroy_raid_4(void);
#endif