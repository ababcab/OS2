#include "os2sys_raid4.h"


#define PARITY_DISK (VIRTIO_RAID_DISK_END)
#define USABLE_DISKS (PARITY_DISK-1)


uint64 sys_init_raid_4()
{
    uchar* info = (uchar*)kalloc();
    info[0] = '0' + RAID4;
    info[1] = '0';
    for(int i=VIRTIO_RAID_DISK_START; i<=VIRTIO_RAID_DISK_END;i++)
    {
        write_block(i,0,info);
        disk_info[i].broken=0;
        
    }
    /*
    int parity=PARITY_DISK;
    for(int i=block_offset;i<BLOCKS_IN_DISC;i++)
    {
        write_block(parity,i,info);
    }
    */
    kfree(info);
    block_offset = 1;
    cleardisks();
    return 0;
}




uint64 sys_read_raid_4(int block,uint64 bufferPA)
{
    //int org=block;
    block += USABLE_DISKS;
    int disk = block % USABLE_DISKS + 1;
    block = block / USABLE_DISKS;

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
                printf("Disk %d status %d \n",disk,disk_info[disk].broken);
                kfree(all_disks);
                kfree(temp);
                return BROKEN_DISK;
            }
            read_block(i,block,temp);
            
            for(int j=0;j<BSIZE;j++)
            {
                if(flush)
                    all_disks[j]=temp[j];
                else
                    all_disks[j]^=temp[j];
            }
            flush=0;
        }


        for(int i=0;i<BSIZE;i++)
        {
            buffer[i]=all_disks[i];
           // printf("Buffer %d : %d\n",i,buffer[i]);
        }

        //printf("Disk %d status %d | Read from other disks block %d\n",disk,disk_info[disk].broken,org );
        kfree(all_disks);
        kfree(temp);
        return 0;
    }
    else
    {
        read_block(disk,block,(uchar*)bufferPA);
        return 0;
    }

}


uint64 sys_write_raid_4(int block,uint64 bufferPA)
{
    block += USABLE_DISKS;
    int disk = block % USABLE_DISKS + 1;
    block = block / USABLE_DISKS;

    if(disk_info[disk].broken || disk_info[PARITY_DISK].broken)
    {
        printf("RAID4\n\tDisk %d status %d | Parity %d status %d\n",disk,disk_info[disk].broken,PARITY_DISK,disk_info[PARITY_DISK].broken);
        return BROKEN_DISK;
    }
    uchar* old = (uchar*) kalloc();
    uchar* parity = (uchar*) kalloc();
    uchar* new = (uchar*) bufferPA;

    read_block(disk,block,old);

    for(int i=0;i<BSIZE;i++)
    {
        old[i]    ^= new[i];
    }
    // sad je old ustvari change
    write_block(disk,block,new);
    read_block(PARITY_DISK,block,parity);
    for(int i=0;i<BSIZE;i++)
    {
        //if(i==0)
          //  printf("old: %d\tdisk %d\n\tnew %d\n",parity[i],disk,parity[i]^old[i]);
        parity[i] ^= old[i];
    }
    write_block(PARITY_DISK,block,parity);

    kfree(old);
    kfree(parity);

    return 0;
}



uint64 sys_disk_fail_raid_4(int diskn)
{
    disk_info[diskn].broken = 1;
    //printf("RAID4\n\tDisk %d status: %d\n",diskn,disk_info[diskn].broken);
    
    return 0;
}


uint64 sys_disk_repaired_raid_4(int disk)
{
    if(disk_info[disk].broken == 0)
        return 0;
    
    int notWorking=0;
    for(int i=VIRTIO_RAID_DISK_START;i<=VIRTIO_RAID_DISK_END;i++)
    {
        if(disk_info[i].broken)
            notWorking++;
        //printf("Disk %d status: %d\n",i,disk_info[i].broken);
    }
    
    if(notWorking>1) // previse diskova ne radi
        return CANT_FIX_DISK;

    int flush =1;
    //uchar* all_disks = kalloc_zero();
    uchar* temp = kalloc();
    uchar* buffer = kalloc();

    for(int block=block_offset;block<BLOCKS_IN_DISC;block++)
    {
        for(int i=VIRTIO_RAID_DISK_START; i<=VIRTIO_RAID_DISK_END;i++)
        {
            if(i==disk)
                continue;
            if(flush)
            {
                read_block(i,block,buffer);
                flush=0;
                continue;
            }
            else
                read_block(i,block,temp);
            for(int j=0;j<BSIZE; j++)
            {
                buffer[j]^= temp[j];
            }

        }
        write_block(disk,block,buffer);
        flush=1;
    }
    
    disk_info[disk].broken=0;
    kfree(buffer);
    kfree(temp);
    return 0;
}

uint64 sys_info_raid_4(uint *blkn, uint *blks, uint *diskn)
{
    *blkn = (VIRTIO_RAID_DISK_END - 1) * (BLOCKS_IN_DISC - block_offset);
    *blks = BSIZE;
    *diskn = VIRTIO_RAID_DISK_END - 1;
    return 0;
}
uint64 sys_destroy_raid_4()
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