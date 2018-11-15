#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<libgen.h>
#include<sys/types.h>
#include<sys/time.h>
#include<limits.h>
#include<math.h>
#include<time.h>
#define BLOCK_SIZE 512
#define NUM_INODES 16
#define NUM_INODE_BLOCKS 4
#define INODES_BLOCK 4
#define NUM_DATA_BLOCKS 32
#define MAX_NAME_LEN 28
#define NUM_DIRECT 10
#define DISK_INODE_SIZE sizeof(struct diskInode)
#define SB_SIZE sizeof(struct superblock)
#define INODE_BM_SIZE sizeof(struct inodeBit)
#define DATA_BM_SIZE sizeof(struct dataBit)
#define MAX_DIR_DEPTH 20
#define DIR_ENTRIES_BLOCK (BLOCK_SIZE/sizeof(dirRecord))
#define NUM_BLOCKS 39

const int inodeStartBlk = 4;
const int dataStartBlk = 8;

int fsp;

struct superblock{		// Block number 1		
	unsigned long int fsSize;
	unsigned int numFreeData, numFreeInodes, inodeBMap, dataBMap, rootInode;
	char filler[BLOCK_SIZE - 28];
};

struct dirRecord{
	unsigned int inodeNum;
	char name[MAX_NAME_LEN];
};

struct diskInode{
	unsigned int uid;
	unsigned int gid;
	unsigned int n_link;
	unsigned int mode;
	unsigned int blockNums[NUM_DIRECT];
	unsigned int numRecords;
	int blockSize;
	long size;
	long numBlocks;
	int 	aTime;
	unsigned int aTimeNsec;
	int	mTime;
	unsigned int mTimeNsec;
	int	cTime;
	unsigned int cTimeNsec;
	unsigned int type;
	int filler[5];
};
// For the flags 0 - not occupied, 1 - occupied

struct inodeBit{		
	unsigned char flag[NUM_INODES];
	char filler[BLOCK_SIZE - NUM_INODES];
};

struct dataBit{
	unsigned char flag[NUM_DATA_BLOCKS];
	char filler[BLOCK_SIZE - NUM_DATA_BLOCKS];
};



struct superblock spBlk;
struct inodeBit ibmap;
struct dataBit dbmap;
struct dirRecord rootRec[2];
struct diskInode root;


int main(int argc, char *argv){
        unsigned char *buffer = malloc(BLOCK_SIZE);

	struct timespec currTime;
	spBlk.fsSize = NUM_BLOCKS*BLOCK_SIZE;
	spBlk.numFreeData = NUM_DATA_BLOCKS;
	spBlk.numFreeInodes = NUM_INODES;
	spBlk.inodeBMap = 2;
	spBlk.dataBMap = 3;
	spBlk.rootInode = 1;


	rootRec[0].inodeNum = 1;
	strcpy(rootRec[0].name, ".");
	rootRec[1].inodeNum = 1;
	strcpy(rootRec[1].name, "..");

	clock_gettime(CLOCK_REALTIME, &currTime);

	root.uid = getuid();
	root.gid = getgid();
	root.n_link = 1;
	root.mode = S_IFDIR | 0777;
	root.blockNums[0] = 8;
	root.numRecords = 2;
	root.blockSize = BLOCK_SIZE;
	root.size = 2*sizeof(struct dirRecord);
	root.numBlocks = 1;
	root.aTime = currTime.tv_sec;
	root.aTimeNsec = currTime.tv_nsec;
	root.mTime = currTime.tv_sec;
	root.mTimeNsec = currTime.tv_nsec;
	root.cTime = currTime.tv_sec;
	root.cTimeNsec = currTime.tv_nsec;
	root.type = 2; // 2 for directory


	fsp = open("M", O_CREAT | O_TRUNC | O_RDWR, 0777);
	if(fsp<0){
		printf("Error in open");
	}
	write(fsp, &spBlk, sizeof(struct superblock));
	for(int i=0; i< NUM_INODES; i++){
		ibmap.flag[i] = 0;
	}
	for(int i=0; i< NUM_DATA_BLOCKS; i++){
		dbmap.flag[i] = 0;
	}
	ibmap.flag[0] = 1;
	dbmap.flag[0] = 1;
	write(fsp, &ibmap, sizeof(struct inodeBit));
	write(fsp, &dbmap, sizeof(struct dataBit));
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &root, sizeof(struct diskInode));
	write(fsp, buffer, BLOCK_SIZE);
	lseek(fsp, BLOCK_SIZE*7, SEEK_SET);
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &rootRec, sizeof(struct dirRecord)*2);
	write(fsp, buffer, BLOCK_SIZE);
	close(fsp);
	return 0;
}
