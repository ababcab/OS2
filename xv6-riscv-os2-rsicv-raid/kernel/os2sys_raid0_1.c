#include "os2sys_raid0_1.h"


#//define USABLE_DISKS ((VIRTIO_RAID_DISK_END>>1)<<1)
#define USABLE_DISKS (VIRTIO_RAID_DISK_END^1)
#define DISKS_PER_MIRROR (VIRTIO_RAID_DISK_END>>1)
#define IN_DISK_RANGE(x) ( x>=VIRTIO_RAID_DISK_START && x<= USABLE_DISKS ? 1 : 0)
#define PARTNER_DISK(x) (x > DISKS_PER_MIRROR ? x - DISKS_PER_MIRROR : x + DISKS_PER_MIRROR)

static int broken_in_stripe[2]={0,0};

uint64 sys_init_raid_0_1()
{
    int mirror_offset = DISKS_PER_MIRROR;

    uchar* info = kalloc();
    info[0] = (uchar) '0' + RAID0_1;
    info[1] = '0'; //broken: 0 - not; 1- is

    for(int i=VIRTIO_RAID_DISK_START; i<=DISKS_PER_MIRROR;i++)
    {
        disk_info[i].broken=0;
        disk_info[i].stripe=0;
        write_block(i,0,info);

        disk_info[i + mirror_offset].broken=0;
        disk_info[i + mirror_offset].stripe=1;
        write_block(i+mirror_offset,0,info);
    }
    kfree(info);
    block_offset = 1;
    broken_in_stripe[0] =
     broken_in_stripe[1] = 0;
    return 0;
}

uint64 sys_read_raid_0_1(int block,uint64 bufferPA)
{
    block += DISKS_PER_MIRROR;
    int diskn = block % DISKS_PER_MIRROR + 1;
    block = block / DISKS_PER_MIRROR;
    
    int failedDisksInStripe = disk_info[diskn].stripe;

    if(failedDisksInStripe > 0)
    {
        diskn = PARTNER_DISK(diskn);
        failedDisksInStripe = disk_info[diskn].stripe;
        if(failedDisksInStripe > 0)
        {
            return BROKEN_DISK; // both stripes dont work
        }
    }

    int fail = read_block_with_check(diskn,block,(uchar*)bufferPA);

    return fail ? BROKEN_DISK : 0;
}

uint64 sys_write_raid_0_1(int block,uint64 bufferPA)
{
    block += DISKS_PER_MIRROR;
    int diskn = block % DISKS_PER_MIRROR + 1;
    block = block / DISKS_PER_MIRROR;
    
    int failedDisksInStripe = disk_info[diskn].stripe;

    int partner_disk = PARTNER_DISK(diskn);
    int failedDisksInPartnerStripe = disk_info[partner_disk].stripe;

    if(failedDisksInStripe > 0 && failedDisksInPartnerStripe > 0)
    {
        return BROKEN_DISK;
    }
    
    
    int fail=0;
    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(diskn)]);
    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(partner_disk)]);
    if(failedDisksInStripe > 0)
    {
        fail += write_block_with_check(diskn,block,(uchar*)bufferPA);
    }
    if(failedDisksInPartnerStripe > 0)
    {
        fail += write_block_with_check(partner_disk,block,(uchar*)bufferPA);
    }
    releasesleep(&os2_sleeplocks[OS2_DISKLOCK(partner_disk)]);
    releasesleep(&os2_sleeplocks[OS2_DISKLOCK(diskn)]);


    return fail == 2 ? BROKEN_DISK : 0;
}

uint64 sys_disk_fail_raid_0_1(int diskn)
{
    disk_info[diskn].broken=1;
    acquire(&os2_spinlocks[OS2_STRIPEFAIL_LOCK]);
    broken_in_stripe[disk_info[diskn].stripe]++;
    release(&os2_spinlocks[OS2_STRIPEFAIL_LOCK]);
    return 0;
}

uint64 sys_disk_repaired_raid_0_1(int diskn)
{
    if(IN_DISK_RANGE(diskn) == 0)
        return OUT_OF_DISK_RANGE;

    int partner_disk = PARTNER_DISK(diskn);

    if(disk_info[partner_disk].broken)
        return CANT_FIX_DISK;

    
    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(diskn)]);
    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(partner_disk)]);
    uchar* buffer = kalloc();

    int fail=0;
    for(int i=0; i< BLOCKS_IN_DISC && fail==0;i++)
    {
        if(read_block_with_check(partner_disk,i,buffer)) 
        {
            fail=1;break;
        }
        fail = write_block_with_check(diskn,i,buffer);
    }
    kfree(buffer);
    releasesleep(&os2_sleeplocks[OS2_DISKLOCK(partner_disk)]);
    releasesleep(&os2_sleeplocks[OS2_DISKLOCK(diskn)]);
    if(fail)
    {
        disk_info[diskn].broken=1;
        return CANT_FIX_DISK;
    }
    else
    {
        disk_info[diskn].broken=0;
        acquire(&os2_spinlocks[OS2_STRIPEFAIL_LOCK]);
        broken_in_stripe[disk_info[diskn].stripe]--;
        release(&os2_spinlocks[OS2_STRIPEFAIL_LOCK]);
        return 0;
    }
    return 0;
}

uint64 sys_info_raid_0_1(uint *blkn, uint *blks, uint *diskn)
{
    *blkn = (BLOCKS_IN_DISC - block_offset)* DISKS_PER_MIRROR;
    *blks = BSIZE;
    *diskn = DISKS_PER_MIRROR;
    return 0;
}

uint64 sys_destroy_raid_0_1()
{
    uchar* pageBuffer = (uchar*)kalloc();
    pageBuffer[0]=~0;

    for(int i=VIRTIO_RAID_DISK_START; i<=USABLE_DISKS;i++)
    {
        for(int j=0;j<=3;j++)
        {
            write_block(i,j,pageBuffer);
        }
        //write_block(i,0,pageBuffer);
        disk_info[i].broken=1;
    }

    block_offset = -1;
    kfree(pageBuffer);
    return 0;
}
                                                        // 2: 1     | 2
    // int startInclusive = diskn - (diskn % DISKS_PER_STRIPE) + 1; // 4: 1 2   | 3 4
    // int endInclusive   = startInclusive + DISKS_PER_STRIPE - 1;  // 6: 1 2 3 | 4 5 6
                                                                // 1..US/2    US/2+1..US
    //for(int i=startInclusive; i<=endInclusive;i++)