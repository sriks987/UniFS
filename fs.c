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
#define MAX_DIR_DEPTH 16
#define DIR_ENTRIES_BLOCK (BLOCK_SIZE/sizeof(struct dirRecord))

const int inodeStartBlk = 4;
const int dataStartBlk = 8;

FILE *fsp = NULL;
struct memSuperblock spBlk;
struct memInode pwdNode;


/*
struct stat {
 	unsigned long   st_dev;         
        unsigned long   st_ino;         
	unsigned int    st_mode;        
	unsigned int    st_nlink;       
	unsigned int    st_uid;         
	unsigned int    st_gid;         
	unsigned long   st_rdev;        
	unsigned long   __pad1;
	long            st_size;        
	int             st_blksize;     
	int             __pad2;
	long            st_blocks;      
	long            st_atime;       
	unsigned long   st_atime_nsec;
	long            st_mtime;       
	unsigned long   st_mtime_nsec;
	long            st_ctime;       
	unsigned long   st_ctime_nsec;
	unsigned int    __unused4;
	unsigned int    __unused5;
};
*/

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
	unsigned int uid;
	unsigned int gid;
	unsigned int n_link;
	unsigned int mode;	// to specify the file mode
	unsigned int blockNums[NUM_DIRECT];
	unsigned int numRecords; // Required for directories
	int blockSize;
	long size;
	long numBlocks;
	int            aTime;       /* Time of last access.  */
	unsigned int   aTimeNsec;
	int            mTime;       /* Time of last modification.  */
	unsigned int   mTimeNsec;
	int            cTime;       /* Time of last status change.  */
	unsigned int   cTimeNsec;
	unsigned int	 type;	// Type to denote whether it is a regularfile, directory etc.
	int filler[5];		// Filler for padding
};

