# kernel threads and scheduling
DESC = Kernel threads, FIFO / round-robin scheduler
OBJS = start.o utils.o kernel.o assert.o console.o gdt.o idt.o irq.o exceptions.o \
   interrupts.o simple_timer.o frame_pool.o mem_pool.o \
   thread.o threads_low.o scheduler.o machine.o machine_low.o
