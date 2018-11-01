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
#define BLOCK_SIZE 512
#define NUM_INODES 32
#define NUM_INODE_BLOCKS 2 
#define INODES_BLOCK 8
#define NUM_DATA_BLOCKS 256
#define MAX_NAME_LEN 10
#define NUM_DIRECT 11
#define DISK_INODE_SIZE sizeof(struct diskInode)
#define SB_SIZE sizeof(struct superblock)
#define INODE_BM_SIZE sizeof(struct inodeBit)
#define DATA_BM_SIZE sizeof(struct dataBit)
#define MAX_DIR_DEPTH 20
#define DIR_ENTRIES_BLOCK (BLOCK_SIZE/sizeof(dirRecord))

const int inodeStartAddr = sizeof(struct superblock)+sizeof(inodeBit)+sizeof(dataBit);
const int dataStartAddr = inodeStartAddr + BLOCK_SIZE*NUM_INODE_BLOCKS;

FILE *fsp = NULL;
struct memSuperblock spBlk;
struct memInode currDir;
struct memInode tempNode;
// Let fsp be the filesystem pointer
// Let spBlk be the super block

// Defining the file system structure

struct superblock{		// Block number 1		
	unsigned long int fsSize;
	unsigned int numFreeBlocks, numFreeInodes, inodeBMap, blockBMap, rootInode;
	char filler[484];
};

struct dirRecord{
	unsigned int inodeNum;
	char name[MAX_NAME_LEN];
}

struct memSuperblock{
	struct superblock dSblk;
	unsigned int flagSB; // Flag for modified superblock
};

struct diskInode{
	//struct timespec lastModified, lastAccessed, inodeModified;
	unsigned int uid, gid;
	unsigned int size;
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
};

struct dataBit{
	unsigned char flag[NUM_DATA_BLOCKS];
};

// System call declarations

