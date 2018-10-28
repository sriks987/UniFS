#define FUSE_USE_VERSION 26
#include<fuse.h>
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
#define BLOCK_SIZE 4096
#define NUM_INODES 256
#define NUM_BLOCKS 256
#define MAX_NAME_LEN 10
#define NUM_DIRECT 12

// Defining the file system structure

struct superblock{
	unsigned long int fsSize;
	unsigned int numFreeBlocks, numFreeInodes, inodeBMap, blockBMap, root;
};

struct diskInode{
	struct timespec lastModified, lastAccessed, inodeModified;
	unsigned int uid, gid;
	unsigned long int size;
	unsigned int blockNums[NUM_DIRECT];
	unsigned char type;	// Type to denote whether it is a regularfile, directory etc.
};

struct memInode{
	struct diskInode dNode;
	unsigned char flagInode; // Flag for modified Inode
	unsigned char flagBlock; // Flag for modified block 
};

struct inodeBit{
	unsigned char flag[NUM_INODES];
};

struct dataBit{
	unsigned char flag[NUM_BLOCKS];
};

// System call definitions 



