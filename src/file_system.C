/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file_system.H"
#include "utils.H"

/*--------------------------------------------------------------------------*/
/* CLASS Inode */
/*--------------------------------------------------------------------------*/

/* You may need to add a few functions, for example to help read and store 
   inodes from and to disk. */

/*--------------------------------------------------------------------------*/
/* CLASS FileSystem */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    Console::puts("In file system constructor.\n");
    disk = nullptr;
    size = 0;
    inodes = nullptr;
    free_blocks = nullptr;
}

FileSystem::~FileSystem() {
    Console::puts("unmounting file system\n");
    /* Make sure that the inode list and the free list are saved. */

    if (inodes != nullptr && free_blocks != nullptr) {
        WriteInodeBlockToDisk();
        WriteFreeListBlockToDisk();

        delete[] inodes;
        delete[] free_blocks;
    }
}

bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("mounting file system from disk\n");
    /* Here you read the inode list and the free list into memory */
    disk = _disk;

    // Read block 0 and 1 from disk
    unsigned char* block0 = new unsigned char[SimpleDisk::BLOCK_SIZE];
    unsigned char* block1 = new unsigned char[SimpleDisk::BLOCK_SIZE];
    _disk->read(0, block0);
    _disk->read(1, block1);

    // Metadata at start of block 0 (must match Format)
    unsigned int magic_number = read_le32(block0);
    unsigned int fs_size_bytes = read_le32(block0 + 4);

    if (magic_number != FS_MAGIC) {
        Console::puts("Magic number is incorrect\n");
        delete[] block0;
        delete[] block1;
        return false;
    }

    size = fs_size_bytes;

    inodes = new Inode[MAX_INODES];
    memcpy(inodes, block0 + INODE_BLOCK_HEADER_BYTES, MAX_INODES * sizeof(Inode));

    free_blocks = new unsigned char[SimpleDisk::BLOCK_SIZE];
    memcpy(free_blocks, block1, SimpleDisk::BLOCK_SIZE);

    // Set the inodes to point to this file system
    for (int i = 0; i < MAX_INODES; i++) {
        inodes[i].fs = this;
    }

    delete[] block0;
    delete[] block1;
    return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) { // static!
    Console::puts("formatting disk\n");
    /* Here you populate the disk with an initialized (probably empty) inode list
       and a free list. Make sure that blocks used for the inodes and for the free list
       are marked as used, otherwise they may get overwritten. */

    // Allocate blocks
    unsigned char* block0 = new unsigned char[SimpleDisk::BLOCK_SIZE];
    unsigned char* block1 = new unsigned char[SimpleDisk::BLOCK_SIZE];
    
    // Clear block 0 and block 1
    memset(block0, 0, SimpleDisk::BLOCK_SIZE);
    memset(block1, 0, SimpleDisk::BLOCK_SIZE);

    // Write magic number to block 0
    write_le32(block0, FS_MAGIC);
    // Write size to block 0
    write_le32(block0 + 4, _size);

    // Mark first two blocks as used
    bitmap_mark_used(block1, 0);
    bitmap_mark_used(block1, 1);

    /* Initialize inode slots on disk before write (empty slot: id == -1) */
    Inode* inode_table = (Inode*)(block0 + INODE_BLOCK_HEADER_BYTES);
    for (unsigned int i = 0; i < MAX_INODES; i++) {
        inode_table[i].id = -1;
        inode_table[i].data_block = -1;
        inode_table[i].fs = nullptr;
    }

    // Write block 0 and block 1 to disk
    _disk->write(0, block0);
    _disk->write(1, block1);

    delete[] block0;
    delete[] block1;
    return true;
}

