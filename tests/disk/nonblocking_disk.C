/*
     File        : nonblocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "nonblocking_disk.H"
#include "system.H"
#include "thread.H"
#include "machine.H"


/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/


NonBlockingDisk::NonBlockingDisk(unsigned int _size) 
: SimpleDisk(_size) {
}


bool NonBlockingDisk::lock = false; // Shared lock for the disk.

/**
 * @brief Aquire the lock for the disk. If the lock is already held, the thread will yield
 * 
 */
void NonBlockingDisk::aquire_lock() {
  Machine::disable_interrupts();
  while (lock) {
    Machine::enable_interrupts();
    System::SCHEDULER->resume(Thread::CurrentThread());
    System::SCHEDULER->yield();
    Machine::disable_interrupts();
  }
  lock = true;
  Machine::enable_interrupts();
}

/**
 * @brief Release the lock for the disk.
 * 
 */
void NonBlockingDisk::release_lock() {
  Machine::disable_interrupts();
  lock = false;
  Machine::enable_interrupts();
}

/**
 * @brief Read from the disk by using the lock
 * 
 * @param _block_no 
 * @param _buf 
 */
void NonBlockingDisk::read(unsigned long _block_no, unsigned char* _buf) {
  aquire_lock();
  SimpleDisk::read(_block_no, _buf);
  release_lock();
}

/**
 * @brief Write to the disk by using the lock.
 * 
 * 
 * @param _block_no 
 * @param _buf 
 */
void NonBlockingDisk::write(unsigned long _block_no, unsigned char* _buf) {
  aquire_lock();
  SimpleDisk::write(_block_no, _buf);
  release_lock();
}

void NonBlockingDisk::wait_while_busy() {
  while (is_busy()) {
    // Add this thread back to the ready queue and yield the CPU
    System::SCHEDULER->resume(Thread::CurrentThread());
    System::SCHEDULER->yield();
  }
}