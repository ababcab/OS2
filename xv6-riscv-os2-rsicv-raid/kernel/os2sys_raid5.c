#include "os2sys_raid5.h"

#define USABLE_DISKS VIRTIO_RAID_DISK_END
//#define PARITY_BLOCKS_PASSED(x) (((x/(USABLE_DISKS* USABLE_DISKS - USABLE_DISKS))*USABLE_DISKS) + (x%(USABLE_DISKS*USABLE_DISKS-USABLE_DISKS))/USABLE_DISKS)
// #define PARITY_BLOCKS_PASSED_IN_D_D(x) ((x)/(USABLE_DISKS-1))
#define PARITY_BLOCKS_PASSED_IN_D_D(x) ((x+1)/(USABLE_DISKS-1))
#define CHECK_(x) (x % (USABLE_DISKS * USABLE_DISKS)==0 ? 0 : PARITY_BLOCKS_PASSED_IN_D_D((x-1) % (USABLE_DISKS * USABLE_DISKS)))
#define BIG_(x) ((x/(USABLE_DISKS* USABLE_DISKS - USABLE_DISKS))*USABLE_DISKS) 
//#define PARITY_BLOCKS_PASSED(x) (((x/(USABLE_DISKS* USABLE_DISKS - USABLE_DISKS))*USABLE_DISKS) + ((x)/(USABLE_DISKS-1)))
#define PARITY_BLOCKS_PASSED(x) (BIG_(x)+ CHECK_(x))


#define PARITY_BIG(x) ((x/(USABLE_DISKS*USABLE_DISKS - USABLE_DISKS)*USABLE_DISKS))
#define X_MOD_DD_D(x) (x%(USABLE_DISKS*USABLE_DISKS - USABLE_DISKS))
#define PARITY_SMALL(x) ((X_MOD_DD_D(x) == 0 ? 0 : ( X_MOD_DD_D(x) == (USABLE_DISKS*USABLE_DISKS - USABLE_DISKS - 1)) ? USABLE_DISKS :(X_MOD_DD_D(x)-1)/(USABLE_DISKS-2)))
#define PPARITY(x) (PARITY_BIG(x) + PARITY_SMALL(x))


int faulty_block(int block_arg)
{
    int uslov=0;
    switch(USABLE_DISKS)
    {
        case 3:
        uslov = block_arg - 3;
        uslov%=6;
        break;
        case 4:
        uslov = block_arg - 8;
        uslov%=12;
        break;
        case 5:
        uslov = block_arg - 15;
        uslov%=20;
        break;
        case 6:
        uslov = block_arg - 62;
        uslov%=30;
        break;
        case 7:
        uslov = block_arg - 35;
        uslov%=42;
        break;
    }
    return uslov;
}

/// @brief RAID5 index calculation
/// @param block_arg logical block number
/// @param r_ABN physical block number across all disks
/// @param r_parity_disk_index index of disk that contains parity block for this row
/// @param r_block_interact physical block number inside one disk (desired block)
/// @param r_disk_interact index of desired block's disk
void calc_indexes(int *block_arg, int *r_ABN, int *r_parity_disk_index, int *r_block_interact, int *r_disk_interact)
{
    //printf("\nblock_arg: %d; BIG: %d; SMALL: %d\n",*block_arg,BIG_(*block_arg),CHECK_(*block_arg));

    *r_ABN = *block_arg + PPARITY(*block_arg);

    *r_block_interact = *r_ABN/USABLE_DISKS + block_offset;
    *r_disk_interact = *r_ABN % USABLE_DISKS + 1;
    *r_parity_disk_index = USABLE_DISKS - (*r_block_interact - block_offset) % USABLE_DISKS;
}



uint64 sys_init_raid_5()
{
    uchar* info = (uchar*)kalloc();
    info[0] = '0' + RAID5;
    info[1] = '0';
    for(int i=VIRTIO_RAID_DISK_START; i<=VIRTIO_RAID_DISK_END;i++)
    {
        write_block(i,0,info);
        disk_info[i].broken=0;
        
    }
    kfree(info);
    block_offset = 1;
    cleardisks();
    return 0;
}




uint64 sys_read_raid_5(int block_arg,uint64 bufferPA)
{
    int actual_block_number,disk_in_which_the_parity_block_is,block_to_interact_with,disk;
    calc_indexes(&block_arg,&actual_block_number,&disk_in_which_the_parity_block_is,&block_to_interact_with,&disk);


    if(disk_info[disk].broken)
    {
        acquiresleep(&os2_sleeplocks[OS2_ROWLOCK(block_to_interact_with)]);
        uchar* buffer = (uchar*)bufferPA;
        uchar* all_disks = kalloc();
        uchar* temp = kalloc();
        int flush=1;
        int fail=0;
        for(int i=VIRTIO_RAID_DISK_START; i<=VIRTIO_RAID_DISK_END && !fail;i++)
        {
            if(i==disk)
                continue;
            if(flush)
            {
                fail = read_block_with_check(i,block_to_interact_with,all_disks);
                flush=0;
            }
            else
            {
                fail = read_block_with_check(i,block_to_interact_with,temp);
                if(!fail)
                    for(int i=0;i<BSIZE;i++)
                    {
                        all_disks[i]^=temp[i];
                    }
            }
        }
        if (!fail)
            for(int i=0;i<BSIZE;i++)
            {
                buffer[i]=all_disks[i];
            }
        
        releasesleep(&os2_sleeplocks[OS2_ROWLOCK(block_to_interact_with)]);
        kfree(all_disks);
        kfree(temp);
        return fail ? BROKEN_DISK : 0;
    }
    else
    {
        //int uslov = faulty_block(block_arg); if(uslov==0)  printf("RAID5: reading on disk %d on block: %d (ABN: %d,ABN-4: %d, block_arg : %d); parity is on disk: %d (block number of parity: %d)\n", disk,block_to_interact_with, actual_block_number + 4,actual_block_number,block_arg, disk_in_which_the_parity_block_is,block_to_interact_with*USABLE_DISKS+disk_in_which_the_parity_block_is-1-4);
        
        read_block(disk,block_to_interact_with,(uchar*)bufferPA);
        return 0;
    }

}