struct memInode{
	struct diskInode dNode;
	unsigned int inodeNum;	// This field contains the inode number
	unsigned int refCount;	// Reference count
	unsigned char flagInode; // Flag for modified Inode
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

// Superblock functions

static int syncSuper(struct memSuperblock *spBlk);

// Free Block functions

static unsigned int getFreeInode();	// Function to get a free Inode 
static unsigned int getFreeData();	// Function to get a free Data block
static int releaseData(unsigned int *blockList, int len);
static int releaseInode(struct memInode *currNode);	

// Inode functions

static int getDiskNode(unsigned int inodeNum, struct memInode *currNode);
static int getPathNode(const char *path, struct memInode *currNode);
static int initMemInode(struct memInode *currNode, struct diskInode dNode, unsigned int inodeNum);
static int initDiskInode(unsigned int type, struct memInode *currNode);	// To initialize a newly allocated inode 
static int swapNode(unsigned int inodeNum, struct memInode *currNode);	// To swap the current inode in memory for another inode in the disk
static int diskSync(struct memInode *currNode);				// Function to sync the in-memory inode to disk
static int unlinkInode(unsigned int inodeNum);
#ifdef DEBUG
static int inodeDetails(struct memInode currNode){
	printf("Inode details\nuid: %u\ngid: %u\nn_link: %u\nmode: %u\n",currNode.dNode.uid, currNode.dNode.gid, currNode.dNode.n_link, currNode.dNode.mode);
	for(int i = 0; i< NUM_DIRECT; i++){
		printf("blockNums[%d]: %u\n", i, currNode.dNode.blockNums[i]);
	}
	printf("numRecords: %u\nblockSize: %d\nsize: %ld\nnumBlocks: %ld\naTime: %d\naTimeNsec: %u\nmTime: %d\nmTimeNsec: %u\ncTime: %d\ncTimeNsec: %u\ntype: %u\n", currNode.dNode.numRecords, currNode.dNode.blockSize, currNode.dNode.size, currNode.dNode.numBlocks, currNode.dNode.aTime, currNode.dNode.aTimeNsec, currNode.dNode.mTime, currNode.dNode.mTimeNsec, currNode.dNode.cTime, currNode.dNode.cTimeNsec, currNode.dNode.type);
	return 1;
}
#endif

// Directory functions

static int getNames(const char *path, unsigned int *countNames, char names[][MAX_NAME_LEN]);	// To get the names in a path
static int addName(char *name, unsigned int inodeNum, struct memInode *currNode);
static unsigned int searchDir(char *name, struct memInode currNode);	// Function to search a directory and return the inode number
static int getDirRecs(struct memInode currNode, struct dirRecord *list);
static int delName(char *name, struct memInode *currNode);
static int searchPath(const char *path);

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

#ifdef DEBUG
int dummyfn() {
    return 0;
}
#endif

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

// Superblock functions

static int syncSuper(struct memSuperblock *spBlk){
	putBlock(1, &(spBlk->dSblk));
	spBlk->flagSB = 0;
	return 1;
}

// Free block functions


static unsigned int getFreeInode(){
	struct inodeBit bitMap;
	unsigned int inodeNum;
	if(spBlk.dSblk.numFreeInodes==0){
		printf("\ngetFreeInode: No free inodes");
		return 0;
	}
	getBlock(spBlk.dSblk.inodeBMap, &bitMap);
	for(unsigned int i=0; i< NUM_INODES; i++){
		if(bitMap.flag[i]==0){
			bitMap.flag[i]=1;
			inodeNum = i+1;
			spBlk.dSblk.numFreeInodes--;
			spBlk.flagSB = 1;
			break;
		}
	}
#ifdef DEBUG
	printf("getFreeInode: Returned inodenumber: %d\n", inodeNum);
#endif
	putBlock(spBlk.dSblk.inodeBMap, &bitMap);
	syncSuper(&spBlk);
	return inodeNum;
}

static unsigned int getFreeData(){
	struct dataBit bitMap;
	unsigned int blockNum;
	if(spBlk.dSblk.numFreeData==0){
		printf("\ngetFreeData: No free data Blocks\n");
		return 0;
	}
	getBlock(spBlk.dSblk.dataBMap, &bitMap);
	for(unsigned int i=0; i< NUM_DATA_BLOCKS; i++){
		if(bitMap.flag[i]==0){
			bitMap.flag[i]=1;
			blockNum = i + dataStartBlk;	
			spBlk.dSblk.numFreeData--;
			spBlk.flagSB=1;
			break;
		}
	}
#ifdef DEBUG
	printf("getFreeData: Returned blocknumber: %d\n", blockNum);
#endif
	putBlock(spBlk.dSblk.dataBMap, &bitMap);
	syncSuper(&spBlk);
	return blockNum;
}

static int releaseData(unsigned int *blockList, int len){
	struct dataBit bitMap;
	unsigned int blockNum;
	getBlock(spBlk.dSblk.dataBMap, &bitMap);
	for(unsigned int i=0; i<len; i++){
		bitMap.flag[blockList[i]-1] = 0;
	}
	spBlk.dSblk.numFreeData += len;
	spBlk.flagSB = 1;
	putBlock(spBlk.dSblk.dataBMap, &bitMap);
	syncSuper(&spBlk);
	return 1;
}

static int releaseInode(struct memInode *currNode){
	int numBlocks = currNode->dNode.numBlocks;
	struct inodeBit bitMap;
	unsigned int *blockList = malloc(sizeof(int)*numBlocks);
	for(int i=0;i<numBlocks; i++){
		blockList[i] = currNode->dNode.blockNums[i];
	}
	releaseData(blockList, numBlocks);
	free(blockList);
	getBlock(spBlk.dSblk.inodeBMap, &bitMap);
	bitMap.flag[currNode->inodeNum - 1] = 0;
	putBlock(spBlk.dSblk.inodeBMap, &bitMap);
	syncSuper(&spBlk);
	return 1;
}

// Inode functions

static int swapNode(unsigned int inodeNum, struct memInode *currNode){
	diskSync(currNode);
	getDiskNode(inodeNum, currNode);
	return 1;
}

static int diskSync(struct memInode *currNode){
	unsigned int blockNum, offset;
	struct diskInode inodeArr[INODES_BLOCK];
	blockNum = ((currNode->inodeNum-1) / INODES_BLOCK) + inodeStartBlk;
	offset = (currNode->inodeNum-1) % INODES_BLOCK;
#ifdef DEBUG
	printf("diskSync: inodeNum: %u\tblockNum: %u\toffset: %u\n", currNode->inodeNum, blockNum, offset);
#endif
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
	return 1;
}

static int initDiskInode(unsigned int type, struct memInode *currNode){	
	struct timespec currTime;
	clock_gettime(CLOCK_REALTIME, &currTime);
	currNode->dNode.uid = getuid();
	currNode->dNode.gid = getgid();
	currNode->dNode.n_link = 1;
	if(type==2){
		currNode->dNode.mode = S_IFDIR | 0766;
	}
	else{
		currNode->dNode.mode = S_IFREG | 0777;
	}
	currNode->dNode.numRecords = 0;
	currNode->dNode.blockSize = BLOCK_SIZE;
	currNode->dNode.size = 0;
	currNode->dNode.numBlocks = 0;
	currNode->dNode.aTime = currTime.tv_sec;
	currNode->dNode.aTimeNsec = currTime.tv_nsec;
	currNode->dNode.mTime = currTime.tv_sec;
	currNode->dNode.mTimeNsec = currTime.tv_nsec;
	currNode->dNode.cTime = currTime.tv_sec;
	currNode->dNode.cTimeNsec = currTime.tv_nsec;
	currNode->dNode.type = type;
#ifdef DEBUG
	inodeDetails(*currNode);
#endif
	return 1;
}

static int getDiskNode(unsigned int inodeNum, struct memInode *currNode){
	size_t inodeLen;
	struct diskInode tempDiskNode;
	struct diskInode inodeArr[INODES_BLOCK];
	if(inodeNum==0){
		printf("\ngetDiskNode: Invalid inode number");
		return 0;
	}
	unsigned int blockNum = (inodeNum-1)/(INODES_BLOCK) + inodeStartBlk;
	unsigned int offset = (inodeNum-1)%INODES_BLOCK;
	getBlock(blockNum, inodeArr);
	tempDiskNode = inodeArr[offset];	
	if(initMemInode(currNode, tempDiskNode, inodeNum)==0){
		printf("\ngetDiskNode: Error in initializing Inode");
		return 0;
	}
	return 1;
}


static int getPathNode(const char *path, struct memInode *currNode){
	char names[MAX_DIR_DEPTH][MAX_NAME_LEN];
	unsigned int countNames, inodeNum;
	struct memInode tempNode;
#ifdef DEBUG
	printf("\ngetPathNode: path: %s\n", path);
#endif
	if(getNames(path, &countNames, names)==0){
		printf("getPathNode: Error parsing path\n");
		return 0;
	}
	printf("getPathNode: countNames: %d\n", countNames);
	printf("getPathNode: Names[%d]: %s\n", countNames-1, names[countNames-1]);
	if(strcmp(names[countNames-1],"/")==0){
		if(getDiskNode(spBlk.dSblk.rootInode, &tempNode)==0){	// Fetching the root inode 
			printf("getPathNode: Failure to get root inode\n");
			return 0;
		}
	}
	else if(strcmp(names[countNames-1],".")==0){
		tempNode = *currNode;
	}
	else if(strcmp(names[countNames-1],"..")==0){
		inodeNum = searchDir(names[0], tempNode);
		if(getDiskNode(inodeNum, &tempNode)==0){
			printf("getPathNode: Failure to get parent inode\n");
			return 0;
		}
	}
	for(int i=countNames-2; i>=0; i--){
		printf("getPathNode: Names[%d]: %s\n", i, names[i]); 
		inodeNum = searchDir(names[i], tempNode);
		if(inodeNum==0){
			printf("getPathNode: Invalid path\n");
			return 0;
		}
		swapNode(inodeNum, &tempNode);	// To get the child node 
	}
	*currNode = tempNode;
	return 1;
}

static int unlinkInode(unsigned int inodeNum){
	struct memInode tempNode;
	getDiskNode(inodeNum, &tempNode);
	tempNode.dNode.n_link--;
	tempNode.flagInode = 1;
	diskSync(&tempNode);
	if(tempNode.dNode.n_link == 0){
		releaseInode(&tempNode);
	}
	return 1;
}

// Directory functions

static int delName(char *name, struct memInode *currNode){
	struct dirRecord recs[DIR_ENTRIES_BLOCK];
	int j=0;
	int flag = 0;
	unsigned int inodeNum = 0;
	unsigned int blockNum = 0;
	for(int i=0;i<NUM_DIRECT && i< currNode->dNode.numBlocks; i++){
		blockNum = currNode->dNode.blockNums[i];
		getBlock(blockNum, recs);		// Reading a block of records
		for(; j < currNode->dNode.numRecords; j++){
			if(strcmp(recs[j % DIR_ENTRIES_BLOCK].name, name)==0){
				inodeNum = recs[j % DIR_ENTRIES_BLOCK].inodeNum;
				recs[j % DIR_ENTRIES_BLOCK].inodeNum = 0;	
				currNode->dNode.numRecords--;
				putBlock(blockNum, recs);
				unlinkInode(inodeNum);	// To release the inode and all the blocks associated with it
				flag=1;
				return 1;
			}
		}
	}
	return 0;

}

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
				k++;
#ifdef DEBUG
				printf("getDirRecs : list[%d] = Name: %s\t inodeNum: %u\n", k-1, buf[j].name, buf[j].inodeNum);
#endif
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
	if(i>=numRecords){
		return 0;
	}
	return inodeNum;
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
		currNode->dNode.numRecords++;
		currNode->dNode.size += sizeof(struct dirRecord);
		putBlock(blockNum, recs);
		flag = 1;
		return 1;
	}
	for(int i=0;i<NUM_DIRECT && i< currNode->dNode.numBlocks; i++){
		blockNum = currNode->dNode.blockNums[i];
		getBlock(blockNum, recs);		// Reading a block of records
		for(; j < currNode->dNode.numRecords; j++){
			if(recs[j % DIR_ENTRIES_BLOCK].inodeNum == 0){
				memset(&recs[j % DIR_ENTRIES_BLOCK], 0, sizeof(struct dirRecord));
				recs[j % DIR_ENTRIES_BLOCK].inodeNum = inodeNum;
				strcpy(recs[j % DIR_ENTRIES_BLOCK].name, name);
				currNode->dNode.numRecords++;
				putBlock(blockNum, recs);
				flag=1;
				return 1;
			}
		}
	}
	if(flag==0){	// If we have to append to the data in the last block
		memset(&recs[j % DIR_ENTRIES_BLOCK], 0, sizeof(struct dirRecord));
		recs[j % DIR_ENTRIES_BLOCK].inodeNum = inodeNum;
		strcpy(recs[j % DIR_ENTRIES_BLOCK].name, name);
		currNode->dNode.numRecords++;
		currNode->dNode.size += sizeof(struct dirRecord);
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
	for(int i=0; i< MAX_DIR_DEPTH; i++)
		names[i][0] = '\0';
	while(strcmp(basename(s), s)!=0){
		strcpy(names[k], basename(s));
		k++;
		strcpy(s, dirname(s));
	}
	strcpy(names[k++], basename(s));
	if(strcmp(names[k-1],"/")!=0 && strcmp(names[k-1], ".")!=0){
		strcpy(names[k], dirname(s));
		k++;
	}
	*countNames = k;
	free(s);
#ifdef DEBUG
	printf("\nNames\n");
	for(int i = 0;i < *countNames; i++){
		printf("%s\n", names[i]);
	}
#endif
	return 1;
}

