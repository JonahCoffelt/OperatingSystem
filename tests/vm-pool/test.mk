# virtual memory pools
DESC = Virtual memory pools on recursive page tables
OBJS = start.o utils.o kernel.o assert.o console.o gdt.o idt.o irq.o exceptions.o \
   interrupts.o simple_timer.o paging_low.o page_table.o cont_frame_pool.o vm_pool.o \
   machine.o machine_low.o
