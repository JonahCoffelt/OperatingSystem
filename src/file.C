/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2021/11/28

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
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
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR/DESTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File(FileSystem *_fs, int _id) {
    Console::puts("Opening file.\n");

    // Initialize data structures
    fs = _fs;
    inode = fs->LookupFile(_id);

    // Check if file exists
    if (inode == nullptr) {
        Console::puts("File not found\n");
        return;
    }
    if (inode->data_block == -1) {
        Console::puts("File has no data block\n");
        return;
    }

    current_position = 0;
    fs->disk->read(inode->data_block, indirect_block_cache);
    
    // No data yet
    data_block_no = -1;
}

File::~File() {
    Console::puts("Closing file.\n");
    /* Make sure that you write any cached data to disk. */
    /* Also make sure that the inode in the inode list is updated. */
    if (inode != nullptr && fs != nullptr) {
        fs->disk->write(inode->data_block, indirect_block_cache);
        if (data_block_no != -1) {
            unsigned int phys = indirect_get_phys(indirect_block_cache, data_block_no);
            if (phys != 0) {
                fs->disk->write(phys, data_cache);
            }
        }
        fs->WriteInodeBlockToDisk();
    }
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char *_buf) {
    Console::puts("reading from file\n");
    // If no data yet, read the first block
    if (data_block_no == -1) {
        // Load the first data block
        data_block_no = 0;
        unsigned int next_data_block = indirect_get_phys(indirect_block_cache, 0);
        if (next_data_block == 0) {
            return 0;
        }
        // Read the first data block
        fs->disk->read(next_data_block, data_cache);
        current_position = 0;
    }
    for (unsigned int i = 0; i 
        < _n; i++) {
        if (current_position >= SimpleDisk::BLOCK_SIZE) {
            // Need to read the next block
            data_block_no++;
            if (data_block_no >= MAX_DATA_BLOCKS) {
                return i;
            }
            unsigned int next_data_block = indirect_get_phys(indirect_block_cache, data_block_no);
            if (next_data_block == 0) {
                return i;
            }
            unsigned int prev_phys = indirect_get_phys(indirect_block_cache, data_block_no - 1);
            if (prev_phys != 0) {
                fs->disk->write(prev_phys, data_cache);
            }
            // Read the next block
            fs->disk->read(next_data_block, data_cache);
            current_position = 0;
        }
        // Read the current block's data
        _buf[i] = data_cache[current_position];
        current_position++;
    }
    return _n;
}

int File::Write(unsigned int _n, const char *_buf) {
    Console::puts("writing to file\n");
    // If no data yet, write to the first block
    if (data_block_no == -1) {
        // Load the first data block
        data_block_no = 0;
        unsigned int next_data_block = indirect_get_phys(indirect_block_cache, 0);
        if (next_data_block == 0) {
            int got = fs->GetFreeBlock();
            if (got < 0) {
                return 0;
            }
            next_data_block = (unsigned int)got;
            indirect_set_phys(indirect_block_cache, 0, next_data_block);
            fs->disk->write(inode->data_block, indirect_block_cache);
        }
        // Read the first data block
        fs->disk->read(next_data_block, data_cache);
        current_position = 0;
    }
    // Write to the data block
    for (unsigned int i = 0; i < _n; i++) {
        if (current_position >= SimpleDisk::BLOCK_SIZE) {
            // Need to write the next block
            data_block_no++;
            if (data_block_no >= MAX_DATA_BLOCKS) {
                return i;
            }

            unsigned int next_data_block = indirect_get_phys(indirect_block_cache, data_block_no);
            if (next_data_block == 0) {
                int got = fs->GetFreeBlock();
                if (got < 0) {
                    return i;
                }
                next_data_block = (unsigned int)got;
                indirect_set_phys(indirect_block_cache, data_block_no, next_data_block);
                fs->disk->write(inode->data_block, indirect_block_cache);
            }
            /* Persist full cache for the block we are leaving, then load the next block */
            unsigned int prev_phys = indirect_get_phys(indirect_block_cache, data_block_no - 1);
            if (prev_phys != 0) {
                fs->disk->write(prev_phys, data_cache);
            }
            fs->disk->read(next_data_block, data_cache);
            current_position = 0;
        }
        // Write to cache
        data_cache[current_position] = _buf[i];
        current_position++;
    }
    return _n;
}

void File::Reset() {
    Console::puts("resetting file\n");
    // Flush any cached data to disk
    if (data_block_no != -1) {
        unsigned int phys = indirect_get_phys(indirect_block_cache, data_block_no);
        if (phys != 0) {
            fs->disk->write(phys, data_cache);
        }
    }
    // Reset the file position
    current_position = 0;
    data_block_no = -1;
}

bool File::EoF() {
    Console::puts("checking for EoF\n");
    if (data_block_no < 0) {
        return indirect_get_phys(indirect_block_cache, 0) == 0;
    }
    if (indirect_get_phys(indirect_block_cache, data_block_no) == 0) {
        return true;
    }
    if ((unsigned int)data_block_no >= MAX_DATA_BLOCKS) {
        return true;
    }
    return false;
  }
