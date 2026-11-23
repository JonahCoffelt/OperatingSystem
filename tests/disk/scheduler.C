/*
 File: scheduler.C
 
 Author: Jonah Coffelt
 Date  : 3/24/2026
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "interrupts.H"
#include "simple_timer.H"


extern Thread * current_thread;

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

Thread * Scheduler::zombie_threads[MAX_THREADS] = {};
unsigned int Scheduler::zombie_threads_count = 0;

EOQTimer * Scheduler::eoq_timer = nullptr;

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

/* Helper functions */
void Scheduler::enable_interrupts() { if (!Machine::interrupts_enabled()) Machine::enable_interrupts(); }
void Scheduler::disable_interrupts() { if (Machine::interrupts_enabled()) Machine::disable_interrupts(); }

// EOQTimer Scheduler::eoq_timer(100);

Scheduler::Scheduler() {
  ready_queue_tail = 0;
  ready_queue_head = 0;
  ready_queue_size = 0;

  if (!eoq_timer) {
    eoq_timer = new EOQTimer(100);
    InterruptHandler::register_handler(0, eoq_timer);
  }

  Console::puts("Constructed Scheduler.\n");
}

/**
 * @brief Called by the currently running thread in order to give up the CPU and dispatch to the next thread in the ready queue.
 * 
 */
void Scheduler::yield() {
  disable_interrupts();
  
  // Check if the ready queue is empty, we should never be here
  if (ready_queue_size == 0) {
    current_thread = nullptr;
    Console::puts("No threads in ready queue.\n");
    assert(false);
  }

  // Set current thread to the next thread in the ready queue
  Thread* next_thread = ready_queue[ready_queue_head];
  ready_queue_head = (ready_queue_head + 1) % MAX_THREADS;
  ready_queue_size--;
  // Dispatch to the next thread in the ready queue
  Thread::dispatch_to(next_thread);
  enable_interrupts();
}

/**
 * @brief Add the given thread to the ready queue of the scheduler. Does not dispatch it
 * 
 * @param _thread 
 */
void Scheduler::resume(Thread * _thread) {
  disable_interrupts();

  // Kill all zombie threads
  kill_zombie_threads();

  // Check if the thread is nullptr
  assert(_thread != nullptr);

  // Check if the ready queue is full
  assert(ready_queue_size < MAX_THREADS);

  // Add the thread to the ready queue and update the tail
  ready_queue[ready_queue_tail] = _thread;
  ready_queue_tail = (ready_queue_tail + 1) % MAX_THREADS;
  ready_queue_size++;

  enable_interrupts();
}

/**
 * @brief Add the given thread to the ready queue of the scheduler. Does not dispatch it
 * 
 * @param _thread 
 */
void Scheduler::add(Thread * _thread) {
  resume(_thread);
}

/**
 * @brief Terminate the given thread.
 * 
 * @param _thread 
 */
void Scheduler::terminate(Thread * _thread) {
  disable_interrupts();
  // Add the thread to the zombie threads
  assert(zombie_threads_count < MAX_THREADS);
  zombie_threads[zombie_threads_count] = _thread;
  zombie_threads_count++;

  // If the thread is the current thread, yield
  if (_thread == current_thread) {
    enable_interrupts();
    yield();
    return;
  }

  // Find the thread in the ready queue
  int termination_index = -1;
  int current_index = ready_queue_head;
  while (current_index != ready_queue_tail) {
    if (ready_queue[current_index] == _thread) {
      termination_index = current_index;
      break;
    }
    current_index = (current_index + 1) % MAX_THREADS;
  }

  // Check if the thread is in the ready queue
  if (termination_index == -1) {
    Console::puts("Thread not found in ready queue.\n");
    enable_interrupts();
    return;
  }

  // Shift the ready queue to remove the thread
  current_index = termination_index;
  while ((current_index + 1) % MAX_THREADS != ready_queue_tail) {
    int next_index = (current_index + 1) % MAX_THREADS;
    ready_queue[current_index] = ready_queue[next_index];
    current_index = next_index;
  }

  // Update the head to point back
  ready_queue[current_index] = nullptr;
  ready_queue_size--;
  ready_queue_tail = (ready_queue_tail + MAX_THREADS - 1) % MAX_THREADS;
  enable_interrupts();
}

/**
 * @brief Kill all zombie threads.
 * 
 */
void Scheduler::kill_zombie_threads() {
  while (zombie_threads_count > 0) {
    zombie_threads[zombie_threads_count - 1]->release_stack();
    delete zombie_threads[zombie_threads_count - 1];
    zombie_threads[zombie_threads_count - 1] = nullptr;
    zombie_threads_count--;
  }
}

// Round-robin Change
/**
 * @brief Preempt the current thread on EOQ. This is called when the EOQ timer interrupt is triggered.
 * 
 */
void Scheduler::end_of_quantum() {
  disable_interrupts();
  if (current_thread == nullptr) { return; }  // Just fall through

  // Add the current thread to the ready queue
  resume(current_thread);
  // Send EOI to the master PIC
  Machine::outportb(0x20, 0x20);
  // Mark that the EOI has been sent so we don't send it again
  InterruptHandler::mark_end_of_interrupt_sent();
  // Yield to the next thread in the ready queue
  yield(); // This will enable interrupts, so we don't need to do it here
}
