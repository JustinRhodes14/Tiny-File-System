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
#include <math.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];
enum FILE_STATUS{VALID = 1, NOT_SET = 0, INVALID = -1};

struct superblock* sBlock;
bitmap_t inode_bits;
bitmap_t data_bits;

int inodesPerBlock = (BLOCK_SIZE/sizeof(struct inode));
int numOfDirents = (BLOCK_SIZE/sizeof(struct dirent));

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	bio_read(sBlock->i_bitmap_blk,(void*)inode_bits);
	
	// Step 2: Traverse inode bitmap to find an available slot
	int i = 0;
	while(get_bitmap(inode_bits,i)!=0){ //while ith bit of bitmap b is unfree...
		
		i++; // ...iterate
	}
		//after WHILE, should
	// Step 3: Update inode bitmap and write to disk 
	set_bitmap(inode_bits,i); //= 1; //mark as used
	bio_write(sBlock->i_bitmap_blk,(void*)inode_bits); //write to disk (superblock.data_bitmap)

	return i;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	bio_read(sBlock->d_bitmap_blk,(void*)data_bits);

	// Step 2: Traverse data block bitmap to find an available slot
	int i = 0;
	while(get_bitmap(data_bits,i)!=0){
		i++;
	}
	// Step 3: Update data block bitmap and write to disk 
	set_bitmap(data_bits,i); //= 1; //mark as used
	bio_write(sBlock->d_bitmap_blk,(void*)data_bits); //write to superblock.data_bitmap
	return i;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
	void* buf = malloc(BLOCK_SIZE);
  // Step 1: Get the inode's on-disk block number
	uint16_t onDiskBlockNo = sBlock->i_start_blk + (ino/inodesPerBlock); //starting block + ino * inodes-per-block
  // Step 2: Get offset of the inode in the inode on-disk block
	uint16_t offset = ino % inodesPerBlock;
  // Step 3: Read the block from disk and then copy into inode structure
	bio_read(onDiskBlockNo,buf); //disk to struct
	*inode = ((struct inode*)buf)[offset];
	free(buf);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {
	void* buf = malloc(BLOCK_SIZE);
	// Step 1: Get the block number where this inode resides on disk
	uint16_t onDiskBlockNo = (sBlock->i_start_blk) + ino/inodesPerBlock;
	// Step 2: Get the offset in the block where this inode resides on disk
	uint16_t offset = ino % inodesPerBlock;
	// Step 3: Write inode to disk
	bio_read(onDiskBlockNo,buf); //struct to disk
	((struct inode*)buf)[offset] = *(inode);
	bio_write(onDiskBlockNo,buf);
	free(buf);
	return 0;
}

