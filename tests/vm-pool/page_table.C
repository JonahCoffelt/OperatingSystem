#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

#define MB * (0x1 << 20)
#define KB * (0x1 << 10)

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;

VMPool* PageTable::vm_pools[50] = {nullptr};
unsigned int PageTable::vm_pool_count = 0;

/**
 * @brief Initialize the paging system
 * 
 * @param _kernel_mem_pool Frame pool for the kernel memory
 * @param _process_mem_pool Frame pool for the process memory  
 * @param _shared_size Size of the shared address space
 */
void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   // Init static variables
   current_page_table = 0x0;
   paging_enabled     = false;
   kernel_mem_pool    = _kernel_mem_pool;
   process_mem_pool   = _process_mem_pool;
   shared_size        = _shared_size;

   Console::puts("Initialized Paging System\n");
}

/**
 * @brief Construct a new Page Table object
 * 
 * @note Cite: http://www.osdever.net/tutorials/view/implementing-basic-paging
 */
PageTable::PageTable()
{
   page_directory = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);

   // Get frame for the direct page table
   unsigned long *direct_page_table = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);

   // Set the first entry of the page directory to the direct page table
   page_directory[0] = (unsigned long)direct_page_table | 3;

   // map the first 4MB of memory directly
   unsigned long address = 0; // holds the physical address of where a page is
   for(unsigned int i = 0; i < ENTRIES_PER_PAGE; i++) {
      direct_page_table[i] = address | 3; // attribute set to: supervisor level, read/write, present(011 in binary)
      address = address + PAGE_SIZE; // 4096 = 4kb
   };

   // Mark the rest of the page directory as not present
   for (unsigned int i = 1; i < ENTRIES_PER_PAGE; i++) {
      page_directory[i] = 0 | 2;  // supervisor, r/w, NOT present
   };

   // Set the self reference entry on last entry of the page directory
   page_directory[1023] = (unsigned long)page_directory | 3;
   
   Console::puts("Constructed Page Table object\n");
}

/**
 * @brief Make this page table the current table
 * 
 */
void PageTable::load()
{
   current_page_table = this;
   write_cr3((unsigned long)page_directory);
   Console::puts("Loaded page table\n");
}

/**
 * @brief Enable paging on the CPU
 * 
 */
void PageTable::enable_paging()
{
    unsigned long cr0 = read_cr0();
    cr0 = cr0 | (0x1 << 31); 
    write_cr0(cr0);
    paging_enabled = true;
    Console::puts("Enabled paging\n");
}

/**
 * @brief Handle a page fault, makes a new page table and maps the fault address
 * 
 * @param _r Registers from the CPU
 */
void PageTable::handle_fault(REGS * _r)
{
    // Get the fault address
    unsigned long fault_addr = read_cr2();

    // Check legit
    bool legitimate = false;
    for (unsigned int i = 0; i < vm_pool_count; i++) {
        if (vm_pools[i]->is_legitimate(fault_addr)) {
            legitimate = true;
            break;
        }
    }
    if (!legitimate) {
        Console::puts("Fault address is not legitimate\n");
        assert(false);
    }

        // Get pde by slaming first 10 bits to the right
    unsigned long * pde = PageTable::PDE_address(fault_addr);
    // Get pte by slamming first 10 bits away to the left, then slamming the next 10 bits to the right
    unsigned long * pte = PageTable::PTE_address(fault_addr);

    // Check that the pde is present by looking at the last bit
    unsigned long *page_table;
    if (((*pde) & 0x1) == 0x0) {
        // Page directory entry is not present, so we need to create a new page table
        page_table = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);

        // Set the pde in the directory to the new page table
        *pde = (unsigned long)page_table | 3;

        // Mark the entries of the page table as not present
        for (unsigned int i = 0; i < ENTRIES_PER_PAGE; i++) {
            page_table[i] = 0 | 2;  // supervisor, r/w, NOT present
        };
    }
    else {
        page_table = (unsigned long *)(((*pde) >> 12) << 12); // Prune the data bits by slaming 12 bits to the right then slamming 12 bits to the left
    }

    // Now that we have ensured the page table is present, we can get a frame and map it to the fault address
    // If the directory did no exist, then the frame wont either
    unsigned long frame = process_mem_pool->get_frames(1);
    unsigned long frame_addr = frame * PAGE_SIZE;
        
    // Map the frame in the page table
    *pte = frame_addr | 3;  // supervisor, r/w, present

    Console::puts("handled page fault\n");
}

/** New in MP4 **/

/**
 * @brief Get the PDE address for a given address
 * 
 * @param addr Address to get the PDE address for
 * @return unsigned long* PDE address
 */
unsigned long * PageTable::PDE_address(unsigned long addr) {
    return (unsigned long *)(((addr >> 22) << 2) | 0xfffff000);
}

/**
 * @brief Get the PTE address for a given address
 * 
 * @param addr Address to get the PTE address for
 * @return unsigned long* PTE address
 */
unsigned long * PageTable::PTE_address(unsigned long addr) {
    return (unsigned long *)(((addr >> 12) << 2) | 0xffc00000);
}

/**
 * @brief Register a virtual memory pool with the page table
 * 
 * @param pool Virtual memory pool to register
 */
void PageTable::register_pool(VMPool * pool) {
    // Check for max number of pools
    if (vm_pool_count >= 50) { Console::puts("Maximum number of VM pools reached\n"); assert(false); }

    // Check for duplicates
    for (unsigned int i = 0; i < vm_pool_count; i++) {
        if (vm_pools[i] == pool) { return; }
    }

    // Add the pool to the array
    vm_pools[vm_pool_count] = pool;
    vm_pool_count++;
}

/**
 * @brief Free a page in the page table
 * 
 * @param _page_no Page number to free
 */
void PageTable::free_page(unsigned long _page_no) {
    // Get the virtual address of the page number
    unsigned long virtual_addr = _page_no * PAGE_SIZE;

    // Check legit
    bool legitimate = false;
    for (unsigned int i = 0; i < vm_pool_count; i++) {
        if (vm_pools[i]->is_legitimate(virtual_addr)) {
            legitimate = true;
            break;
        }
    }
    if (!legitimate) {
        Console::puts("Virtual address is not legitimate\n");
        assert(false);
    }

    // Get the PTE for this virtual address
    unsigned long *pte = PageTable::PTE_address(virtual_addr);
    
    // If page is not present, nothing to free
    if ( ((*pte) & 0x1) == 0 ) {
        Console::puts("Attempt to free non-present page\n");
        *pte = 0 | 2;
        write_cr3(read_cr3());
        return;
        // assert(false);
    }


    // Extract the physical frame number from the PTE
    unsigned long frame_no = (*pte) >> 12;

    // Mark the PTE as not present
    *pte = 0 | 2;

    // Release the frame back to the frame pool
    ContFramePool::release_frames(frame_no);

    // Flush the TLB entry
    write_cr3(read_cr3());
}