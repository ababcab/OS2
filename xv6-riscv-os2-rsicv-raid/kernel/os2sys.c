#include "os2sys.h"
#include "os2sys_raid0.h"
#include "os2sys_raid1.h"
#include "os2sys_raid0_1.h"
#include "os2sys_raid4.h"
#include "os2sys_raid5.h"

enum RAID_TYPE currentRAID=-1;
int block_offset = -1;
dInfo disk_info[VIRTIO_RAID_DISK_END + 1];

uint64 (*arr_sys_init_raid[5])  (void)          = {sys_init_raid_0,          sys_init_raid_1,          sys_init_raid_0_1,          sys_init_raid_4,             sys_init_raid_5};
uint64 (*arr_sys_read_raid[5])  (int,uint64)    = {sys_read_raid_0,          sys_read_raid_1,          sys_read_raid_0_1,          sys_read_raid_4,             sys_read_raid_5};
uint64 (*arr_sys_write_raid[5])  (int,uint64)   = {sys_write_raid_0,         sys_write_raid_1,         sys_write_raid_0_1,         sys_write_raid_4,            sys_write_raid_5};
uint64 (*arr_sys_disk_fail_raid[5])  (int)      = {sys_disk_fail_raid_0,     sys_disk_fail_raid_1,     sys_disk_fail_raid_0_1,     sys_disk_fail_raid_4,        sys_disk_fail_raid_5};
uint64 (*arr_sys_disk_repaired_raid[5])  (int)  = {sys_disk_repaired_raid_0, sys_disk_repaired_raid_1, sys_disk_repaired_raid_0_1, sys_disk_repaired_raid_4,    sys_disk_repaired_raid_5};
uint64 (*arr_sys_info_raid[5])  (uint *,uint *,uint *)= {sys_info_raid_0,          sys_info_raid_1,          sys_info_raid_0_1,          sys_info_raid_4,             sys_info_raid_5};
uint64 (*arr_sys_destroy_raid[5])  (void)       = {sys_destroy_raid_0,       sys_destroy_raid_1,       sys_destroy_raid_0_1,       sys_destroy_raid_4,          sys_destroy_raid_5};

#pragma region Definitions
// Returns entry offset for a virtual address
#define PGOFFSET(x) (x - PGROUNDDOWN(x))

// Returns variable's physical addr based on the virtual address
// Cast the virtual addr to uint64
// Use: 
// uint64 variableVA; 
// type* variable = (type*)Virt2PhysAddr(variableVA)
#define Virt2PhysAddr(x) (walkaddr(myproc()->pagetable,x) + PGOFFSET(x))
// Constructs usable-in-syscalls pointer based on a uint64 virtual address
#define UsableVariable(type_t,name,VA) type_t* name = (type_t*)Virt2PhysAddr(VA)


// 0. javni_test
// 1. Nemoj da pravis lokalne pokazivace, nego samo lokalne promenljive
// 2. Adresa lokalnih promenljivi su i fizicke i virtuelne adrese
/**  3.

  uint64 buf=0; //vrednost ovoga je adresa bafera
  argaddr(1, &buf);

  printf("va addr %x\n",buf);

  printf("offset %x\n",buf - PGROUNDDOWN(buf));

  printf("pa bez off %x\n",walkaddr(myproc()->pagetable,buf));

  uint64* addr = (uint64*)(walkaddr(myproc()->pagetable,buf) + buf - PGROUNDDOWN(buf));

*/

#pragma endregion
//int init_raid(enum RAID_TYPE raid);

struct spinlock  os2_spinlocks[OS2_SPINLOCK_COUNT];
struct sleeplock os2_sleeplocks[OS2_SLEEPLOCK_COUNT];

void init_disk_locks()
{
	for(int i=0; i<OS2_SPINLOCK_COUNT; i++)
	{
		initlock     (&os2_spinlocks[i],"os2");
	}
	for(int i=0; i<OS2_SLEEPLOCK_COUNT; i++)
	{
		initsleeplock(&os2_sleeplocks[i],"os2 sleep");
	}
}