/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	int status = -1;
  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
	struct inode* myNode = (struct inode*)malloc(sizeof(struct inode));
	void* buf = malloc(BLOCK_SIZE); //create a buffer
	readi(ino,myNode);
	int i;
	for(i=0;i<16;i++){ //Iterate through the List of pointers
	// Step 2: Get data block of current directory from inode
		if(myNode->direct_ptr[i] != INVALID){ //If valid...
		const int myBlockNumber = sBlock->d_start_blk+myNode->direct_ptr[i];
		// Step 3: Read directory's data block and check each directory entry.
			bio_read(myBlockNumber,buf); //...read into the buffer
			struct dirent* listOfDirents = (struct dirent*) buf; //Make a list of DIRECTORY ENTRIES. This is within direct_ptr[i].
			//New FOR loop:
			int j;
			for(j=0;j<numOfDirents;j++){ //For everything in the directory...
			//If the name matches, then copy directory entry to dirent structure'
				if((listOfDirents[j].valid == VALID) && (strcmp(fname,listOfDirents[j].name) == 0)){ //if valid and both have same name...
					*dirent = listOfDirents[j]; // let our dirent pointer be that entry
					status = 0;
					break;
				} //...and set status flag to 1 to indicate success.
			}
			if (status == 0) {
				break;
			}
		}
	}
	free(buf);
	return status;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	enum statuses {NEW_ENTRY = 0, ALREADY_EXISTS=-1};
	int i;
	int j;
	struct dirent* goodDir;
	int loopStatus = 0;
	struct dirent* temp = (struct dirent*)malloc(sizeof(struct dirent));
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	if (dir_find(dir_inode.ino,fname,name_len,temp) == 0) {
		free(temp);
		return ALREADY_EXISTS;
	}
	free(temp);
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	for (i = 0; i < 16; i++) {
		if (dir_inode.direct_ptr[i] != INVALID) {
			const int myBlockNumber = sBlock->d_start_blk+dir_inode.direct_ptr[i]; 
			struct dirent* dirEntry = (struct dirent*)malloc(BLOCK_SIZE);
			bio_read(myBlockNumber,(void*)dirEntry);
			for (j = 0; j < numOfDirents; j++) {
				if (dirEntry[j].valid == NOT_SET) {
					goodDir = dirEntry;
					loopStatus = 1;
					break;
				}
			}
			if (loopStatus == 1) {
				break;
			}
		}
	}
	// Allocate a new data block for this directory if it does not exist
	if (loopStatus == 0) {
		for (i = 0; i < 16; i++) {
			if (dir_inode.direct_ptr[i] == INVALID) {
				break;
			}
		}
		dir_inode.direct_ptr[i] = get_avail_blkno();
		set_bitmap(data_bits,dir_inode.direct_ptr[i]);
		bio_write(sBlock->d_bitmap_blk,data_bits);
		const int blockNo = sBlock->d_start_blk+ dir_inode.direct_ptr[i];
		struct dirent* dirEntry = (struct dirent*)malloc(sizeof(struct dirent));
		memset(dirEntry,0,sizeof(struct dirent));
		bzero(dirEntry->name,252);
		for (i = 0; i < numOfDirents; i++) {
			void* buf = malloc(BLOCK_SIZE);
			bio_read(blockNo,buf);
			struct dirent* dirBuf = (struct dirent*)buf;
			dirBuf[i] = *dirEntry;
			bio_write(blockNo, (void*)dirBuf);
			free(buf);
		}
		writei(dir_inode.ino,&dir_inode);
		// run again with new data block created
		return dir_add(dir_inode,f_ino,fname,name_len);
	}
	// Write directory entry
	goodDir[j].ino = f_ino;
	goodDir[j].valid = VALID;
	strcpy(goodDir[j].name,fname);
	int blockNo = sBlock->d_start_blk + dir_inode.direct_ptr[i];
	bio_write(blockNo,(void*)goodDir);
	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	int i;
	struct dirent* listOfDirents;
	for (i = 0; i < 16; i++) {
		if (dir_inode.direct_ptr[i] != INVALID) { //not invalid
			void* buf = malloc(BLOCK_SIZE);
			const int blockNo = sBlock->d_start_blk+dir_inode.direct_ptr[i];
			bio_read(blockNo, buf);
			listOfDirents = (struct dirent*)buf;
			for (int j = 0; j < numOfDirents; j++) {
				// Step 2: Check if fname exist
				if (listOfDirents[j].valid == VALID && (strcmp(fname,listOfDirents[j].name) == 0)) { //valid bit and file name we are searching for
					// Step 3: If exist, then remove it from dir_inode's data block and write to disk
					struct dirent* emptyDir = (struct dirent*)malloc(sizeof(struct dirent));
					memset(emptyDir,0,sizeof(struct dirent));
					bzero(emptyDir->name,252);
					bio_read(blockNo,buf);
					struct dirent* dirList = (struct dirent*)buf;
					dirList[j] = *emptyDir;
					bio_write(blockNo,(void*)dirList); //write new dir list back to block with empty directory
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
	readi(ino,inode);
	char* tok = strdup(path);
	char* head = strtok_r(tok,"/",&tok);
	if (head == NULL) {
		return inode->valid == VALID ? 0 : -1;
	}
	struct dirent *myDirent = malloc(sizeof(struct dirent));
	if (dir_find(ino,head,strlen(head),myDirent) == -1) {
		free(myDirent);
		return -1; // no inode exists
	}
	
	return get_node_by_path(tok,myDirent->ino,inode);
	// Note: You could either implement it in a iterative way or recursive way
}

/* 
 * Make file system
 */
int tfs_mkfs() {
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	// write superblock information
	// initialize inode bitmap

	sBlock->magic_num = MAGIC_NUM;
	sBlock->max_inum = MAX_INUM;
	sBlock->max_dnum = MAX_DNUM;
	sBlock->i_bitmap_blk = (sizeof(struct superblock)/BLOCK_SIZE) + 1;
	sBlock->d_bitmap_blk = sBlock->i_bitmap_blk + (MAX_INUM/BLOCK_SIZE) + 1;
	sBlock->i_start_blk = sBlock->d_bitmap_blk + (MAX_DNUM/BLOCK_SIZE) + 1;
	sBlock->d_start_blk = sBlock->i_start_blk + (MAX_INUM/inodesPerBlock) + 1; 
	// update bitmap information for root directory
	bio_write(0,(void*)sBlock);
	bio_write(sBlock->i_bitmap_blk,(void*)inode_bits);
	// initialize data block bitmap
	bio_write(sBlock->d_bitmap_blk,(void*)data_bits);
	// update inode for root directory
	int i;
	for (i = 1; i < MAX_INUM; i++) { // preserve zero for root inum
		struct inode* node = (struct inode*)malloc(sizeof(struct inode));
		memset(node,0,sizeof(struct inode));
		node->ino = i;
		int j;
		for (j = 0; j < 16; j++) {
			if (j < 8) {
				node->indirect_ptr[j] = NOT_SET;
			}
			node->direct_ptr[j] = NOT_SET;
		}
		writei(node->ino,node);
		free(node);
	}

	set_bitmap(inode_bits,0); // 0 is for root
	bio_write(sBlock->i_bitmap_blk,(void*)inode_bits);

	struct inode* root = (struct inode*)malloc(sizeof(struct inode));
	root->type = FOLDER;
	root->valid = VALID;
	root->ino = 0;
	root->size = 0;
	root->link = 2;
	for (i = 0; i < 16; i++) {
			if (i < 8) {
				root->indirect_ptr[i] = INVALID;
			}
			root->direct_ptr[i] = INVALID;
	}

	int availNo = get_avail_blkno();
	set_bitmap(data_bits,availNo);
	bio_write(sBlock->d_bitmap_blk,data_bits);
	root->direct_ptr[0] = availNo;
	struct dirent* dirEntry = (struct dirent*)malloc(sizeof(struct dirent));
	memset(dirEntry,0,sizeof(struct dirent));
	bzero(dirEntry->name,252);
	for (i = 0; i < numOfDirents; i++) {
		void* buf = malloc(BLOCK_SIZE);
		const int blockNo = sBlock->d_start_blk+ availNo;
		bio_read(blockNo,buf);
		struct dirent* dirBuf = (struct dirent*)buf;
		dirBuf[i] = *dirEntry;
		bio_write(blockNo, (void*)dirBuf);
		free(buf);
	}
	writei(0,root);
	dir_add(*root,root->ino,".",2);
	free(root);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
	// Step 1a: If disk file is not found, call mkfs

	int fd = dev_open(diskfile_path); //open diskfile, to be read into superblock

	sBlock = (struct superblock*)malloc(BLOCK_SIZE);
	inode_bits = (bitmap_t)malloc(MAX_INUM); 
	data_bits = (bitmap_t)malloc(MAX_DNUM);
	memset(sBlock,0,sizeof(struct superblock));

	if (fd < 0) {
		tfs_mkfs();
	}

	// Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
	void* buf = malloc(BLOCK_SIZE);
	bio_read(0,buf);
	sBlock = (struct superblock*)buf;



	return NULL;
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(inode_bits);
	free(data_bits);
	free(sBlock);
	// Step 2: Close diskfile
	dev_close();
}

static int tfs_getattr(const char *path, struct stat *stbuf) {
	int NOT_FOUND = -1;
	// Step 1: call get_node_by_path() to get inode from path
	struct inode *resNode = (struct inode*)malloc(sizeof(struct inode));
	// Step 2: fill attribute of file into stbuf from inode

	if (get_node_by_path(path,0,resNode) == NOT_FOUND) {
		return -ENOENT;
	}

	stbuf->st_mode = S_IFDIR | 0755;
	stbuf->st_nlink = resNode->link;
	stbuf->st_size = resNode->size;
	if (resNode->type == FILE) {
		stbuf->st_mode = S_IFREG | 0644;
	}
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();

	stbuf->st_mtime = resNode->vstat.st_mtime;
	stbuf->st_ctime = resNode->vstat.st_ctime;
	stbuf->st_atime = resNode->vstat.st_atime;
	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* node = (struct inode *)malloc(sizeof(struct inode));
	// Step 2: If not find, return -1
	return get_node_by_path(path,0,node);
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *myInode = malloc(sizeof(struct inode));
	if (get_node_by_path(path, 0, myInode) == -1) {
		return -ENOENT;
	}

	int i, j;
	for (i = 0; i < 16; i++) {
		if (myInode->direct_ptr[i] != INVALID) {
		struct dirent* myDirent = malloc(BLOCK_SIZE);
		const int blockNo = sBlock->d_start_blk + myInode->direct_ptr[i];
		bio_read(blockNo, myDirent);
		for (j = 0; j < numOfDirents; j++) {
			if (myDirent[j].valid == VALID) {
				if (j == 0 && strcmp(myDirent[j].name,".") != 0) {
					filler(buffer,".",NULL,0);
				}
				filler(buffer, myDirent[j].name, NULL, 0);
				if (i == 0 && j == 0 && (strcmp(path,"/") == 0)) {
					filler(buffer,"..",NULL,0);
				}
			}
		}
		free(myDirent);
		}
	}

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char* head = strdup(path);
	char* dirName = dirname(head);
	head = strdup(path);
	char* baseName = basename(head);
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode* dirNode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(dirName,0,dirNode) == -1) {
		free(dirNode);
		return -ENOENT; //dirname not found, cannot create dir
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int availNo = get_avail_ino();
	set_bitmap(inode_bits,availNo);
	bio_write(sBlock->i_bitmap_blk,(void*)inode_bits);
	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(*dirNode,availNo,baseName,strlen(baseName)+1);
	// Step 5: Update inode for target directory
	struct inode* newNode = (struct inode*)malloc(sizeof(struct inode));
	memset(newNode,0,sizeof(struct inode));
	newNode->type = FOLDER;
	newNode->valid = VALID;
	newNode->ino = availNo;
	newNode->link = 2;
	int i;
	for (i = 0;i < 16; i++) {
		if (i < 8) {
			newNode->indirect_ptr[i] = INVALID;
		}
		newNode->direct_ptr[i] = INVALID; 
	}
	// Step 6: Call writei() to write inode to disk
	writei(availNo,newNode);
	dir_add(*newNode,availNo,".",2);
	readi(availNo,newNode);
	dir_add(*newNode,dirNode->ino,"..",3);
	time(&(newNode->vstat.st_mtime));
	time(&(newNode->vstat.st_ctime));
	time(&(newNode->vstat.st_atime));
	free(newNode);

	return 0;
}

static int tfs_rmdir(const char *path) {

	char* pathCopy = strdup(path);
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char* myDirName = dirname(pathCopy);
	pathCopy = strdup(path);
	char* myBaseName = basename(pathCopy);
	// Step 2: Call get_node_by_path() to get inode of target directory
	struct inode* myInode = malloc(sizeof(struct inode));
	if(strcmp(path,"/") == 0){ return -1;}//if root leave
	if ((get_node_by_path(path,0,myInode) != 0) || (myInode->type != FOLDER)){
		free(myInode);
		return -ENOENT;
	}
	char* myBitmap = malloc(sizeof(char)*BLOCK_SIZE);
	char* myData = malloc(sizeof(char)*BLOCK_SIZE);

	bio_read(2,myBitmap);
	
	// Step 3: Clear data block bitmap of target directory
	int i;
	for(i=0;i<16;i++){
		if(myInode->direct_ptr[i] != NOT_SET){continue;}
		bio_read(myInode->direct_ptr[i],myData); //read into myData
		memset(myData,0,BLOCK_SIZE);
		bio_write(myInode->direct_ptr[i],myData);
		unset_bitmap( (bitmap_t) myBitmap, myInode->direct_ptr[i]);
		myInode->direct_ptr[i] = NOT_SET; //invalid
	}
	// Step 4: Clear inode bitmap and its data block
	myInode->valid = myInode->type = myInode->link = NOT_SET; //can i do this in c....
	writei(myInode->ino,myInode);
	bio_read(1,myBitmap);
	unset_bitmap( (bitmap_t) myBitmap, myInode->ino);
	bio_write(1,myBitmap);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	if ( (get_node_by_path(myDirName,0,myInode) != 0) || (myInode->type != FOLDER)){
		free(myInode);
		return -ENOENT;
	}
	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	dir_remove(*myInode,myBaseName,strlen(myBaseName));
	free(myInode);
	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char* head = strdup(path);
	char* dirName = dirname(head);
	head = strdup(path);
	char* baseName = basename(head);
	//char* baseName = basename(head);
	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode* dirNode = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(dirName,0,dirNode) == -1) {
		free(dirNode);
		return -ENOENT;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int availNo = get_avail_ino();
	set_bitmap(inode_bits,availNo);
	bio_write(sBlock->i_bitmap_blk,(void*)inode_bits);
	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(*dirNode,availNo,baseName,strlen(baseName)+1);
	// Step 5: Update inode for target file
	struct inode* newNode = (struct inode*)malloc(sizeof(struct inode));
	newNode->type = FILE;
	newNode->valid = VALID;
	newNode->ino = availNo;
	newNode->link = 1;
	newNode->size = 0;
	int i;
	for (i = 0;i < 16; i++) {
		if (i < 8) {
			newNode->indirect_ptr[i] = INVALID;
		}
		newNode->direct_ptr[i] = INVALID; // 0 for invalid
	}
	time(&(newNode->vstat.st_mtime));
	time(&(newNode->vstat.st_ctime));
	time(&(newNode->vstat.st_atime));
	// Step 6: Call writei() to write inode to disk
	writei(availNo,newNode);
	free(newNode);
	tfs_write(path,"",0,0,fi);
	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* node = (struct inode *)malloc(sizeof(struct inode));
	// Step 2: If not find, return -1
	return get_node_by_path(path,0,node);
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	int bytes = 0;
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode* myInode = malloc(sizeof(struct inode));
	void* buf;
	if (get_node_by_path(path,0,myInode) == -1) {
		return -ENOENT;
	}
	// Step 2: Based on size and offset, read its data blocks from disk
	int offdivblock = offset / BLOCK_SIZE;
	while (size > 0) {
		buf = malloc(BLOCK_SIZE);
		int blockNo = sBlock->d_start_blk + myInode->direct_ptr[offdivblock];
		bio_read(blockNo,buf);
		int currBytes = (size < BLOCK_SIZE) ? size : BLOCK_SIZE;
		memcpy(buffer, buf, currBytes);
		bytes += currBytes;
		size -= BLOCK_SIZE;
		offdivblock++;
		free(buf);
	}
	return bytes; //else ret 0
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	int bytes = 0;
    void* buf;
    struct inode* node = (struct inode*)malloc(sizeof(struct inode));
    if (get_node_by_path(path,0,node) == -1) {
        return -ENOENT;
    }

    int offsetBlock = offset / BLOCK_SIZE;
    int oOffset = offsetBlock + 1;
    int i = 0;
    while (i < oOffset && size >= 0) {
        int blockNo = sBlock->d_start_blk + node->direct_ptr[offsetBlock];
		void* writeOffset;
		size_t currBytes = (size < BLOCK_SIZE) ? size : BLOCK_SIZE;
		buf = malloc(BLOCK_SIZE);
		if (node->direct_ptr[offsetBlock] == INVALID) {
			node->direct_ptr[offsetBlock] = get_avail_blkno();
			set_bitmap(data_bits,node->direct_ptr[offsetBlock]);
			bio_write(sBlock->d_bitmap_blk,data_bits);
			memcpy(buf,buffer,currBytes);
		} else {
			bio_read(blockNo,buf);
			writeOffset = buf + (offset % BLOCK_SIZE);
			memcpy(writeOffset,buffer,currBytes);
			if (size < BLOCK_SIZE && size + offset <= strlen((char*)buf)) {
				memset(writeOffset + size, '\0', BLOCK_SIZE - size - offset);
			}
		}
		bytes += currBytes;
		size -= currBytes;
		bio_write(blockNo,buf);
		offsetBlock++;
		i++;
		free(buf);
    }
	node->size += bytes;
	time(&(node->vstat.st_mtime));
	writei(node->ino,node);
	return bytes;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char* myPath = strdup(path);
	char* dirName = dirname(myPath);
	myPath = strdup(path);
	char* baseName = basename(myPath);
	// Step 2: Call get_node_by_path() to get inode of target file
	struct inode* target = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(path,0,target) == -1) {
		return -ENOENT;
	}
	// Step 3: Clear data block bitmap of target file
	target->link = 0;
	int i;
	for(i=0;i<16;i++){
		if(target->direct_ptr[i] != INVALID){unset_bitmap(data_bits,target->direct_ptr[i]);}
	}
	bio_write(sBlock->d_bitmap_blk,(void*)data_bits);

	// Step 4: Clear inode bitmap and its data block
	if(target->link <= 0){
		unset_bitmap(inode_bits,target->ino); //unset...
		bio_write(sBlock->d_bitmap_blk,(void*)data_bits); //write from superblock->data_bits bitmap
		target->valid = NOT_SET; //was this 0 or -1? //mark as invaild
	}
	writei(target->ino,target);
	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode* parent = (struct inode*)malloc(sizeof(struct inode));
	if (get_node_by_path(dirName,0,parent) == -1) {
		return -ENOENT;
	}
	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	dir_remove(*parent,baseName,strlen(baseName)+1);
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

