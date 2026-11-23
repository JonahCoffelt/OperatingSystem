/*
     File        : nonblocking_disk.H

     Author      : 

     Date        : 
     Description : 

*/

#ifndef _NONBLOCKING_DISK_H_
#define _NONBLOCKING_DISK_H_

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "simple_disk.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */ 
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* N o n B l o c k i n g D i s k  */
/*--------------------------------------------------------------------------*/

class NonBlockingDisk : public SimpleDisk {
public:
   NonBlockingDisk(unsigned int _size); 
   /* Creates a NonBlockingDisk device with the given size connected to the 
      MASTER slot of the primary ATA controller.
      NOTE: We are passing the _size argument out of laziness. 
      In a real system, we would infer this information from the 
      disk controller. */

   virtual void read(unsigned long _block_no, unsigned char* _buf);
   virtual void write(unsigned long _block_no, unsigned char* _buf);

   virtual void wait_while_busy();

private:
  void aquire_lock();
  void release_lock();

  static bool lock;
};

#endif
