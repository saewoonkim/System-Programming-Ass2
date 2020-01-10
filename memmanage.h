
#ifndef _MEMMANAGE_H
#define _MEMMANAGE_H

#include <stdio.h>

// mmap, shared memory
void sharedMemInit();

// get a block memory
void* getSharedMemBlock(ssize_t size);

// free
void sharedMemFree();

#endif // _MEMMANAGE_H
