//
// Created by os on 2/22/25.
//

#include "os2sys_raid0.h"

uint64 sys_init_raid_0(void)
{
	uchar* info=(uchar*)kalloc();

	info[0]=(uchar)('0' + RAID0);//koji je raid (gleda se po vrednosti enum-a)
	info[1]=(uchar)'0';//da li je polomljen disk: 0-nije, 1-jeste
	for(int i=VIRTIO_RAID_DISK_START; i<=VIRTIO_RAID_DISK_END;i++)
	{
		write_block(i,0,info);
		disk_info[i].broken=0;
	}
	kfree(info);

	block_offset = 1;

	return 0;
}


uint64 sys_read_raid_0(int block,uint64 bufferPA)//buffer)
{
	block += VIRTIO_RAID_DISK_END; // ovo je vec block_offset

	int disk = (block % VIRTIO_RAID_DISK_END ) + 1;

	int blockIndex= block / VIRTIO_RAID_DISK_END;

	if(disk_info[disk].broken)
		return BROKEN_DISK;

//  printf("\nCitanje sa diska %d blok %d %d\n", disc,blockIndex,block);
	read_block(disk,blockIndex,(uchar*)bufferPA);

	return 0;

}


uint64 sys_write_raid_0(int block,uint64 bufferPA)//bufferVA)
{
    block += VIRTIO_RAID_DISK_END; // ovo je vec block_offset

    int disk= (block % VIRTIO_RAID_DISK_END) + 1;
    int blockIndex= block / VIRTIO_RAID_DISK_END;

	if(disk_info[disk].broken)
		return BROKEN_DISK;

    write_block(disk,blockIndex,(uchar*)bufferPA);

    return 0;
}

uint64 sys_disk_fail_raid_0(int diskn)
{
    disk_info[diskn].broken=1;

    return 0;
}

uint64 sys_disk_repaired_raid_0(int diskn)
{
    return CANT_FIX_DISK;
}


uint64 sys_info_raid_0(uint *blkn, uint *blks, uint *diskn)
{
    *blkn = VIRTIO_RAID_DISK_END * (BLOCKS_IN_DISC - block_offset);
    *blks = BSIZE;
    *diskn = VIRTIO_RAID_DISK_END;
    return 0;
}



uint64 sys_destroy_raid_0(void)
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
        //printf("Cleared disk %d\n", i);
        disk_info[i].broken=1;
    }

    block_offset = -1;
    kfree(pageBuffer);
    return 0;
}
