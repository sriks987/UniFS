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
#include<errno.h>
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
#define MAX_DIR_DEPTH 16
#define DIR_ENTRIES_BLOCK (BLOCK_SIZE/sizeof(struct dirRecord))

const int inodeStartBlk = 4;
const int dataStartBlk = 6;

FILE *fsp = NULL;
struct memSuperblock spBlk;
struct memInode pwdNode;
struct memInode tempNode;

// Defining the file system structure

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
	unsigned int size, n_link;
	unsigned int mode;	// to specify the file mode
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

// System call declarations

static int mkdir_f(const char *path, mode_t fileMode);
static int readdir_f(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
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
static int putBlock(unsigned int blockNum, void *buffer);	// To write a block of memory

// Free Block functions

static unsigned int getFreeInode();	// Function to get a free Inode 
static unsigned int getFreeData();	// Function to get a free Data block

// Inode functions

static int getDiskNode(unsigned int inodeNum, struct memInode *currNode);
static int getPathNode(const char *path, struct memInode *currNode);
static int initMemInode(struct memInode *currNode, struct diskInode dNode, unsigned int inodeNum);
static int swapNode(unsigned int inodeNum, struct memInode *currNode);	// To swap the current inode in memory for another inode in the disk
static int diskSync(struct memInode *currNode);				// Function to sync the in-memory inode to disk

// Directory functions

static int getNames(const char *path, unsigned int *countNames, char names[][MAX_NAME_LEN]);	// To get the names in a path
static int addName(char *name, unsigned int inodeNum, struct memInode *currNode);
static unsigned int searchDir(char *name, struct memInode currNode);	// Function to search a directory and return the inode number
static int getDirRecs(struct memInode currNode, struct dirRecord *list);


static struct fuse_operations operations = {
	.mkdir = mkdir_f,
	.readdir = readdir_f,
	.getattr = getattr_f/*,
	.rmdir = rmdir_f,
	.open = open_f,
	.create = create_f,
	.write = write_f,
	.read = read_f,
	.unlink = unlink_f
	*/
};

// Function definitions

// Block functions

static int getBlock(unsigned int blockNum, void *buffer){
	fseek(fsp, (blockNum-1)*BLOCK_SIZE, SEEK_SET);
	fread((char*)buffer, BLOCK_SIZE, 1, fsp);
	return 1;
}

static int putBlock(unsigned int blockNum, void *buffer){
	fseek(fsp, (blockNum-1)*BLOCK_SIZE, SEEK_SET);
	fwrite((char*)buffer, BLOCK_SIZE, 1, fsp);
	return 1;
}

// Free block functions


static unsigned int getFreeInode(){
	struct inodeBit bitMap;
	unsigned int inodeNum;
	if(spBlk.dSblk.numFreeInodes==0){
		printf("\nNo free inodes");
		return 0;
	}
	getBlock(spBlk.dSblk.inodeBMap, &bitMap);
	for(unsigned int i=0; i< NUM_INODES; i++){
		if(bitMap.flag[i]==0){
			bitMap.flag[i]=1;
			inodeNum = i+1;
			spBlk.dSblk.numFreeInodes--;
			break;
		}
	}
	putBlock(spBlk.dSblk.inodeBMap, &bitMap);
	return inodeNum;
}

static unsigned int getFreeData(){
	struct dataBit bitMap;
	unsigned int blockNum;
	if(spBlk.dSblk.numFreeData==0){
		printf("\nNo free inodes");
		return 0;
	}
	getBlock(spBlk.dSblk.dataBMap, &bitMap);
	for(unsigned int i=0; i< NUM_DATA_BLOCKS; i++){
		if(bitMap.flag[i]==0){
			bitMap.flag[i]=1;
			blockNum = i+1;
			spBlk.dSblk.numFreeData--;
			break;
		}
	}
	putBlock(spBlk.dSblk.dataBMap, &bitMap);
	return blockNum;
}

static int releaseInode(struct memInode currNode){
	

// Inode functions

static int swapNode(unsigned int inodeNum, struct memInode *currNode){
	diskSync(currNode);
	getDiskNode(inodeNum, currNode);
	return 1;
}

static int diskSync(struct memInode *currNode){
	unsigned int blockNum, offset;
	struct diskInode inodeArr[INODES_BLOCK];
	blockNum = (currNode->inodeNum / INODES_BLOCK) + inodeStartBlk;
	offset = currNode->inodeNum % INODES_BLOCK;
	getBlock(blockNum, inodeArr);
	inodeArr[offset] = currNode->dNode;
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
	struct diskInode inodeArr[INODES_BLOCK];
	if(inodeNum==0){
		printf("\nInvalid inode number: getDiskNode");
		return 0;
	}
	unsigned int blockNum = (inodeNum-1)/(INODES_BLOCK) + inodeStartBlk;
	unsigned int offset = (inodeNum-1)%INODES_BLOCK;
	getBlock(blockNum, inodeArr);
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
	printf("\npath: %s", path);
	if(getNames(path, &countNames, names)==0){
		printf("\nError parsing path");
		return 0;
	}
	printf("\ncountNames: %d \nNames[0]: %s", countNames, names[0]);
	if(names[countNames-1][0]=='/'){
		if(getDiskNode(spBlk.dSblk.rootInode, &tempNode)==0){	// Fetching the root inode 
			printf("\nFailure to get root inode");
			return 0;
		}
	}
	else if(names[countNames-1][0]=='.'){
		tempNode = *currNode;
	}
	for(int i=countNames-2; i>=0; i--){
		printf("\ngetPathNode: Names[%d]: %s", i, names[i]); 
		inodeNum = searchDir(names[i], tempNode);
		if(inodeNum==0){
			printf("\nInvaid path");
			return 0;
		}
		swapNode(inodeNum, &tempNode);	// To get the child node 
	}
	*currNode = tempNode;
	return 1;
}



// Directory functions

static int remove

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
	unsigned int inodeNum;
	int i = 0;
	struct dirRecord *list = malloc(sizeof(struct dirRecord)*(numRecords+1));
	getDirRecs(currNode, list);
	for(i=0; i<numRecords; i++){
		if(strcmp(name, list[i].name)==0){
			inodeNum = list[i].inodeNum;
			break;
		}
	}
	free(list);
	if(i==numRecords){
		return 0;
	}
	return 1;
}

static int addName(char *name, unsigned int inodeNum, struct memInode *currNode){
	struct dirRecord recs[DIR_ENTRIES_BLOCK];
	int j=0;
	int flag = 0;
	unsigned int blockNum = 0;
	if(currNode->dNode.numRecords % DIR_ENTRIES_BLOCK==0){
		blockNum = getFreeData();
		currNode->dNode.blockNums[currNode->dNode.numBlocks] = blockNum;
		currNode->dNode.numBlocks++;
		strcpy(recs[0].name, name);
		recs[0].inodeNum = inodeNum;
		putBlock(blockNum, recs);
		return 1;
	}
	for(int i=0;i<NUM_DIRECT && i< currNode->dNode.numBlocks; i++){
		blockNum = currNode->dNode.blockNums[i];
		getBlock(blockNum, recs);		// Reading a block of records
		for(; j < currNode->dNode.numRecords; j++){
			if(recs[j % DIR_ENTRIES_BLOCK].inodeNum == 0){
				recs[j % DIR_ENTRIES_BLOCK].inodeNum = inodeNum;
				strcpy(recs[j % DIR_ENTRIES_BLOCK].name, name);
				currNode->dNode.numRecords++;
				putBlock(blockNum, recs);
				flag=1;
				return 1;
			}
		}
	}
	if(flag==0){
		recs[j % DIR_ENTRIES_BLOCK].inodeNum = inodeNum;
		strcpy(recs[j % DIR_ENTRIES_BLOCK].name, name);
		currNode->dNode.numRecords++;
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
	while(strcmp(basename(s), s)!=0){
		strcpy(names[k++], basename(s));
		strcpy(s, dirname(s));
	}
	strcpy(names[k++], basename(s));
	if(names[k-1][0]!='/'){
		strcpy(names[k++], dirname(s));
	}
	*countNames = k;
	free(s); 
	return 1;
}

// Callback definitions

static int mkdir_f(const char *path, mode_t fileMode){
	printf("\nEntered mkdir");
	char pathCopy[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char dirPath[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char dirName[MAX_NAME_LEN];
	unsigned int dirInodeNum;
	struct memInode tempNode, newNode;
	tempNode = pwdNode;
	strcpy(dirName, basename(pathCopy));
	strcpy(dirPath, dirname(pathCopy));
	if(getPathNode(dirPath, &tempNode)==0){
		printf("\nFailure to get Node");
		return -1;
	}
	if(searchDir(dirName, tempNode)!=0){
		printf("\nDirectory already exists");
		return -1;
	}
	if((dirInodeNum = getFreeInode())==0){
		printf("\nNo free inodes");
		return -1;
	}
	if(getDiskNode(dirInodeNum, &newNode)==0){
		printf("\nInvalid inode num: mkdir_f");
		return -1;
	}
	addName(dirName, dirInodeNum, &tempNode);
	diskSync(&tempNode);
	if(pwdNode.inodeNum == tempNode.inodeNum){	// Making sure all in-memory copies of an inode are consistent 
		pwdNode = tempNode;
	}
	addName(".", dirInodeNum, &newNode);
	addName("..", tempNode.inodeNum, &newNode);
	diskSync(&newNode);	// Writing all the changes to persistent storage
	return 0;
}

static int readdir_f(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
	printf("\nEntered readdir");
	unsigned int numRecords;
	unsigned int inodeNum;
	struct dirRecord *list = malloc(sizeof(struct dirRecord)*(numRecords+1));
	tempNode = pwdNode;

	if(getPathNode(path, &tempNode)==0){
		printf("\nInvalid path");
		return -1;
	}
	if(tempNode.dNode.type!=2){
		printf("\nNot a directory");
		return -1;
	}
	numRecords = tempNode.dNode.numRecords;
	getDirRecs(tempNode, list);
	for(int i=0; i < numRecords; i++){
		filler(buffer, list[i].name, NULL, 0);
	}
	return 0;
}

static int getattr_f(const char *path, struct stat *st){
	printf("\nEntered getattr_f");
	tempNode = pwdNode;
	if(strcmp(path, "/.xdg-volume-info")==0 || strcmp(path, "/autorun.inf")==0){
		printf("\nxdg or autorun access");
		return -ENOENT;
	}
	if(getPathNode(path, &tempNode)==0){
		printf("\nInvalid path");
		return -ENOENT;
	}
	st->st_nlink = tempNode.dNode.n_link;
	st->st_uid = get_uid();
	st->st_gid = get_gid();
	st0>st_mode = tempNode.dNode.mode;
	return 0;
}

/*
static int rmdir_f(const char *path){
	if(getPathNode(path, &tempNode)==0){
		printf("\nrmdir_f: Invalid path");
		return -ENOENT;
	}
	if(tempNode.dNode.numRecords>2){
		printf("\nrmdir_f: Directory not empty.");
	}	
	
}
*/

static int open_f(const char *path, struct fuse_file_info *fi){
	if(getPathNode(path, &tempNode)==0){
		printf("\nopen_f: Invalid Path");
		return ENOENT;
	}
	printf("\nopen_f: Flags = %d", fi->flags);
	return 0;
}

static int create_f(const char *path, mode_t md, struct fuse_file_info *fi);

static int read_f(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
	char block[BLOCK_SIZE];
	unsigned int k; 
	struct memInode fileNode;
	unsigned int startBlockNum;
	unsigned int endBlockNum;
	startBlockNum = offset/BLOCK_SIZE;
	endBlockNum = (size + offset)/BLOCK_SIZE;
	if(getPathNode(path, &fileNode)==0){
		printf("\nread_f: Invalid Path");
		return ENOENT;
	}
	getBlock(fileNode.dNode.blockNums[startBlockNum], block);
	memcpy(buffer, block + offset, BLOCK_SIZE - offset);
	k += BLOCK_SIZE - offset;
	for(int i=startBlockNum+1; i <= endBlockNum;i++){
		getBlock(fileNode.dNode.blockNums[i], block);
		memcpy(buffer+k, block, BLOCK_SIZE);
		k+= BLOCK_SIZE;
	}
	if((size+offset)%BLOCK_SIZE!=0){
		getBlock(fileNode.dNode.blockNums[i], block);
		memcpy(buffer+k, block, (size+offset)%BLOCK_SIZE);
	}
	return 0;
}

static int write_f(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
	char block[BLOCK_SIZE];
	unsigned int k;
	struct memInode fileNode;
	unsigned int startBlockNum, startOffset;
	unsigned int endBlockNum, endOffset;
	if(getPathNode(path, &fileNode)==0){
		printf("\nwrite_f: Invalid Path");
		return ENOENT;
	}
	startBlockNum = offset/BLOCK_SIZE;
	startOffset = BLOCK_SIZE - offset%BLOCK_SIZE;
	endBlockNum = (offset + size) / BLOCK_SIZE;
	endOffset = (offset + size) % BLOCK_SIZE;
	// Not complete	

}

static int unlink_f(const char *path){
	printf("\nEntered unlink_f");
	char pathCopy[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char filePath[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char fileName[MAX_NAME_LEN];
	unsigned int fileInodeNum;
	struct memInode dirNode, fileNode;
	dirNode = pwdNode;
	strcpy(fileName, basename(pathCopy));
	strcpy(filePath, dirname(pathCopy));
	if(getPathNode(dirPath, &dirNode)==0){
		printf("\nFailure to get Node");
		return -1;
	}
	delName(fileName, dirNode);
	release(fileNode);
	return 0;
}


int main(int argc, char *argv[]){
	fsp = fopen("M", "rb+");
	if(fsp == NULL)
		perror("open");
	umask(0);
	getBlock(1, &(spBlk.dSblk));
	spBlk.flagSB = 0;
	printf("\nRoot Inode: %d", spBlk.dSblk.rootInode);
	getDiskNode(spBlk.dSblk.rootInode, &pwdNode);
	fuse_main(argc, argv, &operations, NULL);
	diskSync(&pwdNode);
	return 0;
}
