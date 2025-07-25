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
    int diskn=1, blocksPerDisc = BLOCKS_IN_DISC - block_offset;
    int usableDisks = USABLE_DISKS;
    block+=block_offset;
    while(block > BLOCKS_IN_DISC - block_offset && diskn <= usableDisks)
    {
        block-= blocksPerDisc;
        diskn+=2;
        block++;
        //printf("loop in read raid1 %d %d %d\n",diskn, block, blocksPerDisc);
    }
    if(diskn > usableDisks)
        return OUT_OF_BLOCK_RANGE;
    if(disk_info[diskn].broken)
        diskn++; // njegova kopija
    if(disk_info[diskn].broken)
        return BROKEN_DISK; // obe kopije ne funkcionisu
    read_block(diskn,block,(uchar*)bufferPA);

    return 0;
}


uint64 sys_write_raid_1(int block,uint64 bufferPA)
{
    int disc=1, blocksPerDisc = BLOCKS_IN_DISC - block_offset;
    int usableDisks = USABLE_DISKS;
    block+=block_offset;
    while(block > BLOCKS_IN_DISC - block_offset && disc <= usableDisks)
    {
        block-= blocksPerDisc;
        disc+=2;
        block++;
    }
    if(disc > usableDisks)
        return OUT_OF_BLOCK_RANGE; // block van opsega
        
    if(!disk_info[disc]      .broken)
        write_block(disc,     block,(uchar*)bufferPA); //upisi u original
    if(!disk_info[ disc + 1 ].broken)
        write_block(disc + 1 ,block,(uchar*)bufferPA); //upisi u kopiju

    if(disk_info[disc].broken && disk_info[disc+1].broken) //oba su beyond repair
        return BROKEN_DISK;

    return 0;
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

    if(disk_info[diskn].broken)
    {
        int partner_disk = PARTNER_DISK(diskn);
        if(disk_info[partner_disk].broken)
        {
            return CANT_FIX_DISK; //both are beyond repair
        }

        uchar* block=(uchar*)kalloc();
        for(int blockIndex =0; blockIndex < BLOCKS_IN_DISC;blockIndex++)
        {
            read_block(partner_disk,blockIndex,block);
            write_block(diskn,blockIndex,block);
        }
        kfree(block);
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

