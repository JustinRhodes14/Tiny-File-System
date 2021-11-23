/*
 *  Copyright (C) 2019 CS416 Spring 2019
 *	
 *	Tiny File System
 *
 *	File:	tfs.c
 *  Author: Yujie REN
 *	Date:	April 2019
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

struct superblock* sBlock;
bitmap_t inode_bits;
bitmap_t data_bits;
uint16_t inodesPerBlock = BLOCK_SIZE/sizeof(struct inode);


/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	bitmap_t b = inode_bits;
	
	// Step 2: Traverse inode bitmap to find an available slot
	int i;
	while(get_bitmap(b,i)!=0){ //while ith bit of bitmap b is unfree...

		i++; // ...iterate
	}
		//after WHILE, should be at first available free inode
	
	// Step 3: Update inode bitmap and write to disk 
	set_bitmap(b,i) = 1; //mark as used
	bio_write(sBlock->i_bitmap_blk,b); //write to disk (superblock.data_bitmap)

	return i;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	bitmap_t b = data_bits;

	// Step 2: Traverse data block bitmap to find an available slot
	int i;
	while(get_bitmap(b,i)!=0){
		i++;
	}
	// Step 3: Update data block bitmap and write to disk 
	set_bitmap(b,i) = 1; //mark as used
	bio_write(sBlock->d_bitmap_blk,b); //write to superblock.data_bitmap
	return i;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
	uint16_t onDiskBlockNo = (sBlock->i_start_blk); //starting block + ino * inodes-per-block

  // Step 2: Get offset of the inode in the inode on-disk block
	uint16_t offset = ino % inodesPerBlock;

  // Step 3: Read the block from disk and then copy into inode structure
	bio_read(onDiskBlockNo+offset,*inode); //disk to struct
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	uint16_t onDiskBlockNo = (sBlock->i_start_blk)
	
	// Step 2: Get the offset in the block where this inode resides on disk
	uint16_t offset = ino % inodesPerBlock;

	// Step 3: Write inode to disk 
	bio_write(onDiskBlockNo+offset,*inode); //struct to disk
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	int status = -1;
  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode *myNode;
	readi(ino,*myNode);
  // Step 2: Get data block of current directory from inode
	void* buf = malloc(BLOCK_SIZE); //create a buffer

	int i;
	for(i=0;i<16;i++){ //Iterate through the List of pointers
		const int myBlockNumber = sBlock->d_start_blk+myNode.direct_ptr[i]; 
		if(myNode.direct_ptr[i] != -1){ //If valid...
			bio_read(myBlockNumber,buf); //...read into the buffer
			struct dirent* listOfDirents = (struct dirent*) buf; //Make a list of DIRECTORY ENTRIES. This is within direct_ptr[i].
			//New FOR loop:
			int j;
			numOfDirents = BLOCK_SIZE/sizeof(struct dirent);

			for(j=0;j<numOfDirents;j++){ //For everything in the directory...
				if((listOfDirents[j].valid == 1) && (strcmp(fname,listOfDirents[j].name) == 0)){ //if valid and both have same name...
					*dirent = listOfDirents[j]; // let our dirent pointer be that entry
					status = 1;} //...and set status flag to 1 to indicate success.
			}
	}
	free(buf);
	return status;


  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure'
	
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	return 0;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile

	// write superblock information

	// initialize inode bitmap

	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory

	dev_init(diskfile_path);

	sBlock = (struct superblock*)malloc(BLOCK_SIZE);
	inode_bits = (bitmap_t)malloc(BLOCK_SIZE);
	data_bits = (bitmap_t)malloc(BLOCK_SIZE);
	memset(inode_bits,0,BLOCK_SIZE);
	memset(data_bits,0,BLOCK_SIZE);

	int i;
	for (i = 1; i < MAX_INUM; i++) { //start at 1 for root inode
		struct inode* node = malloc(sizeof(struct inode));
		memset(node,0,sizeof(struct inode));
		node->valid = 0; //0 for invalid

		int j;
		for (j = 0; j < 16; j++) {
			node->direct_ptr[j] = 0;
		}

		for (j = 0; j < 8; j++) {
			node->indirect_ptr[j] = 0;
		}
		node->ino = i;

		writei(i,node);
		free(node);
	}

	sBlock->magic_num = MAGIC_NUM;
	sBlock->max_inum = MAX_INUM;
	sBlock->max_dnum = MAX_DNUM;
	sBlock->i_bitmap_blk = 1; //starting address for block
	sBlock->d_bitmap_blk = sBlock->i_bitmap_blk+1;
	sBlock->i_start_blk = sBlock->d_bitmap_blk+1;
	sBlock->d_start_blk = sBlock->i_start_blk+129; //128 inode blocks + 1 (starting block)
	bio_write(0,(void*)sBlock);
	set_bitmap(inode_bits,0);
	bio_write(sBlock->i_bitmap_blk,(void*)inode_bits);
	bio_write(sBlock->d_bitmap_blk,(void*)data_bits);

	struct inode* root = malloc(sizeof(struct inode));
	memset(root,0,sizeof(struct inode));
	root->ino = 0;
	root->type = FOLDER;
	root->valid = 1;
	root->size = 0;
	root->link = 2 // 2 links because current points to the inode

	int g;
	for (g = 0; g < 16; g++) {
		node->direct_ptr[j] = 0;
	}

	for (g = 0; g < 8; g++) {
		node->indirect_ptr[j] = 0;
	}

	time(&(root->vstat.st_atime));
	time(&(root->vstat.st_mtime));
	time(&(root->vstat.st_ctime));

	int avail_block = get_avail_blkno;
	set_bitmap(data_bits,avail_block);
	bio_write(sBlock->d_bitmap_blk,(void*)data_bits);
	root->direct_ptr[0] = avail_block;
	//somewhat confused, come back
	writei(0,root);
	dir_add(*root,root,".",2);
	free(root);
	

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

	int fd = dev_open(diskfile_path); //open diskfile, to be read into superblock

	if (fd == -1) {
		tfs_mkfs();
	}

	sBlock = (struct superblock*)malloc(BLOCK_SIZE);
	inode_bits = (bitmap_t)malloc(BLOCK_SIZE); 
	data_bits = (bitmap_t)malloc(BLOCK_SIZE);
	bio_read(0,sBlock); //read diskfile info



	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

	free(sBlock);
	free(inode_bits);
	free(data_bits);

	dev_close();

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

