/*
	File: file_system.H

	Author: R. Bettati
			Department of Computer Science
			Texas A&M University
	Date  : 24/11/22

	Description: Simple File System.


*/

#ifndef _FILE_SYSTEM_H_ // include file only once
#define _FILE_SYSTEM_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

class FileSystem;


/*--------------------------------------------------------------------------*/
/* HELPER FUNCTIONS */
/*--------------------------------------------------------------------------*/

static unsigned int read_le32(const unsigned char* _p) {
	return (unsigned int)_p[0] | ((unsigned int)_p[1] << 8) | ((unsigned int)_p[2] << 16) | ((unsigned int)_p[3] << 24);
}

static void write_le32(unsigned char* _p, unsigned int _value) {
	_p[0] = (unsigned char)(_value & 0xFF);
	_p[1] = (unsigned char)((_value >> 8) & 0xFF);
	_p[2] = (unsigned char)((_value >> 16) & 0xFF);
	_p[3] = (unsigned char)((_value >> 24) & 0xFF);
}

static void bitmap_mark_used(unsigned char* _bm, unsigned int _block) {
	_bm[_block / 8] |= (unsigned char)(1u << (_block % 8));
}

static void bitmap_mark_free(unsigned char* _bm, unsigned int _block) {
	_bm[_block / 8] &= (unsigned char)(~(1u << (_block % 8)));
}

static bool bitmap_is_free(const unsigned char* _bm, unsigned int _block) {
	return (_bm[_block / 8] & (unsigned char)(1u << (_block % 8))) == 0;
}

static unsigned int indirect_slot_count() {
	return SimpleDisk::BLOCK_SIZE / 4;
}

static unsigned int indirect_get_phys(const unsigned char* _ib, int _slot) {
	if (_slot < 0 || (unsigned int)_slot >= indirect_slot_count()) {
		return 0;
	}
	return read_le32(_ib + _slot * 4);
}

static void indirect_set_phys(unsigned char* _ib, int _slot, unsigned int _phys) {
	if (_slot >= 0 && (unsigned int)_slot < indirect_slot_count()) {
		write_le32(_ib + _slot * 4, _phys);
	}
}

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

class Inode
{
	friend class FileSystem; // The inode is in an uncomfortable position between
	friend class File;       // File System and File. We give both full access
	// to the Inode.

private:
	long id; // File "name"

	/* You will need additional information in the inode, such as allocation
	   information. */
	int data_block;

	FileSystem* fs; // It may be handy to have a pointer to the File system.
	// For example when you need a new block or when you want
	// to load or save the inode list. (Depends on your
	// implementation.)

/* You may need a few additional functions to help read and store the
   inodes from and to disk. */
};

/*--------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS */
/*--------------------------------------------------------------------------*/

/* (none) */

/*--------------------------------------------------------------------------*/
/* F i l e S y s t e m  */
/*--------------------------------------------------------------------------*/

class FileSystem
{

	friend class Inode;
	friend class File;

private:
	/* -- DEFINE YOUR FILE SYSTEM DATA STRUCTURES HERE. */

	SimpleDisk* disk;
	unsigned int size;

	/* Block 0 layout: [ magic (4) ][ fs size in bytes (4) ][ inode array ... ] */
	static constexpr unsigned int INODE_BLOCK_HEADER_BYTES = 8;
	static constexpr unsigned int FS_MAGIC = 42424242u;

	static constexpr unsigned int MAX_INODES = (SimpleDisk::BLOCK_SIZE - INODE_BLOCK_HEADER_BYTES) / sizeof(Inode);
	/* Inode records occupy the rest of block 0 after the header */

	Inode* inodes; // Block 0 — INODES: one contiguous array of Inode records (the starter already assumes MAX_INODES = BLOCK_SIZE / sizeof(Inode)).

	/* The inode list */

	unsigned char* free_blocks;
	/* The free-block list. You may want to implement the "list" as a bitmap.
	   Feel free to use an unsigned char to represent whether a block is free or not;
	   no need to go to bits if you don't want to.
	   If you reserve one block to store the "free list", you can handle a file system up to
	   256kB. (Large enough for a proof of concept.) */

	Inode* GetFreeInode();
	int GetFreeBlock();
	   /* It may be helpful to two functions to hand out free inodes in the inode list and free
		  blocks. These functions also come useful to class Inode and File. */

	void WriteInodeBlockToDisk();
	void WriteFreeListBlockToDisk();

public:
	FileSystem();
	/* Just initializes local data structures. Does not connect to disk yet. */

	~FileSystem();
	/* Unmount file system if it has been mounted. */

	bool Mount(SimpleDisk* _disk);
	/* Associates this file system with a disk. Limit to at most one file system per disk.
	   Returns true if operation successful (i.e. there is indeed a file system on the disk.) */

	static bool Format(SimpleDisk* _disk, unsigned int _size);
	/* Wipes any file system from the disk and installs an empty file system of given size. */

	Inode* LookupFile(int _file_id);
	/* Find file with given id in file system. If found, return its inode.
		 Otherwise, return null. */

	bool CreateFile(int _file_id);
	/* Create file with given id in the file system. If file exists already,
	   abort and return false. Otherwise, return true. */

	bool DeleteFile(int _file_id);
	/* Delete file with given id in the file system; free any disk block occupied by the file. */
};
#endif
