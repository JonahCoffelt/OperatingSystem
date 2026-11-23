/*
 File: vm_pool.C
 
 Author:
 Date  : 2024/09/20
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

const unsigned long PAGE_SIZE   = PageTable::PAGE_SIZE;

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

/**
 * @brief Construct a new VMPool::VMPool object
 * 
 * @param _base_address Base address of the pool
 * @param _size Size of the pool
 * @param _frame_pool Frame pool to use
 * @param _page_table Page table to use
 */
VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {

    // Save attributes
    base_address = _base_address;
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;
    allocated_regions_count = 0;
    free_regions_count = 0;
    total_pages = size / PAGE_SIZE + (size % PAGE_SIZE > 0 ? 1 : 0);
    total_regions = 512;
    initalizing = true;


    // Register the pool with the page table
    _page_table->register_pool(this);

    // Initialize regions frames
    free_regions = (Region*)(_base_address);
    allocated_regions = (Region*)(_base_address + PAGE_SIZE);
    
    unsigned long pool_base_page = _base_address / PAGE_SIZE;
    
    // Initialize initial allocated region (first two frames)
    allocated_regions[0].base = pool_base_page;
    allocated_regions[0].length = 2;
    allocated_regions_count += 1;

    // Initialize initial free region (the rest of the frames)
    free_regions[0].base = pool_base_page + 2;
    free_regions[0].length = total_pages - 2;
    free_regions_count += 1;

    initalizing = false;

    Console::puts("Constructed VMPool object.\n");
}

/**
 * @brief Allocate a region of memory
 * 
 * @param _size Size of the region to allocate
 * @return unsigned long Virtual address of the allocated region
 */
unsigned long VMPool::allocate(unsigned long _size) {

    // Check if there are any allocation regions left
    if (allocated_regions_count >= total_regions) {
        Console::puts("No free region found.\n");
        assert(false);
    }

    // Calculate the number of frames needed
    int nframes = _size / PAGE_SIZE + (_size % PAGE_SIZE > 0 ? 1 : 0);
    if (nframes == 0) nframes = 1;

    // Check for a larger enough free region
    int first_fit_index = -1;
    for (unsigned int i = 0; i < free_regions_count; i++) {
        if (free_regions[i].length >= nframes) {
            first_fit_index = (int)i;
            break;
        }
    }

    if (first_fit_index == -1) {
        Console::puts("No free region found.\n");
        return 0;
    }

    // Allocate the region
    unsigned long base_page = free_regions[first_fit_index].base;

    allocated_regions[allocated_regions_count].base = base_page;
    allocated_regions[allocated_regions_count].length = nframes;
    allocated_regions_count++;

    // Update the free region
    free_regions[first_fit_index].base += nframes;
    free_regions[first_fit_index].length -= nframes;

    // If that free region is now empty
    if (free_regions[first_fit_index].length == 0) {
        free_regions[first_fit_index] = free_regions[free_regions_count - 1];
        free_regions_count--;
    } 

    // Return the virtual address of the allocated region
    Console::puts("Allocated region of memory.\n");
    return allocated_regions[allocated_regions_count - 1].base * PAGE_SIZE;
}

/**
 * @brief Release a region of memory
 * 
 * @param _start_address Start address of the region to release
 */
void VMPool::release(unsigned long _start_address) {

    // Find the allocated region that equals the start address
    unsigned long start_page = _start_address / PAGE_SIZE;
    int allocated_region_index = -1;
    for (unsigned int i = 0; i < allocated_regions_count; i++) {
        if (allocated_regions[i].base == start_page) {
            allocated_region_index = i;
            break;
        }
    }

    // Check if the region was found
    if (allocated_region_index == -1) {
        Console::puts("Region not found.\n");
        assert(false);
    }
    
    // Free the region
    Region* region = &allocated_regions[allocated_region_index];
    for (unsigned int i = 0; i < region->length; i++) {
        page_table->free_page(region->base + i);
    }

    // Update the free regions
    free_regions[free_regions_count].base = region->base;
    free_regions[free_regions_count].length = region->length;
    free_regions_count++;

    // Pop swap back to current index
    allocated_regions[allocated_region_index] = allocated_regions[allocated_regions_count - 1];
    allocated_regions_count--;

    Console::puts("Released region of memory.\n");
}

/**
 * @brief Check if the address is legitimate
 * 
 * @param _address Address to check
 * @return true If the address is legitimate
 * @return false If the address is not legitimate
 */
bool VMPool::is_legitimate(unsigned long _address) {
    const unsigned long PAGE_SIZE = PageTable::PAGE_SIZE;
    unsigned long page = _address / PAGE_SIZE;

    if (initalizing) { return true; }

    // Check if address is within any allocated region
    for (unsigned int i = 0; i < allocated_regions_count; i++) {
        unsigned long base   = allocated_regions[i].base;
        unsigned long length = allocated_regions[i].length;
        if (page >= base && page < base + length) {
            return true;
        }
    }

    return false;
}
