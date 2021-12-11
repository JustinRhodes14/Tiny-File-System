# Tiny File System using FUSE

1.	Names and netIDs: 
Ashank Punj, ap1635 | Justin Rhodes, jgr85

2.	Benchmarking Information
a.	The total number of blocks used on sample benchmark was:104
b.	The amount of time it took to run the benchmark was: 200 ms (estimate)

3.	Implementation:
a.	get_avail_ino
	 	Find the first zero entry in the bitmap inode_bits, mark it as used, and then write it to superblock->i_bitmap_blk (the inode bitmap of the superblock).
b.	get_avail_blkno
	 	Get the bitmap data_bits, and find the first entry that = 0. Set this location to 1 to mark it as used, and then write it to the superblock data bitmap.
c.	readi
	 	Calculate the on-disk block number (given by the superblock), and the offset, which is the inode number modulus the number of inodes-per-block.
Then, call bio_read to read the block from the disk, and create an inode pointer such that the list of inodes is equivalent to the offset calculated earlier. 
d.	writei
	 	Similarly to before, compute the on-disk block number and offset.
Then, write the inode to disk by calling bio_write to write from our struct to disk. Call bio_write to write from the list of inodes to the on-disk blkno, and free all buffers.
e.	dir_find	
	 	For each entry in the directory’s data block, check for validity and read into the buffer if so. Then, iterate on each directory-entry, and check both for validity and if the name of the jth entry matches the parameter fname. If so, let the dirent-pointer defined be that entry, and return the status 1, indicating success.
f.	dir_add
	 	For every entry in dir_inode->direct_ptr, we check for validity and read into the buffer if so, before initializing a list of directory entries in direct_ptr[i].
			For each entry, we check if the given name fname is already used. If so, we return a 0 status and just add it to the inode’s list.

		Else, we allocate a new data block (and set the status to 1), update the directory inode with bio_read, and then use memset to create a set directory entry, writing fname to its “name” parameter, 1 into valid, and the given inode number into ino, before then updating the list of dirents with this entry, and using bio_write to write this back into the block.

g.	dir_remove
	 	For every entry in dir_inode->direct_ptr, we define a buffer, read from the direct pointer into the buffer, and look through the number of directory entries within that inode. If we get a match for our filename and a valid-bit, we define an empty-directory, with zero’d-out parameters, and read this into the current location. We also define an empty directory-list that merely points back to the current empty directory, and write this to the incode before freeing our buffers and pointers and exiting.
h.	get_node_by_path
	 	We initialize a token from our path parameter, and a “head” from tokenizing it and splitting by a “/” character. After initializing a directory-entry pointer “myDirent”, we resolve the path name as so:
			Check if head is null - if so, read the parameter inode and return 0. This is our base-case.
			Otherwise, call dir_find - if we find a directory, look through it recursively and tokenize further.
		We have one further case if dir_find fails, returning -1 to indicate a failure in either dir_find or a malformed input.
i.	tfs_mkfs
	 	The make file system function initializes all necessary data structures, and also sets up our file system and our disk. Here, we initialize our bitmaps, as well as our inodes, after initializing each inode, we write it to disk and continue for the value of MAX_INUM - 1. We do one less than MAX_INUM because we must save slot 0 for the root inode. Lastly, we initialize the root inode, and set it up as the parent directory for all future files.
j.	tfs_init
	 	Here, we simply check if our file system has been made, if not, we call tfs_mkfs. If it is, we initialize the data structures and load in the necessary data to continue running our file system.
k.	tfs_destroy
	 	This simply is a parent function that frees the bitmaps for the inodes and data block, and then frees the superblock, before calling dev_close.
l.	tfs_getattr
	 	This function gets the attribute for a given file, and is a backbone for a file system. This is called in parallel with most of our functions, and ensures a file actually exists. If a file exists, it will give us information about the inodes/blocks it resides on, and whether or not it exists, as well as whether it's a file or a folder. In order to do this, we call get_node_by_path(), and check stbufs data to get info about the file if it exists on our diskfile.
m.	tfs_opendir
	 	Somewhat similar to the functionality of tfs_getattr, we call get_node_by_path() to see if a directory exists, and if not, we return the appropriate value.
n.	tfs_readdir
	 	The directory entries are read while the current block-number is <= 16. If the inode’s direct-pointer entry at that index is nonzero, we read it into the data block and update a status flag as appropriate. If the read-directory is further not-null, we return 0 - else, we iterate the current block-number (our index) and continue through the loop.
o.	tfs_mkdir
	 	We call get_avail_ino to get our first available inode number, and set the inode bitmap with such a value, before then writing that with bio_write. Then, we add a directory entry of the target entry with dir_add, before initializing and updating a new inode for the target - the number is our available number from before, the type is an enum “FOLDER”, we set its validity flag to 1, the link to 2, and the size to 0. We then set the 8 entries in the indirect_ptr array to 0, as well as the 16 entries in direct_ptr array, before writing this all to disk and freeing that node pointer we had initialized earlier.
p.	tfs_rmdir
	 	After the inode is obtained, we check if the path is root, or if it’s null or has a type that isn’t 1. To clear the data-block bitmap, we run a loop through each pointer in inode->direct_ptr[] and read it into a char* myData. We set myData to 0 with memset, and write from myData back into the location in the direct-pointer array, before declaring it invalid and unsetting the bitmap. To clear the inode bitmap, we zero-out all the inode’s parameters, including validity, write these new changes to the inode number, read and unset the bitmap, and then write our new results back into the bitmap. We then clear the parent directory by freeing the inode and calling dir_remove() on the inode with the basename for the length of the basename string.
q.	tfs_create
	 	Upon creation of a file, we get the node by its path for the parent directory, and from there, we find the most recent inode, update our bitmap, then add the parent directory to be the parent of said file. We then update the inode with the new data, updating all attributes. We update the stats of the file, and write it to disk, as well as call tfs_write on the file to prime it for future write additions.
r.	tfs_open
	 	We initialize our status to -1 (“not found”). Using get_node_by_path, we find if 0 is the root inode number of our inode from the path or not. If so, we set status to one, and return that value at the end.
s.	tfs_read
	 	Get_node_by_path is used to get the inode from the parameter bath. Then, the the offset parameter divided by the blocksize is used to determine if it is within the proper bounds (under 16). If so, we check if the pointer at that location exists, and let it be the first available block-number if so using get_avail_blkno(). We read it into a temp value, copy that into the buffer, increment the size of our inode from earlier by the parameter size, then write this into the inode, and return the final size (# of bytes).
t.	tfs_write
	 	We follow through the steps in this function, and we find the data bloicks to read based on the offset. From here, if we can't find an available data block, we must allocate one for the given inode. If data block exists, then we can simply read the block and write to the buffer. One we have the data block, we then copy the memory from the buffer to write location. And we update our byte size accordingly. For larger writes, we loop through until the write is complete. In our last step, we update the inode stats and write it to disk.
u.	tfs_unlink
	 	The target is allocated, and then the inode is obtained using get_node_by_path and the parameter path. Then, for every pointer in the target node, the bitmap is unset and bio_write is used to update the bitmap accordingly. Then, the incode is cleared by unsetting it, updating the bitmap, and declaring it as invalid. Get_node_by_path is used to get the inode of the parent, before dir_remove is used to remove the target’s parent directory’s entry.
		
4.	Special Compiling Instructions:
	a.	(if needed)