static int mkdir_f(const char *path, mode_t fileMode);
static int readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int getattr_f(const char *path, struct stat *st);
static int rmdir_f(const char *path);
static int open_f(const char *path, struct fuse_file_info *fi);
static int create_f(const char *path, mode_t md, struct fuse_file_info *fi);
static int read_f(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
static int write_f(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
static int unlink_f(const char *path);

// Other functions needed

// Block functions 

static int getBlock(unsigned int blockNum, void *buffer);	// To get a block of memory
static int putBlock(unsigned int blockNum, const void *buffer);	// To write a block of memory

// Free Block functions

static unsigned int getFreeInode();	// Function to get a free Inode 
static unsigned int getFreeData();	// Function to get a free Data block

// Inode functions

static int getDiskNode(unsigned int inodeNum, struct memInode *currNode);
static int getPathNode(const char *path, struct memInode *currNode);
static int initMemInode(struct memInode *currNode, struct diskInode dNode);
static int swapNode(unsigned int inodeNum, struct memInode *currNode);	// To swap the current inode in memory for another inode in the disk
static int diskSync(struct memInode *currNode);				// Function to sync the in-memory inode to disk

// Directory functions

static int getNames(const char *path, char names[][MAX_NAME_LEN]);	// To get the names in a path
static int addName(char *name, unsigned int inodeNum, struct memInode *currNode);
static unsigned int searchDir(char *name, struct memInode currNode);	// Function to search a directory
static int getDirRecs(struct memInode currNode, struct dirRecord *list);


static struct fuse_operations operations = {
	.mkdir = mkdir_f,
	.readdir = readdir_f,
	.getattr = getattr_f,
	.rmdir = rmdir_f,
	.open = open_f,
	.create = create_f,
	.write = write_f,
	.read = read_f,
	.unlink = unlink_f
};

// Function definitions

// Block functions

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

// Free block functions


static unsigned int getFreeInode(){
	struct inodeBit bitMap;
	unsigned int inodeNum;
	if(spBlk.numFreeInodes==0){
		printf("\nNo free inodes");
		return 0;
	}
	getBlock(spBlk.dSblk.inodeBit, &bitMap);
	for(unsigned int i=0; i< NUM_INODES; i++){
		if(bitMap.flag[i]==0){
			bitMap.flag[i]=1;
			inodeNum = i+1;
			spBlk.dSblk.numFreeInodes--;
			break;
		}
	}
	putBlock(spBlk.dSblk.inodeBit, &bitMap);
	return inodeNum;
}

static unsigned int getFreeData(){
	struct dataBit bitMap;
	unsigned int blockNum;
	if(spBlk.dSblk.numFreeData==0){
		printf("\nNo free inodes");
		return 0;
	}
	getBlock(spBlk.dSblk.dataBit, &bitMap);
	for(unsigned int i=0; i< NUM_DATA_BLOCKS; i++){
		if(bitMap.flag[i]==0){
			bitMap.flag[i]=1;
			blockNum = i+1;
			spBlk.dSblk.numFreeData--;
			break;
		}
	}
	putBlock(spBlk.dSblk.dataBit, &bitMap);
	return blockNum;
}

// Inode functions

static int swapNode(unsigned int inodeNum, struct *currNode){
	diskSync(currNode);
	getDiskNode(inodeNum, currNode);
	return 1;
}

static int diskSync(struct memInode *currNode){
	unsigned int blockNum, offset;
	struct inodeArr[INODES_BLOCK];
	blockNum = (currNode->inodeNum/INODES_BLOCK) + inodeStartBlk;
	offset = (currNode-<inodeNum%INODES_BLOCK;
	getBlock(blockNum, inodeArr);
	inodeArr[offset] = currNode->dnode;
	putBlock(blockNum, inodeArr);
	currNode->flagInode = 0;
	return 1;
}

static int initMemInode(struct memInode *currNode, struct diskInode dNode, unsigned int inodeNum){
	currNode->dNode = dNode;
	currNode->inodeNum = inodeNum;
	currNode->refCount = 1;		// As it is being reference by the current process
	currNode->flagInode = 0;
	currNode->flagFile = 0;
}

static int getDiskNode(unsigned int inodeNum, struct memInode *currNode){
	size_t inodeLen;
	struct diskInode tempDiskNode;
	struct inodeArr[INODES_BLOCK];
	if(inodeNum==0){
		printf("\nInvalid inode number");
		return 0;
	}
	unsigned int blockNum = (inodeNum-1)/(INODES_BLOCK) + inodeStartBlk;
	unsigned int offset = (inodeNum-1)%INODES_BLOCK;
	getBlock(blockNum, *inodeArr);
	tempDiskNode = inodeArr[offset];	
	if(initMemInode(currNode, tempDiskNode, inodeNum)==0){
		printf("\nError in initializing Inode");
		return 0;
	}
	return 1;
}


static int getPathNode(const char *path, struct memInode *currNode){
	char names[MAX_DIR_DEPTH][MAX_NAME_LEN];
	unsigned int countNames, inodeNum;
	struct memInode tempNode; 
	if(getNames(path, countNames, names)==0){
		printf("\nError parsing path");
		return 0;
	}	
	if(names[countNames-1][0]=='/'){
		if(getDiskNode(mSblk.dSblk.rootInode, &tempNode)==0){	// Fetching the root inode 
			printf("\nFailure to get root inode");
			return 0;
		}
	}
	else if(names[countNames-1][0]=='.'){
		tempNode = *currNode;
	}
	for(int i=countNames-2; i>=0; i++){
		inodeNum = searchDir(names[i], &tempNode);
		if(inodeNum==0){
			printf("\nInvaid path");
			return 0;
		}
		swapNode(inodeNum, tempNode);	// To get the child node 
	}
	*currNode = tempNode;
	return 1;
}



// Directory functions

static int getDirRecs(struct memInode currNode, struct dirRecord *list){
	struct dirRecord buf[DIR_ENTRIES_BLOCK];
	int numEntries = DIR_ENTRIES_BLOCK;
	int numRecords = currNode.dNode.numRecords;
	int numBlocks = currNode.dNode.numBlocks;
	int k = 0;
	for(int i=0; i<numBlocks; i++){
		getBlock(currNode.dNode.blockNums[i], buf);
		for(int j=0; j< numEntries && k<numRecords; j++){
			if(buf[j].inodeNum!=0){
				list[k] = buf[j];
			}
		}
	}
	return 1;
}

static unsigned int searchDir(char *name, struct memInode currNode){
	unsigned int numRecords = currNode.dNode.numRecords;
	struct dirRecord list[+1];
	getDirRecs(currNode, list);
	for(int i=0; i<numRecords; i++){
		if(strcmp(name, list[i].name)==0){
			return list[i].inodeNum;
		}
	}
	return 0;
}

static int addName(char *name, unsigned int inodeNum, struct memInode *currNode){
	struct dirRecord recs[NUM_ENTRIES_BLOCK];
	int j=0;
	int flag = 0;
	unsigned int blockNum = 0;
	if(numRecords% NUM_ENTRIES_BLOCK==0){
		blockNum = getFreeBlock();
		currNode->dnode.blockNums[currNode->dnode.numBlocks] = blockNum;
		currNode->dnode.numBlocks++;
		strcpy(recs[0].name, name);
		recs[0].inodeNum = inodeNum;
		putBlock(blockNum, recs);
		return 1;
	}
	for(int i=0;i<NUM_DIRECT && i< currNode->numBlocks; i++){
		blockNum = currNode->blockNums[i];
		getBlock(blockNum, recs);		// Reading a block of records
		for(; j < currNode.dNode.numRecords; j++){
			if(recs[j%NUM_ENTRIES_BLOCK].inodeNum == 0){
				recs[j%NUM_ENTRIES_BLOCK].inodeNum = inodeNum;
				strcpy(recs[j%NUM_ENTRIES_BLOCK].name, name);
				currNode.dNode.numRecords++;
				putBlock(blockNum, recs);
				flag=1;
				return 1;
			}
		}
	}
	if(flag==0){
		recs[j%NUM_ENTRIES_BLOCK].inodeNum = inodeNum;
		strcpy(recs[j%NUM_ENTRIES_BLOCK].name, name);
		currNode.dNode.numRecords++;
		putBlock(blockNum, recs);
		flag=1;
		return 1;
	}
	return 0;
}

static int getNames(const char *path, unsigned int *countNames, char names[][MAX_NAME_LEN]){
	int k = 0;
	char *s = malloc(MAX_NAME_LEN*MAX_DIR_DEPTH);
	if(path[0]=='\0')
		return 0;
	strcpy(s, path);
	for(int i=0; i<100; i++)
		names[i][0] = '\0';
	while(strcpy(basename(s), s)!=0){
		strcpy(names[k++], basename(s));
		strcpy(s, dirname(s));
	}
	strcpy(names[k++], basename(s));
	strcpy(names[k++], dirname(s));
	*countNames = k;
	free(s); 
	return 1;
}

// Callback definitions

static int mkdir_f(const char *path, mode_t fileMode){
	char *dirPath[MAX_NAME_LEN*MAX_DIR_DEPTH];
	char *dirName[MAX_NAME_LEN];
	unsigned int dirInodeNum;
	struct memNode tempNode, newNode;
	tempNode = pwdNode;
	strcpy(dirName, basename(path));
	strcpy(dirPath, dirname(path));
	getPathNode(dirPath, &tempNode);
	if(searchDir(dirName, tempNode)!=0){
		printf("\nDirectory already exists");
		return 0;
	}
	if((dirInodeNum = getFreeInode())==0){
		printf("No free inodes");
		return 0;
	}
	getDiskNode(dirInodeNum, newNode);
	addName(dirName, dirInodeNum, &tempNode);
	diskSync(tempNode);
	if(pwdNode.inodeNum == tempNode.inodeNum){	// Making sure all in-memory copies of an inode are consistent 
		pwdNode = tempNode;
	}
	addName(".", dirInodeNum, &newNode);
	addName("..", tempNode.inodeNum, &newNode);
	diskSync(newNode);	// Writing all the changes to persistent storage
	return 1;
}

int main(int argc, char *argv[]){
	fsp = fopen("M", "r+");
	if(fsp == NULL)
		perror("open");
	umask(0);
	getBlock(1, &(spBlk.dSblk));
	spBlk.flag = 0;
	getDiskNode(spBlk.dSblk.root, &currDir);
	fuse_main(argc, argv, &operations, NULL);
	diskSync(currNode);
	return 0;
}
