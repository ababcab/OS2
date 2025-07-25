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
//#define PARITY_SMALL_ZERO
//#define PARITY_SMALL(x) ((X_MOD_DD_D(x) == 0 || X_MOD_DD_D(x) == (USABLE_DISKS*USABLE_DISKS - USABLE_DISKS - 1))? 0 : (X_MOD_DD_D(x)-1)/(USABLE_DISKS-2))
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


void calc_indexes(int *block_arg, int *r_ABN, int *r_parity_disk_index, int *r_block_interact, int *r_disk_interact)
{
    //printf("\nblock_arg: %d; BIG: %d; SMALL: %d\n",*block_arg,BIG_(*block_arg),CHECK_(*block_arg));

    *r_ABN = *block_arg + PPARITY(*block_arg);
    //*r_ABN = *block_arg + PARITY_BLOCKS_PASSED(*block_arg);

    *r_block_interact = *r_ABN/USABLE_DISKS + block_offset;
    *r_disk_interact = *r_ABN % USABLE_DISKS + 1;
    *r_parity_disk_index = USABLE_DISKS - (*r_block_interact-block_offset) % USABLE_DISKS;
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
    block_offset = 1;
    for(int i=0;i<PGSIZE;i++)
        info[i]=0;
    int block=block_offset, diskn=VIRTIO_RAID_DISK_END;
    while(block<BLOCKS_IN_DISC)
    {
        write_block(diskn,block,info);

        block++;
        diskn = ((diskn - 1 ) - 1 + VIRTIO_RAID_DISK_END) % VIRTIO_RAID_DISK_END + 1;
    }
    kfree(info);
    return 0;
}




uint64 sys_read_raid_5(int block_arg,uint64 bufferPA)
{
    /**
    // block je redni broj bloka ako gledamo samo blokove sa podacima
    block += USABLE_DISKS;

    //koliko treba da se preskoci parity blokova
    int passed_parity_blocks = (block - USABLE_DISKS)/(USABLE_DISKS + 1) + 1;

    //na kom je disku trazeni blok
    int disk = (block + passed_parity_blocks) % USABLE_DISKS;

    printf("\nRAID5: reading from disk %d\n", disk);

    block = (block + passed_parity_blocks) / USABLE_DISKS;


    */

    /**
    int actual_block_number = block_arg + block_arg/ USABLE_DISKS + 1;
    int disk = actual_block_number % USABLE_DISKS + 1;
    int block_to_interact_with = actual_block_number / USABLE_DISKS + block_offset;
    */
    //int disk_in_which_the_parity_block_is = (block_arg/ USABLE_DISKS) % USABLE_DISKS + 1;

    int actual_block_number,disk_in_which_the_parity_block_is,block_to_interact_with,disk;
    calc_indexes(&block_arg,&actual_block_number,&disk_in_which_the_parity_block_is,&block_to_interact_with,&disk);


    if(disk_info[disk].broken)
    {
        uchar* buffer = (uchar*)bufferPA;
        uchar* all_disks = kalloc();
        uchar* temp = kalloc();
        int flush=1;

        for(int i=VIRTIO_RAID_DISK_START; i<=VIRTIO_RAID_DISK_END;i++)
        {
            if(i==disk)
                continue;
            if(disk_info[i].broken)
            {
                kfree(all_disks);
                kfree(temp);
                return BROKEN_DISK;
            }
            if(flush)
            {
                read_block(i,block_to_interact_with,all_disks);
                flush=0;
            }
            else
            {
                read_block(i,block_to_interact_with,temp);
                for(int i=0;i<BSIZE;i++)
                {
                    all_disks[i]^=temp[i];
                }
            }
        }


        for(int i=0;i<BSIZE;i++)
        {
            buffer[i]=all_disks[i];
        }

        kfree(all_disks);
        kfree(temp);
        return 0;
    }
    else
    {
        //int uslov = faulty_block(block_arg);
        //if(uslov==0)  printf("RAID5: reading on disk %d on block: %d (ABN: %d,ABN-4: %d, block_arg : %d); parity is on disk: %d (block number of parity: %d)\n", disk,block_to_interact_with, actual_block_number + 4,actual_block_number,block_arg, disk_in_which_the_parity_block_is,block_to_interact_with*USABLE_DISKS+disk_in_which_the_parity_block_is-1-4);
        
        read_block(disk,block_to_interact_with,(uchar*)bufferPA);
        return 0;
    }

}


