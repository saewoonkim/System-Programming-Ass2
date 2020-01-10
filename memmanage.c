#include "memmanage.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *global_mem = NULL;
int *offset = NULL;

int MemSize = 1024 * 100000;


void sharedMemInit() {
    global_mem = (char*)mmap(NULL, MemSize,PROT_READ | PROT_WRITE,
                             MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    offset = (int*)(global_mem);
    *offset = sizeof(int);
}

void* getSharedMemBlock(ssize_t size) {
    void *temp = (void*)(global_mem + *offset);
    *offset += size;
    return temp;
}

void sharedMemFree() {
    munmap(global_mem , MemSize);
}


