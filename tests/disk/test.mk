# blocking / non-blocking disk driver
DESC = Disk I/O with threads (non-blocking disk)
DISK = c.img
OBJS = start.o utils.o kernel.o assert.o console.o gdt.o idt.o irq.o exceptions.o \
   interrupts.o simple_timer.o frame_pool.o mem_pool.o \
   thread.o threads_low.o simple_disk.o nonblocking_disk.o \
   machine.o machine_low.o system.o scheduler.o
