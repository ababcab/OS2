//
// Created by os on 2/22/25.
//

#ifndef OS2SYS_RAID1_H
#define OS2SYS_RAID1_H
#include "os2sys.h"

uint64 sys_init_raid_1(void);

uint64 sys_read_raid_1(int,uint64);

uint64 sys_write_raid_1(int,uint64);

uint64 sys_disk_fail_raid_1(int);

uint64 sys_disk_repaired_raid_1(int);

uint64 sys_info_raid_1(uint *, uint *, uint *);

uint64 sys_destroy_raid_1(void);
#endif //OS2SYS_RAID1_H