// Callback definitions

static int mkdir_f(const char *path, mode_t fileMode){
#ifdef DEBUG
	printf("Entered mkdir_f: path: %s\n", path);
#endif
	char pathCopy[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char dirPath[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char dirName[MAX_NAME_LEN];
	unsigned int dirInodeNum;
	struct memInode tempNode, newNode;
	tempNode = pwdNode;
	strcpy(pathCopy, path);
	strcpy(dirName, basename(pathCopy));
	strcpy(dirPath, dirname(pathCopy));
	printf("dirName: %s\n dirPath: %s\n", dirName, dirPath);
	if(getPathNode(dirPath, &tempNode)==0){
		printf("mkdir_f: Failure to get Node\n");
		return -ENOENT;
	}
	if(searchDir(dirName, tempNode)>0){
		printf("mkdir_f: Directory already exists\n");
		return -ENOENT;
	}
	if((dirInodeNum = getFreeInode())==0){
		printf("mkdir_f: No free inodes\n");
		return -1;
	}
	if(getDiskNode(dirInodeNum, &newNode)==0){
		printf("mkdir_f: Invalid inode num\n");
		return -1;
	}
	initDiskInode(2, &newNode);			// Initializing the new inode to a directory
	addName(dirName, dirInodeNum, &tempNode);
	diskSync(&tempNode);
	if(pwdNode.inodeNum == tempNode.inodeNum){	// Making sure all in-memory copies of an inode are consistent 
		pwdNode = tempNode;
	}
	addName(".", dirInodeNum, &newNode);
	addName("..", tempNode.inodeNum, &newNode);
	diskSync(&newNode);	// Writing all the changes to persistent storage
#ifdef DEBUG
	printf("mkdir_f: NewDir details\n");
	printf("inodeNum: %d\tDirname: %s\tblock: %d\n\n", dirInodeNum, dirName, newNode.dNode.blockNums[0]);
#endif
	return 0;
}

static int readdir_f(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
#ifdef DEBUG
	printf("Entered readdir_f: path: %s\n", path);
#endif
	unsigned int numRecords;
	unsigned int inodeNum;
	struct dirRecord *list;
	struct memInode tempNode = pwdNode;

	if(getPathNode(path, &tempNode)==0){
		printf("readdir_f: Invalid path\n");
		return -1;
	}
	if(tempNode.dNode.type!=2){
		printf("readdir_f: Not a directory\n");
		return -1;
	}
	numRecords = tempNode.dNode.numRecords;
	list = malloc(sizeof(struct dirRecord)*(numRecords+1));
	getDirRecs(tempNode, list);
	for(int i=2; i < numRecords; i++){		// Exluding the entires . and ..
		filler(buffer, list[i].name, NULL, 0);
#ifdef DEBUG
		printf ("readdir_f : Copying direntry %s using filler\n", list[i].name);
#endif
	}
#ifdef DEBUG
	dummyfn();
#endif
	free(list);
	return 0;
}

static int getattr_f(const char *path, struct stat *st){
#ifdef DEBUG
	printf("\nEntered getattr_f: path: %s, st: 0x%p\n", path, st);
#endif
	struct memInode tempNode = pwdNode;
	memset(st, 0, sizeof(struct stat));
	if(strcmp(path, "/.xdg-volume-info")==0 || strcmp(path, "/autorun.inf")==0 || strcmp(path, "/.Trash")==0){
		printf("getattr_f: xdg or autorun access\n");
		return -ENOENT;
	}
	if(getPathNode(path, &tempNode)==0){
		printf("getattr_f: Invalid path\n");
		return -ENOENT;
	}
	st->st_uid = tempNode.dNode.uid;
	st->st_gid = tempNode.dNode.gid;
	st->st_ino = tempNode.inodeNum;
	st->st_nlink = tempNode.dNode.n_link; 
	st->st_mode = tempNode.dNode.mode;
	st->st_blksize = BLOCK_SIZE;
	st->st_size = tempNode.dNode.size;
	st->st_blocks = tempNode.dNode.numBlocks;
	st->st_atime = tempNode.dNode.aTime;
	//st->st_atime_nsec = tempNode.dNode.aTimeNsec;
	st->st_mtime = tempNode.dNode.mTime;
	//st->st_mtime_nsec = tempNode.dNode.mTimeNsec;
	st->st_ctime = tempNode.dNode.cTime;
	//st->st_ctime_nsec = tempNode.dNode.cTimeNsec;
#ifdef DEBUG
	printf("Struct stat: \ninodeNum: %u\nst_uid: %u\nst_gid: %u\nst_ino: %u\nst_nlink: %lu\n", tempNode.inodeNum, st->st_uid, st->st_gid, st->st_ino, st->st_nlink);
	printf("\n");
	printf("stat: %x\n", st);
#endif
	return 0;
}


static int rmdir_f(const char *path){
#ifdef DEBUG
	printf("Entered rmdir_f: path: %s", path);
#endif
	struct memInode tempNode;
	if(getPathNode(path, &tempNode)==0){
		printf("\nrmdir_f: Invalid path");
		return -ENOENT;
	}
	if(tempNode.dNode.numRecords>2){
		printf("\nrmdir_f: Directory not empty.");
		return -1;
	}	
	unlink_f(path);
	return 0;
}


static int open_f(const char *path, struct fuse_file_info *fi){
#ifdef DEBUG
	printf("Entered open_f: path: %s\n", path);
#endif
	struct memInode tempNode;
	if(getPathNode(path, &tempNode)==0){
		printf("\nopen_f: Flags = %d", fi->flags);
		printf("\nopen_f: Invalid Path");
		return -ENOENT;
	}
	printf("\nopen_f: Flags = %d", fi->flags);
	return 0;
}

static int create_f(const char *path, mode_t md, struct fuse_file_info *fi){
	char pathCopy[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char filePath[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char fileName[MAX_NAME_LEN];
	unsigned int fileInodeNum;
	struct memInode dirNode, newNode;
#ifdef DEBUG
	printf("Entered create_f: path: %s\n", path);
#endif

	dirNode = pwdNode;
	strcpy(pathCopy, path);
	strcpy(fileName, basename(pathCopy));
	strcpy(filePath, dirname(pathCopy));
	if(getPathNode(filePath, &dirNode)==0){
		printf("\ncreate_f: Failure to get Node");
		return -1;
	}
	if(searchDir(fileName, dirNode)!=0){
		printf("\ncreate_f: File already exists");
		return -1;
	}
	if((fileInodeNum = getFreeInode())==0){
		printf("\ncreate_f: No free inodes");
		return -1;
	}
	if(getDiskNode(fileInodeNum, &newNode)==0){
		printf("\ncreate_f: Invalid inode num: mkdir_f");
		return -1;
	}
	initDiskInode(1, &newNode);
	addName(fileName, fileInodeNum, &dirNode);
	diskSync(&dirNode);
	if(pwdNode.inodeNum == dirNode.inodeNum){	// Making sure all in-memory copies of an inode are consistent 
		pwdNode = dirNode;
	}
	diskSync(&newNode);
#ifdef DEBUG
	printf("Details after file creation\n");
	inodeDetails(newNode);
	printf("create_f: newfile details\nInode number: %d\nfile Name: %s\n", fileInodeNum, fileName);
#endif
	return 0;
}

static int read_f(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
	char *block;
	unsigned int k = 0; 
	int i;
	struct memInode fileNode;
	unsigned int startBlockNum;
	unsigned int endBlockNum;
#ifdef DEBUG
	printf("Entered read_f: path: %s\n", path);
#endif
	startBlockNum = offset/BLOCK_SIZE;
	endBlockNum = (size + offset)/BLOCK_SIZE;
	if(getPathNode(path, &fileNode)==0){
		printf("\nread_f: Invalid Path");
		return -ENOENT;
	}
	block = malloc(BLOCK_SIZE);
	getBlock(fileNode.dNode.blockNums[startBlockNum], block);
	memcpy(buffer, block + offset, BLOCK_SIZE - offset);
	k += BLOCK_SIZE - offset;
	for(i=startBlockNum+1; i < endBlockNum;i++){
		getBlock(fileNode.dNode.blockNums[i], block);
		memcpy(buffer+k, block, BLOCK_SIZE);
		k+= BLOCK_SIZE;
	}
	if(/*(size+offset)%BLOCK_SIZE!=0*/ endBlockNum!= startBlockNum){
		getBlock(fileNode.dNode.blockNums[endBlockNum], block);
		memcpy(buffer+k, block, (size+offset)%BLOCK_SIZE);
	}
	free(block);
	return size;
}

static int write_f(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
	unsigned int k;
	struct memInode fileNode;
	unsigned int startBlockNum, startOffset;
	unsigned int endBlockNum, endOffset;
	char *block = NULL;
#ifdef DEBUG
	printf("\nEntered write_f: path: %s\n", path);
#endif
	if(getPathNode(path, &fileNode)==0){
		printf("\nwrite_f: Invalid Path");
		return -ENOENT;
	}
	startBlockNum = offset/BLOCK_SIZE;
	startOffset = offset % BLOCK_SIZE;
	endBlockNum = (offset + size - 1) / BLOCK_SIZE;
	endOffset = (offset + size - 1) % BLOCK_SIZE;
	if(endBlockNum > (NUM_DIRECT-1)){
		printf("\nwrite_f: File too large");
		return -ENOENT;
	}
	for(int i= fileNode.dNode.numBlocks; i <= endBlockNum; i++){	// To allocate extra blocks if needed
		if((fileNode.dNode.blockNums[i] = getFreeData())==0){
			return -ENOENT;
		}
	}
	block = malloc(BLOCK_SIZE);
	getBlock(fileNode.dNode.blockNums[startBlockNum], block);
	k = (endBlockNum == startBlockNum)? endOffset - startOffset + 1: BLOCK_SIZE - startOffset; // Size to write in the first block
#ifdef DEBUG
	printf("write_f: k: %u\nstartBlockNum: %u\tstartOffset: %u\nendBlockNum: %u\tendOffset: %u\n", k, startBlockNum, startOffset, endBlockNum, endOffset);
#endif
	memcpy(block + startOffset, buffer, k);
	putBlock(fileNode.dNode.blockNums[startBlockNum], block);
	for(int i= startBlockNum + 1; i < endBlockNum; i++){
		getBlock(fileNode.dNode.blockNums[i], block);
		memcpy(block, buffer + k, BLOCK_SIZE);
		putBlock(fileNode.dNode.blockNums[i], block);
		k += BLOCK_SIZE;
	}
	if(endBlockNum != startBlockNum){
		getBlock(fileNode.dNode.blockNums[endBlockNum], block);
		memcpy(block, buffer + k, endOffset);
		putBlock(fileNode.dNode.blockNums[endBlockNum], block);
	}
	if(fileNode.dNode.size <= endOffset){
		fileNode.dNode.size = endOffset + 1; // Changing the file node details 
	}
	free(block);
	diskSync(&fileNode);
#ifdef DEBUG
	printf("write_f: New file size: %d\n", fileNode.dNode.size);
#endif
	return size; 
}

static int unlink_f(const char *path){
	printf("\nEntered unlink_f");
	char pathCopy[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char filePath[MAX_NAME_LEN * MAX_DIR_DEPTH];
	char fileName[MAX_NAME_LEN];
	unsigned int fileInodeNum;
	struct memInode dirNode;
	dirNode = pwdNode;
	strcpy(pathCopy, path);
	strcpy(fileName, basename(pathCopy));
	strcpy(filePath, dirname(pathCopy));
	if(getPathNode(filePath, &dirNode)==0){
		printf("\nunlink_f: Failure to get directory Node");
		return -ENOENT;
	}
	delName(fileName, &dirNode);
	diskSync(&dirNode);
	return 0;
}


int main(int argc, char *argv[]){
	fsp = NULL;
	fsp = fopen("M", "rb+");
	if(fsp == NULL)
		perror("open");
	umask(0);
	getBlock(1, &(spBlk.dSblk));
	spBlk.flagSB = 0;
	printf("\nRoot Inode: %d\n", spBlk.dSblk.rootInode);
	getDiskNode(spBlk.dSblk.rootInode, &pwdNode);
	printf("Structure sizes: ");
//	printf("struct superblock: %lu\nstruct dirRecord: %lu\nstruct memSuperblock: %lu\nstruct diskInode: %lu\nstruct memInode: %lu\nstruct inodeBit: %lu\nstruct dataBit: %lu\n", sizeof(struct superblock), sizeof(struct dirRecord), sizeof(struct memSuperblock), sizeof(struct diskInode), sizeof(struct memInode), sizeof(struct inodeBit), sizeof(struct dataBit));
	fuse_main(argc, argv, &operations, NULL);
	diskSync(&pwdNode);
	fclose(fsp);
	return 0;
}