uint64 sys_write_raid_5(int block_arg,uint64 bufferPA)
{
    int actual_block_number,disk_in_which_the_parity_block_is,block_to_interact_with,disk;
    calc_indexes(&block_arg,&actual_block_number,&disk_in_which_the_parity_block_is,&block_to_interact_with,&disk);

    //printf("\nRAID5: writing on disk %d on block: %d (ABN: %d,ABN-4: %d, block_arg : %d); parity is on disk: %d (block number of parity: %d)", disk,block_to_interact_with, actual_block_number + 4,actual_block_number,block_arg, disk_in_which_the_parity_block_is,block_to_interact_with*USABLE_DISKS+disk_in_which_the_parity_block_is-1-4);

    if(disk_info[disk].broken || disk_info[disk_in_which_the_parity_block_is].broken)
    {
        return BROKEN_DISK;
    }

    acquiresleep(&os2_sleeplocks[OS2_ROWLOCK(block_to_interact_with)]);

    uchar* old = (uchar*) kalloc();
    uchar* parity = (uchar*) kalloc();
    uchar* new = (uchar*) bufferPA;

    if(read_block_with_check(disk, block_to_interact_with,old))
    {
        releasesleep(&os2_sleeplocks[OS2_ROWLOCK(block_to_interact_with)]);
        kfree(old);
        kfree(parity);
        return BROKEN_DISK;
    }

    for(int i=0;i<BSIZE;i++)
    {
        old[i]    ^= new[i];
    }
    // sad je old ustvari change
    if( write_block_with_check(disk, block_to_interact_with,new))
    {
        releasesleep(&os2_sleeplocks[OS2_ROWLOCK(block_to_interact_with)]);
        kfree(old);
        kfree(parity);
        return BROKEN_DISK;
    }
    if(read_block_with_check(disk_in_which_the_parity_block_is, block_to_interact_with,parity))
    {
        releasesleep(&os2_sleeplocks[OS2_ROWLOCK(block_to_interact_with)]);
        kfree(old);
        kfree(parity);
        return BROKEN_DISK;
    }
    for(int i=0;i<BSIZE;i++)
    {
        parity[i] ^= old[i];
    }
    int fail = write_block_with_check(disk_in_which_the_parity_block_is, block_to_interact_with,parity);

    releasesleep(&os2_sleeplocks[OS2_ROWLOCK(block_to_interact_with)]);
    kfree(old);
    kfree(parity);

    return fail ? BROKEN_DISK : 0;
}


uint64 sys_info_raid_5(uint *blkn, uint *blks, uint *diskn)
{
    *blkn = (VIRTIO_RAID_DISK_END - 1) * (BLOCKS_IN_DISC - block_offset);
    *blks = BSIZE;
    *diskn = VIRTIO_RAID_DISK_END;
    return 0;
}

uint64 sys_disk_fail_raid_5(int diskn)
{
    disk_info[diskn].broken=1;
    printf("Disk %d status: %d\n",diskn,disk_info[diskn].broken);

    return 0;
}
uint64 sys_disk_repaired_raid_5(int diskn)
{
    int notWorking=0;
    for(int i=VIRTIO_RAID_DISK_START;i<=VIRTIO_RAID_DISK_END;i++)
    {
        if(disk_info[i].broken)
            notWorking++;
        //printf("Disk %d status: %d\n",i,disk_info[i].broken);
    }
    
    if(notWorking>1) // previse diskova ne radi
        return CANT_FIX_DISK;
    uchar* buffer =(uchar*) kalloc();
    uchar* temp =(uchar*) kalloc();
    int flush=1,fail=0;
    for(int i=block_offset;i<BLOCKS_IN_DISC && !fail;i++)
    {
        acquiresleep(&os2_sleeplocks[OS2_ROWLOCK(buffer)]);
        for(int curr=VIRTIO_RAID_DISK_START; curr<=VIRTIO_RAID_DISK_END && !fail;curr++)
        {
            if(diskn == curr)
                continue;
            if(flush)
            {
                fail = read_block_with_check(curr,i,buffer);
                flush=0;
            }
            else
            {
                fail = read_block_with_check(curr,i,temp);
                for(int j=0;j<BSIZE;j++)
                {
                    buffer[j]^=temp[j];
                }
            }
        }
        if(!fail)
        {
            fail = write_block_with_check(diskn,i,buffer);
            flush=1;
        }
        releasesleep(&os2_sleeplocks[OS2_ROWLOCK(buffer)]);
    }
    kfree(temp);
    kfree(buffer);
    if(fail)
    {
        disk_info[diskn].broken=1;
        return CANT_FIX_DISK;
    }
    disk_info[diskn].broken=0;
    return 0;
}


uint64 sys_destroy_raid_5()
{

    uchar* pageBuffer = (uchar*)kalloc();
    pageBuffer[0]=~0;

    for(int i=VIRTIO_RAID_DISK_START; i<=VIRTIO_RAID_DISK_END;i++)
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
