//
// Created by os on 2/22/25.
//

#ifndef OS2SYS_H
#define OS2SYS_H
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"


typedef enum {
    NOT_INIT = 1,
    ALREADY_INIT,
    OUT_OF_DISK_RANGE,
    OUT_OF_BLOCK_RANGE,
    BROKEN_DISK,
    CANT_FIX_DISK
}RAID_ERROR;

enum RAID_TYPE {RAID0, RAID1, RAID0_1, RAID4, RAID5};
extern enum RAID_TYPE currentRAID;

typedef struct dInfo
{
    uint16 broken;
    uint8 stripe;
    uint8 parityBlock;
} dInfo;
extern int block_offset;
extern dInfo disk_info[VIRTIO_RAID_DISK_END + 1];
extern struct spinlock os2_spinlocks[OS2_SPINLOCK_COUNT];
extern struct sleeplock os2_sleeplocks[OS2_SLEEPLOCK_COUNT];


uint64 sys_init_raid(void);
uint64 sys_read_raid(void);
uint64 sys_write_raid(void);

uint64 sys_disk_fail_raid(void);
uint64 sys_disk_repaired_raid(void);
uint64 sys_info_raid(void);
uint64 sys_destroy_raid(void);


uint64 checkRAID_init( int wantInitializedRAID);
uint64 checkRAID_disc(int wantOperational, int discn);
uint64 cleardisks();
#endif //OS2SYS_H