Inode * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file with id = "); Console::puti(_file_id); Console::puts("\n");
    /* Here you go through the inode list to find the file. */
    
    if (_file_id == -1) {
        Console::puts("Invalid file id\n");
        return nullptr;
    }
    
    for (int i = 0; i < MAX_INODES; i++) {
        if (inodes[i].id == (long)_file_id) {
            return &inodes[i];
        }
    }
    return nullptr;
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* Here you check if the file exists already. If so, throw an error.
       Then get yourself a free inode and initialize all the data needed for the
       new file. After this function there will be a new file on disk. */
    if (_file_id == -1) {
        Console::puts("Invalid file id\n");
        return false;
    }
    if (LookupFile(_file_id) != nullptr) {
        Console::puts("File already exists\n");
        return false;
    }

    // Find a free inode
    Inode* free_inode = GetFreeInode();
    if (free_inode == nullptr) {
        Console::puts("No free inode found\n");
        return false;
    }

    // Find a free block for the indirect block
    int indirect_block_no = GetFreeBlock();
    if (indirect_block_no < 0) {
        Console::puts("No free block found\n");
        return false;
    }

    // Clear the indirect block
    unsigned char* indirect_block_cache = new unsigned char[SimpleDisk::BLOCK_SIZE];
    memset(indirect_block_cache, 0, SimpleDisk::BLOCK_SIZE);
    disk->write(indirect_block_no, indirect_block_cache);

    // Initialize the inode
    free_inode->id = (long)_file_id;
    free_inode->fs = this;
    free_inode->data_block = indirect_block_no;

    /* Block 0 is [ header ][ entire inode table ]; GetFreeBlock updated the bitmap in memory */
    WriteInodeBlockToDisk();
    WriteFreeListBlockToDisk();

    return true;
}

bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file with id:"); Console::puti(_file_id); Console::puts("\n");
    /* First, check if the file exists. If not, throw an error. 
       Then free all blocks that belong to the file and delete/invalidate 
       (depending on your implementation of the inode list) the inode. */
    

    if (_file_id == -1) {
        Console::puts("Invalid file id\n");
        return false;
    }

    Inode* inode = LookupFile(_file_id);
    if (inode == nullptr) {
        Console::puts("File not found\n");
        return false;
    }

    // Free all blocks that belong to the file
    int indirect_block_no = inode->data_block;
    if (indirect_block_no != -1) {
        unsigned char* indirect_block_cache = new unsigned char[SimpleDisk::BLOCK_SIZE];
        disk->read(indirect_block_no, indirect_block_cache);
        for (int i = 0; i < indirect_slot_count(); i++) {
            unsigned int phys = indirect_get_phys(indirect_block_cache, i);
            if (phys != 0) {
                bitmap_mark_free(free_blocks, phys);
            }
        }
        bitmap_mark_free(free_blocks, indirect_block_no);
        delete[] indirect_block_cache;
    }

    // Delete the inode
    inode->id = -1;
    inode->data_block = -1;

    // Write changes to disk
    WriteInodeBlockToDisk();
    WriteFreeListBlockToDisk();

    return true;
}

int FileSystem::GetFreeBlock() {
    if (free_blocks == nullptr || size == 0) {
        return -1;
    }

    unsigned int num_blocks = size / SimpleDisk::BLOCK_SIZE;
    const unsigned int BIT_SLOTS = SimpleDisk::BLOCK_SIZE * 8;
    unsigned int limit = num_blocks;
    if (limit > BIT_SLOTS) {
        limit = BIT_SLOTS;
    }

    for (unsigned int b = 0; b < limit; b++) {
        if (bitmap_is_free(free_blocks, b)) {
            bitmap_mark_used(free_blocks, b);
            return (int)b;
        }
    }

    return -1;
}

Inode* FileSystem::GetFreeInode() {
    if (inodes == nullptr) {
        return nullptr;
    }

    for (int i = 0; i < MAX_INODES; i++) {
        if (inodes[i].id == -1) {
            return &inodes[i];
        }
    }
    return nullptr;
}

void FileSystem::WriteInodeBlockToDisk() {
    unsigned char buf[SimpleDisk::BLOCK_SIZE];
    write_le32(buf, FS_MAGIC);
    write_le32(buf + 4, size);
    memcpy(buf + INODE_BLOCK_HEADER_BYTES, inodes, MAX_INODES * sizeof(Inode));
    disk->write(0, buf);
}

void FileSystem::WriteFreeListBlockToDisk() {
    disk->write(1, free_blocks);
}