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
int numOfDirents = BLOCK_SIZE/sizeof(struct dirent);

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
	set_bitmap(b,i); //= 1; //mark as used
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
	set_bitmap(b,i); //= 1; //mark as used
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

	void* buf = malloc(BLOCK_SIZE);
  // Step 3: Read the block from disk and then copy into inode structure
	bio_read(onDiskBlockNo,buf); //disk to struct
	struct inode* listOfInodes = (struct inode*)buf;
	*inode = listOfInodes[offset];
	free(buf);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	uint16_t onDiskBlockNo = (sBlock->i_start_blk);
	
	// Step 2: Get the offset in the block where this inode resides on disk
	uint16_t offset = ino % inodesPerBlock;

	void* buf = malloc(BLOCK_SIZE);
	// Step 3: Write inode to disk 
	bio_read(onDiskBlockNo,buf); //struct to disk

	struct inode* listOfInodes = (struct inode*)buf;

	listOfInodes[offset] = *(inode);
	bio_write(onDiskBlockNo,(void*)listOfInodes);
	free(buf);

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	int status = -1;
  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode myNode;
	readi(ino,&myNode);
  // Step 2: Get data block of current directory from inode
	void* buf = malloc(BLOCK_SIZE); //create a buffer

	int i;
	// Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure'
	for(i=0;i<16;i++){ //Iterate through the List of pointers
		const int myBlockNumber = sBlock->d_start_blk+myNode.direct_ptr[i]; 
		if(myNode.direct_ptr[i] != -1){ //If valid...
			bio_read(myBlockNumber,buf); //...read into the buffer
			struct dirent* listOfDirents = (struct dirent*) buf; //Make a list of DIRECTORY ENTRIES. This is within direct_ptr[i].
			//New FOR loop:
			int j;

			for(j=0;j<numOfDirents;j++){ //For everything in the directory...
				if((listOfDirents[j].valid == 1) && (strcmp(fname,listOfDirents[j].name) == 0)){ //if valid and both have same name...
					*dirent = listOfDirents[j]; // let our dirent pointer be that entry
					status = 1;
				} //...and set status flag to 1 to indicate success.
			}
		}
	}
	free(buf);
	return status;
	
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	enum statuses {NEW_ENTRY = 1, ALREADY_EXISTS=0};
	int status;
	void* myBuf = malloc(BLOCK_SIZE);
	int i;
	for(i=0;i<16;i++){
		const int myBlockNumber = sBlock->d_start_blk+dir_inode.direct_ptr[i]; 
		if(dir_inode.valid != 1){ 
			bio_read(myBlockNumber,myBuf); //read into buffer
			struct dirent* listOfDirents = (struct dirent*) myBuf; //make list of DIR ENTRIES in direct.ptr[i]

			int j; int numOfDirents = BLOCK_SIZE/sizeof(struct dirent);
			for(j=0;j<numOfDirents;j++){ //For each directory entry...
				// Step 2: Check if fname (directory name) is already used in other entries
				if( (listOfDirents[j].valid==1) && (strcmp(fname,listOfDirents[j].name) == 0) ){
					// Step 3: Add directory entry in dir_inode's data block and write to disk
					status = ALREADY_EXISTS; //for the retval
					dir_inode.direct_ptr[j] = listOfDirents[j].ino;//already exists. add to dir.inode's block
					}
					
			}
			//else, file not found
				// Step 4: Allocate a new data block for this directory if it does not exist
				status = NEW_ENTRY;
				const int inodeBlock = sBlock->d_start_blk + dir_inode.direct_ptr[j];
				// Update directory inode
				bio_read(inodeBlock,myBuf);
				// Create new dir entry
				struct dirent* newDirEntry = (struct dirent*)malloc(sizeof(struct dirent));

				/*
				// Check and truncate fname if needed
				const char newName[252];
				if(name_len <= 252){
					*newDirEntry->name = fname;}
				else{
					newName = fname;
					memset(newName,'\0',252);
					*newDirEntry->name = newName;
					}
				*/
				memset(newDirEntry,'\0',252);
				strcat(newDirEntry->name,fname);
				newDirEntry->valid = 1;
				newDirEntry->ino = f_ino;
				//dir_inode.direct_ptr[get_avail_ino()] = (struct dirent*) newDirEntry.ino;
				// Write directory entry
				listOfDirents[j] = *newDirEntry;
				bio_write(inodeBlock,(void*) listOfDirents);
				free(myBuf);
		}

	}

	


	


	return status;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	int i;
	for (i = 0; i < 16; i++) {
		if (dir_inode.direct_ptr[i] != -1) { //not invalid
			void* buf = malloc(BLOCK_SIZE);
			bio_read(sBlock->d_start_blk+dir_inode.direct_ptr[i], buf);
			struct dirent* listOfDirents = (struct dirent*)buf;
			for (int j = 0; j < numOfDirents; j++) {
				// Step 2: Check if fname exist
				if (listOfDirents[j].valid == 1 && (strcmp(fname,listOfDirents[j].name) == 0)) { //valid bit and file name we are searching for
					// Step 3: If exist, then remove it from dir_inode's data block and write to disk
					struct dirent* emptyDir = (struct dirent*)malloc(sizeof(struct dirent));
					memset(emptyDir,0,sizeof(struct dirent));
					emptyDir->ino = 0;
					emptyDir->valid = 0; //new dir is no longer valid because it is empty
					bio_read(sBlock->d_start_blk+dir_inode.direct_ptr[i],buf);
					struct dirent* dirList = (struct dirent*)buf;
					dirList[j] = *emptyDir;
					bio_write(sBlock->d_start_blk+dir_inode.direct_ptr[i],(void*)dirList); //write new dir list back to block with empty directory
					free(buf);
					free(emptyDir);
				}
			}
		}
	}

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

/*
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
	
*/
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
//const char *path, uint16_t ino, struct inode *inode)
	int NOT_FOUND = -1;
	// Step 1: call get_node_by_path() to get inode from path
	struct inode *resNode = (struct inode*)malloc(struct inode);

	int result = get_node_by_path(path,stbuf->st_ino,resNode);
	// Step 2: fill attribute of file into stbuf from inode

	if (result == NOT_FOUND) {
		return NOT_FOUND;
	}

	if (resNode)

	stbuf->st_mode   = S_IFDIR | 0755;
	stbuf->st_nlink  = 2;
	time(&stbuf->st_mtime);

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	int status = -1;
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* node = (struct inode *)malloc(sizeof(struct inode));
	// Step 2: If not find, return -1
	if (get_node_by_path(path,0,node) == 0) { // 0 is root inode number
		status = 1;
	}
	free(node);

	return status;
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

	int status = -1;
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* node = (struct inode *)malloc(sizeof(struct inode));
	// Step 2: If not find, return -1
	if (get_node_by_path(path,0,node) == 0) { // 0 is root inode number
		status = 1;
	}
	free(node);

	return status;
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

