#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void check_data(uint blocks, uchar *blk, uint block_size,int comment);
void fill_data(uchar** data_block, int blkn, int blks);
int value(int i, int j);


int bogdanTest(enum RAID_TYPE raid)
{
    init_raid(raid);

    uint blkn=0, blks=0, diskn=0;
    info_raid(&blkn, &blks, &diskn);
    //printf("num of blocks: %d\nblock size: %d\nnum of discs: %d\n",  blkn, blks, diskn);

    int tmp=10;
    int blocks_to_check = (tmp > blkn ? blkn : tmp);

    uchar* data_check=0;

    printf("with no fail:\n");
    printf("\n\twriting to disks\n");
    fill_data(&data_check,blocks_to_check,blks);
    printf("\tchecking data integrity\n");
    check_data(blocks_to_check,data_check,blks,0);

    printf("\nwith 1 fail:");
    for(int i=1;i<=diskn;i++)
    {
        printf("\n\twriting to disks\n");
        fill_data(&data_check,blocks_to_check,blks);
        printf("\tfailing disk %d\n",i);
        disk_fail_raid(i);
        printf("\tchecking data integrity\n");
        check_data(blocks_to_check,data_check,blks,raid == RAID4);
        printf("\ttrying to repair disk %d\n",i);
        if(disk_repaired_raid(i))
        {
            printf("\tcant repair disk %d\n\n", i);
            break;
        }
        printf("\trepaired disk %d\n",i);
        printf("\tchecking data integrity\n");
        check_data(blocks_to_check,data_check,blks,raid == RAID4);

    }


    //printf("before destroy\n");
    destroy_raid();
    //printf("after destroy\n");
    free(data_check);
  printf("\n");
    return 0;
}

int main(int argc, char *argv[])
{
	//void (*funcs[5]) = {bogdanTest_0,bogdanTest_1,bogdanTest_2,bogdanTest_3,bogdanTest_4};
    //bogdanTest_0();
    //bogdanTest_1();


    //bogdanTest_5();
	char* imena[5]= {"RAID0","RAID1","RAID0_1","RAID4","RAID5"};
    for(int i=RAID4;i<=RAID5;i++)
	{
    	//printf("%d: %s\n",i, imena[i]);
    	printf("%s\n",imena[i]);
        bogdanTest(i);
	}

    exit(0);
}

void default_test()
{
    init_raid(RAID1);

    uint disk_num, block_num, block_size;
    info_raid(&block_num, &block_size, &disk_num);
  
    uint blocks = (512 > block_num ? block_num : 512);
  
    uchar* blk = malloc(block_size);
    for (uint i = 0; i < blocks; i++) {
      for (uint j = 0; j < block_size; j++) {
        blk[j] = j + i;
      }
      write_raid(i, blk);
    }
  
    check_data(blocks, blk, block_size,0);
  
    disk_fail_raid(2);
  
    check_data(blocks, blk, block_size,0);
  
    disk_repaired_raid(2);
  
    check_data(blocks, blk, block_size,0);
  
    free(blk);
}


void check_data(uint blocks, uchar *blk, uint block_size,int comment)
{
  int success=1;
  for (uint i = 0; i < blocks; i++)
  {
    read_raid(i, blk);
    for (uint j = 0; j < block_size; j++)
    {
      //printf("Data in the block %d: %d\n",i, blk[j]);
      if ((uchar)value(i,j)!= blk[j])
      {
        if(comment!=0)
        {
            printf("expected=%d got=%d got-exp=%d ", value(i,j), blk[j], blk[j]-value(i,j));
            printf("Data in the block %d faulty\n", i);
        }
        success = 0;
        break;
      }
    }
  }
    if(success)
    {
        printf("\t____WORKS___\n");
    }
    else
    {
        printf("\t____FAIL___\n");
        //exit(0);
    }
}


void fill_data(uchar** data_block, int blkn, int blks)
{
    if(*data_block==0)
        *data_block = malloc(blks);
    for(int i=0; i< blkn;i++)
    {
      for(int j=0; j<blks;j++)
        {
            (*data_block)[j] = value(i,j);
        }
      write_raid(i, *data_block);
    }
}
int value(int i, int j)
{
  return ('!' + j + i);
}