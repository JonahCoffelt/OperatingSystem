/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
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

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/


// Static variables
unsigned int ContFramePool::poolCount = 0;
ContFramePool* ContFramePool::pools[MAX_POOLS];

/**
 * @brief Get the state of a frame  
 * 
 * @param _frame_no Frame number
 * @return FrameState State of the frame
 */
ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {
    unsigned int bitmap_index = _frame_no / 8;
    unsigned char mask = 0x1 << (_frame_no % 8);

    if (!(headmap[bitmap_index] & mask)) { return FrameState::HoS; }
    return ((bitmap[bitmap_index] & mask) == 0) ? FrameState::Used : FrameState::Free;
}

/**
 * @brief Set the state of a frame
 * 
 * @param _frame_no Frame number
 * @param _state State of the frame
 */
void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {
    unsigned int bitmap_index = _frame_no / 8;
    unsigned char mask = 0x1 << (_frame_no % 8);

    switch (_state) {
        case FrameState::Used:
            bitmap[bitmap_index] ^= mask;;
            break;
        case FrameState::Free:
            bitmap[bitmap_index] |= mask;
            headmap[bitmap_index] |= mask;
            break;
        case FrameState::HoS:
            bitmap[bitmap_index] ^= mask;;
            headmap[bitmap_index] ^= mask;;
    }
}

/**
 * @brief Constructor
 * 
 * @param _base_frame_no Base frame number
 * @param _n_frames Number of frames
 * @param _info_frame_no Info frame number
 */
ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    // Initialize the data structures
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    nFreeFrames = nframes;
    info_frame_no = _info_frame_no;

    // Initialize the bitmap
    if(info_frame_no == 0) {
        bitmap = (unsigned char*)(base_frame_no * FRAME_SIZE);
    }
    else {
        bitmap = (unsigned char*)(info_frame_no * FRAME_SIZE);
    }
    // Initialize the headmap
    headmap = (unsigned char*)(bitmap + ((nframes + 7) / 8));

    // Initialize the frames to free
    for (unsigned long i = 0; i < nframes; i++) {
        set_state(i, FrameState::Free);
    }

    // If no info frame is provided, mark the first frames as inaccessible
    if(_info_frame_no == 0) {
        unsigned long nInfo = needed_info_frames(nframes);
        mark_inaccessible(0, nInfo);
        nFreeFrames -= nInfo;
    }

    // Set in the array of pools
    assert(poolCount < MAX_POOLS);
    pools[poolCount] = this;
    poolCount++;
}

/**
 * @brief Get a contiguous sequence of frames
 * 
 * @param _n_frames Number of frames to get
 * @return unsigned long Frame number of the first frame
 */
unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // Check if there are enough free frames
    assert(nFreeFrames >= _n_frames);

    for (unsigned int startFrame = 0; startFrame < nframes; startFrame++) {
        if (get_state(startFrame) != FrameState::Free) { continue; }

        // Check if the next _n_frames are free
        bool allFree = true;
        for (unsigned int offset = 0; offset < _n_frames; offset++) {
            if (get_state(startFrame + offset) != FrameState::Free) { 
                allFree = false; 
                break;
            }
        }

        // If the next _n_frames are not free, continue to the next start frame
        if (!allFree) { 
            continue; 
        }

        // Mark the first frame as the head of the sequence
        set_state(startFrame, FrameState::HoS);
        nFreeFrames--;
        for (unsigned int offset = 1; offset < _n_frames; offset++) {
            set_state(startFrame + offset, FrameState::Used);
            nFreeFrames--;
        }

        return (startFrame + base_frame_no);
    }
    return -1;
}

/**
 * @brief Mark a contiguous sequence of frames as inaccessible
 * 
 * @param _base_frame_no Base frame number
 * @param _n_frames Number of frames to mark as inaccessible
 */
void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    // Mark all frames in the range as being used.
    for(int fno = _base_frame_no; fno < _base_frame_no + _n_frames; fno++){
        set_state(fno, FrameState::Used);
    }
    set_state(_base_frame_no, FrameState::HoS);
}

/**
 * @brief Release a contiguous sequence of frames
 * 
 * @param _first_frame_no Frame number of the first frame
 */
void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    // Get correct pool
    for (ContFramePool* pool : pools) {
        if (_first_frame_no >= pool->base_frame_no && _first_frame_no < pool->base_frame_no + pool->nframes) {
            pool->release_frame(_first_frame_no);
            return;
        }
    }
}

/**
 * @brief Release a frame
 * 
 * @param _first_frame_no Frame number of the first frame
 */
void ContFramePool::release_frame(unsigned long _first_frame_no) {    
    assert(get_state(_first_frame_no - base_frame_no) == FrameState::HoS);

    // Release Head
    unsigned int local_no = _first_frame_no - base_frame_no;
    set_state(local_no, FrameState::Free);
    nFreeFrames++;
    // Release until we get to a head or free frame
    unsigned long fno = local_no + 1;
    while (fno < nframes && get_state(fno) == FrameState::Used) {
        set_state(fno, FrameState::Free);
        fno++;
        nFreeFrames++;
    }
}

/**
 * @brief Get the number of info frames needed
 * 
 * @param _n_frames Number of frames
 * @return unsigned long Number of info frames needed
 */
unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    unsigned long bits = _n_frames * 2;
    unsigned long bytes = (bits + 7) / 8;
    return (bytes + FRAME_SIZE - 1) / FRAME_SIZE;
}