uint64 sys_init_raid(void)
{

	int raid;
	int ret;
	argint(0, &raid);

	acquiresleep(&os2_sleeplocks[OS2_SLEEPLOCK_INIT]);

	if(currentRAID == raid) 
		ret =  0; 
	else if (currentRAID != -1)  
		ret = ALREADY_INIT; 
	else
	{
		ret=arr_sys_init_raid[raid]();
		if(!ret)
			currentRAID = raid;
	}

	releasesleep(&os2_sleeplocks[OS2_SLEEPLOCK_INIT]);
	return ret;
}


//int read_raid(int blkn, uchar* data);

uint64 sys_read_raid(void)
{
  if (currentRAID == -1)
    return NOT_INIT;

  int block;
  uint64 bufferVA;
  argint(0,&block);
  argaddr(1,&bufferVA);
  UsableVariable(uint64,buffer,bufferVA);

  return  arr_sys_read_raid[currentRAID](block,(uint64)buffer);
}

//int write_raid(int blkn, uchar* data);

uint64 sys_write_raid(void)
{
  if (currentRAID == -1)
    return NOT_INIT;

  int block;
  uint64 bufferVA;
  argint(0, &block);
  argaddr(1, &bufferVA);
  UsableVariable(uint64,buffer,bufferVA);

  return arr_sys_write_raid[currentRAID](block,(uint64)buffer);

}

//int disk_fail_raid(int diskn);

uint64 sys_disk_fail_raid(void)
{
	if (currentRAID == -1)
		return NOT_INIT;


	int diskn;
	argint(0,&diskn);

	if(diskn<VIRTIO_RAID_DISK_START || diskn>VIRTIO_RAID_DISK_END)
		return OUT_OF_DISK_RANGE;

	if(disk_info[diskn].broken)
		return 0;
		
	return arr_sys_disk_fail_raid[currentRAID](diskn);
  
}

//int disk_repaired_raid(int diskn);

uint64 sys_disk_repaired_raid(void)
{
	if (currentRAID == -1)
		return NOT_INIT;

	int diskn;
	argint(0,&diskn);

	if(diskn<VIRTIO_RAID_DISK_START || diskn>VIRTIO_RAID_DISK_END)
		return OUT_OF_DISK_RANGE;

	if(!disk_info[diskn].broken)
		return 0;

	return arr_sys_disk_repaired_raid[currentRAID](diskn);
 
}

//int info_raid(uint *blkn, uint *blks, uint *diskn);

uint64 sys_info_raid(void)
{
  if(currentRAID == -1)
    return 0;

  uint64 blknVA; 
  uint64 blksVA; 
  uint64 disknVA; 
  argaddr(0, &blknVA);
  argaddr(1, &blksVA);
  argaddr(2, &disknVA);
  //printf("%x %x %x <-- PSL\n", (uint64)blknVA, (uint64)blksVA, (uint64)disknVA); ispisuje se ista virtuelna adresa tkd dobro prenosim



  UsableVariable(uint,blkn,blknVA);
  UsableVariable(uint,blks,blksVA);
  UsableVariable(uint,diskn,disknVA);


  return arr_sys_info_raid[currentRAID](blkn,blks,diskn);

}

//int destroy_raid();

uint64 sys_destroy_raid(void)
{
    if(currentRAID==-1)
    return NOT_INIT;

    arr_sys_destroy_raid[currentRAID]();

    currentRAID = -1;
    return 0;
}



// Returns 0 if can proceed
uint64 checkRAID_init( int wantInitializedRAID)
{
    if( wantInitializedRAID && currentRAID ==-1)
        return -1;
    if(!wantInitializedRAID && currentRAID !=-1)
        return -2;
    return 0;
}
// Returns 0 if can proceed
uint64 checkRAID_disc(int wantOperational, int discn)
{
    int ret=0;
    if((ret=checkRAID_init(1)) != 0)
        return ret;
    if(wantOperational && disk_info[discn].broken==1) // not broken
        return -3;
    if(!wantOperational && disk_info[discn].broken==0) // broken
        return -4;
    return 0;
}

uint64 cleardisks()
{
	uchar* zero = kalloc();
	for(int i=0;i<PGSIZE;i++)
		zero[i]=0;
	for(int disk=VIRTIO_RAID_DISK_START;disk<=VIRTIO_RAID_DISK_END;disk++)
		for(int i=block_offset;i<BLOCKS_IN_DISC;i++)
		{
			write_block(disk,i,zero);
		}
	kfree(zero);
}
