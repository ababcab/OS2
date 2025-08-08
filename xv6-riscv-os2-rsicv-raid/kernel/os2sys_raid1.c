//
// Created by os on 2/22/25.
//

#include "os2sys_raid1.h"

#define PARTNER_DISK(x) (x % 2 ? x+1: x-1)
#define USABLE_DISKS ((VIRTIO_RAID_DISK_END>>1)<<1)
#define IN_DISK_RANGE(x) ( x>=VIRTIO_RAID_DISK_START && x<= USABLE_DISKS ? 1 : 0)


uint64 sys_init_raid_1(void)
{
    uchar* info=(uchar*)kalloc();
    
    info[0]=(uchar)('0' + RAID1);//koji je raid (gleda se po vrednosti enum-a)
    info[1]=(uchar)'0';//da li je polomljen disk: 0-nije, 1-jeste
    for(int i=VIRTIO_RAID_DISK_START; i<=USABLE_DISKS/2;i++)
    {
        write_block(i,0,info);
        write_block(2*i,0,info);
        disk_info[i].broken=0;
        disk_info[2*i].broken=0;
    }
    kfree(info);

    block_offset = 1;

    return 0;
}


uint64 sys_read_raid_1(int block,uint64 bufferPA)
{
    //svaki disk: 1 (block_offset) + usable_blocks (BLOCKS_IN_DISC - blockoffset)
    int userAvailableBlocks = BLOCKS_IN_DISC - block_offset;
    int diskn = VIRTIO_RAID_DISK_START + 2 * block / (userAvailableBlocks);
    block = block_offset + block % userAvailableBlocks;

    if(diskn > USABLE_DISKS)
        return OUT_OF_BLOCK_RANGE;

    int fail = read_block_with_check(diskn,block,(uchar*)bufferPA);
    if(fail)  // ne treba da cita dva puta ako je prvi put uspeo
        fail += read_block_with_check(PARTNER_DISK(diskn),block,(uchar*)bufferPA);
    
    return fail ? BROKEN_DISK : 0;
}


uint64 sys_write_raid_1(int block,uint64 bufferPA)
{
    //svaki disk: 1 (block_offset) + usable_blocks (BLOCKS_IN_DISC - blockoffset)
    int userAvailableBlocks = BLOCKS_IN_DISC - block_offset;
    int diskn = VIRTIO_RAID_DISK_START + 2 * block / (userAvailableBlocks);
    block = block_offset + block % userAvailableBlocks;

    if(diskn > USABLE_DISKS)
        return OUT_OF_BLOCK_RANGE; // block van opsega
    
    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(diskn)]);
    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(PARTNER_DISK(diskn))]);
    
    int fail = write_block_with_check(diskn,block,(uchar*)bufferPA);
    fail += write_block_with_check(PARTNER_DISK(diskn),block,(uchar*)bufferPA);

    releasesleep(&os2_sleeplocks[OS2_DISKLOCK(PARTNER_DISK(diskn))]);
    releasesleep(&os2_sleeplocks[OS2_DISKLOCK(diskn)]);
   
    return fail==2 ? BROKEN_DISK : 0;
}


uint64 sys_disk_fail_raid_1(int diskn)
{
    disk_info[diskn].broken=1;

    return 0;
}


uint64 sys_disk_repaired_raid_1(int diskn)
{
    if(IN_DISK_RANGE(diskn) == 0)
    {
        return OUT_OF_DISK_RANGE;
    }

    
    int partner_disk = PARTNER_DISK(diskn);
    if(disk_info[partner_disk].broken)
    {
        return CANT_FIX_DISK; //both are beyond repair
    }

    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(diskn)]);
    acquiresleep(&os2_sleeplocks[OS2_DISKLOCK(partner_disk)]);
    uchar* buffer = (uchar*)kalloc();
    
    int fail=0;
    for(int i = 0; i < BLOCKS_IN_DISC && !fail; i++)
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
    
    disk_info[diskn].broken=0;
    return 0;
}




uint64 sys_info_raid_1(uint *blkn, uint *blks, uint *diskn)
{
    *blkn = (USABLE_DISKS)* (BLOCKS_IN_DISC - block_offset);
    *blks = BSIZE;
    *diskn = USABLE_DISKS;
    return 0;
}



uint64 sys_destroy_raid_1()
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
        //printf("Cleared disk %d\n", i);
        disk_info[i].broken=1;
    }

    block_offset = -1;
    kfree(pageBuffer);
    return 0;
}