uint64 sys_write_raid_5(int block_arg,uint64 bufferPA)
{
    /** 
    // block je redni broj bloka ako gledamo samo blokove sa podacima
    block += USABLE_DISKS;

    //koliko treba da se preskoci parity blokova
    //int passed_parity_blocks = (block - USABLE_DISKS)/(USABLE_DISKS + 1) + 1;
    
    //na kom je disku trazeni blok
    //int disk = (block + passed_parity_blocks) % USABLE_DISKS + 1;

    int ABN= ((1+block -USABLE_DISKS)*(USABLE_DISKS+1)) / USABLE_DISKS;
   // int passed_parity_blocks = ABN/(USABLE_DISKS +1) +1;    

    int disk = (ABN) % USABLE_DISKS + 1;
    //printf("\nRAID5: writing on disk %d\n", disk);

    //int blockIndexWhole = block + passed_parity_blocks;

    int blockIndexWhole= ABN + USABLE_DISKS;
    //block = (block + passed_parity_blocks) / USABLE_DISKS;
    block = (blockIndexWhole) / USABLE_DISKS;

    int disk_in_which_the_parity_block_is = (block - 1) % USABLE_DISKS + 1;

    printf("\nRAID5: writing on disk %d blockIndexWhole: %d; parity is on disk: %d\n", disk,blockIndexWhole, disk_in_which_the_parity_block_is);

    */
    int actual_block_number,disk_in_which_the_parity_block_is,block_to_interact_with,disk;
    calc_indexes(&block_arg,&actual_block_number,&disk_in_which_the_parity_block_is,&block_to_interact_with,&disk);

    //printf("\nRAID5: writing on disk %d on block: %d (ABN: %d,ABN-4: %d, block_arg : %d); parity is on disk: %d (block number of parity: %d)", disk,block_to_interact_with, actual_block_number + 4,actual_block_number,block_arg, disk_in_which_the_parity_block_is,block_to_interact_with*USABLE_DISKS+disk_in_which_the_parity_block_is-1-4);

    if(disk_info[disk].broken || disk_info[disk_in_which_the_parity_block_is].broken)
    {
        return BROKEN_DISK;
    }
    uchar* old = (uchar*) kalloc();
    uchar* parity = (uchar*) kalloc();
    uchar* new = (uchar*) bufferPA;

    read_block(disk, block_to_interact_with,old);

    for(int i=0;i<BSIZE;i++)
    {
        old[i]    ^= new[i];
    }
    // sad je old ustvari change
    write_block(disk, block_to_interact_with,new);
    read_block(disk_in_which_the_parity_block_is, block_to_interact_with,parity);
    for(int i=0;i<BSIZE;i++)
    {
        parity[i] ^= old[i];
    }
    write_block(disk_in_which_the_parity_block_is, block_to_interact_with,parity);

    kfree(old);
    kfree(parity);

    return 0;
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
    uchar* block =(uchar*) kalloc();
    uchar* temp =(uchar*) kalloc();
    int flush=1;
    for(int i=0;i<BLOCKS_IN_DISC;i++)
    {
        for(int curr=VIRTIO_RAID_DISK_START; curr<=VIRTIO_RAID_DISK_END;curr++)
        {
            if(diskn == curr)
                continue;
            if(flush)
            {
                read_block(curr,i,block);
                flush=0;
            }
            else
            {
                read_block(curr,i,temp);
                for(int j=0;j<BSIZE;j++)
                {
                    block[j]^=temp[j];
                }
            }
        }
        write_block(diskn,i,block);
        flush=1;
    }
    kfree(temp);
    kfree(block);
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
