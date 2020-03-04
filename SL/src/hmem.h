#ifndef HMEM_H
#define HMEM_H

// IS_HIGH_PTR(p) is TRUE when p points to the high memory.
#define IS_HIGH_PTR(p)   ((unsigned long int)(p) >= (512*1024*1024))

#if 0

#define INCL_DOSERRORS
#include <os2.h>

ULONG hmAllocateBlock(PVOID *ppMem, ULONG ulSize);
ULONG hmFreeBlock(PVOID pMem);

#endif

int hmInit();
void hmDone();
void* hmMAlloc(unsigned long ulSize);
void  hmFree(void *pMem);

#endif // HMEM_H
