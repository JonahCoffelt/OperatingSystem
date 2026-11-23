# simple file system
DESC = Simple file system
DISK = c.img
OBJS = start.o utils.o kernel.o assert.o console.o gdt.o idt.o irq.o exceptions.o \
   interrupts.o simple_timer.o frame_pool.o mem_pool.o \
   simple_disk.o file.o file_system.o machine.o machine_low.o
