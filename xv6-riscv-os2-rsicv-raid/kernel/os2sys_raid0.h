//
// Created by os on 2/22/25.
//

#ifndef OS2SYS_RAID0_H
#define OS2SYS_RAID0_H
#include "os2sys.h"

uint64 sys_init_raid_0(void);

uint64 sys_read_raid_0(int,uint64);

uint64 sys_write_raid_0(int,uint64);

uint64 sys_disk_fail_raid_0(int);

uint64 sys_disk_repaired_raid_0(int);

uint64 sys_info_raid_0(uint *, uint *, uint *);

uint64 sys_destroy_raid_0(void);
#endif //OS2SYS_RAID0_H
