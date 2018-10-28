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

// For the flags 0 - not occupied, 1 - occupied

struct inodeBit{
	unsigned char flag[NUM_INODES];
};

struct dataBit{
	unsigned char flag[NUM_BLOCKS];
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


