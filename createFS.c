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
#define BLOCK_SIZE 512
#define NUM_INODES 16
#define NUM_INODE_BLOCKS 2 
#define INODES_BLOCK 8
#define NUM_DATA_BLOCKS 32
#define MAX_NAME_LEN 28
#define NUM_DIRECT 10
#define DISK_INODE_SIZE sizeof(struct diskInode)
#define SB_SIZE sizeof(struct superblock)
#define INODE_BM_SIZE sizeof(struct inodeBit)
#define DATA_BM_SIZE sizeof(struct dataBit)
#define MAX_DIR_DEPTH 20
#define DIR_ENTRIES_BLOCK (BLOCK_SIZE/sizeof(dirRecord))
#define NUM_BLOCKS 37

const int inodeStartBlk = 4;
const int dataStartBlk = 6;

FILE *fsp = NULL;

struct superblock{		// Block number 1		
	unsigned long int fsSize;
	unsigned int numFreeData, numFreeInodes, inodeBMap, dataBMap, rootInode;
	char filler[BLOCK_SIZE - 28];
};

struct dirRecord{
	unsigned int inodeNum;
	char name[MAX_NAME_LEN];
};

struct memSuperblock{
	struct superblock dSblk;
	unsigned int flagSB; // Flag for modified superblock
};

struct diskInode{
	//struct timespec lastModified, lastAccessed, inodeModified;
	unsigned int uid, gid;
	unsigned int size, n_links;
	unsigned int mode;
	unsigned int numBlocks;
	unsigned int blockNums[NUM_DIRECT];
	unsigned int numRecords; // Required for directories
	unsigned char type;	// Type to denote whether it is a regularfile, directory etc.
};

struct memInode{
	struct diskInode dNode;
	unsigned int inodeNum;	// This field contains the inode number
	unsigned int refCount;	// Reference count
	unsigned char flagInode; // Flag for modified Inode
	unsigned char flagFile; // Flag for modified file 
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

static int getBlock(unsigned int blockNum, void *buffer){
	fseek(fsp, blockNum*BLOCK_SIZE, SEEK_SET);
	fread((char*)buffer, BLOCK_SIZE, 1, fsp);
	return 1;
}

static int putBlock(unsigned int blockNum, void *buffer){
	fseek(fsp, blockNum*BLOCK_SIZE, SEEK_SET);
	fwrite((char*)buffer, BLOCK_SIZE, 1, fsp);
	return 1;
}

struct superblock spBlk;
struct inodeBit ibmap;
struct dataBit dbmap;
struct dirRecord rootRec;
struct diskInode root;


int main(int argc, char *argv){
	spBlk.fsSize = NUM_BLOCKS*BLOCK_SIZE;
	spBlk.numFreeData = NUM_DATA_BLOCKS;
	spBlk.numFreeInodes = NUM_INODES;
	spBlk.inodeBMap = 2;
	spBlk.dataBMap = 3;
	spBlk.rootInode = 1;


	rootRec.inodeNum = 1;
	strcpy(rootRec.name, ".");

	root.uid = 0;
	root.gid = 0;
	root.size = 0;
	root.numBlocks = 1;
	root.blockNums[0] = 5;
	root.numRecords = 1;
	root.type = 2; // 2 for directory


	FILE *fsp = fopen("M", "wb+");
	fwrite(&spBlk, sizeof(struct superblock), 1, fsp);
	for(int i=0; i< NUM_INODES; i++){
		ibmap.flag[i] = 0;
	}
	for(int i=0; i< NUM_DATA_BLOCKS; i++){
		dbmap.flag[i] = 0;
	}
	ibmap.flag[0] = 1;
	dbmap.flag[0] = 1;
	fwrite(&ibmap, sizeof(struct inodeBit), 1, fsp);
	fwrite(&dbmap, sizeof(struct dataBit), 1, fsp);
	fwrite(&root, sizeof(struct diskInode), 1, fsp);
	fseek(fsp, BLOCK_SIZE*5, SEEK_SET);
	fwrite(&rootRec, sizeof(struct dirRecord), 1, fsp);
	return 0;
}
