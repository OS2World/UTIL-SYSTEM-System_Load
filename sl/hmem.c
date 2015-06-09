#define INCL_DOSMEMMGR
#define INCL_DOSERRORS
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include "hmem.h"
#include "debug.h"

// MIN_POOL_SIZE
// Size of one memory pool.
#define MIN_POOL_SIZE     0x100000

// MAX_POOLS
// The maximum number of memory pools.
#define MAX_POOLS     32

static ULONG	cPools = 0;
static HMTX	hmtxPools = NULLHANDLE;
static PVOID	apPools[MAX_POOLS];

// User's memory blocks header.
typedef struct _BLKINF {
  PVOID		pPool;
  ULONG		cbBlock;
} BLKINF, *PBLKINF;


// ULONG hmAllocateBlock(PVOID *ppMem, ULONG ulSize)
//
// Allocates space for an object of ulSize bytes. A pointer to the allocated
// object placed in ppMem. Returns OS/2 error code.

ULONG hmAllocateBlock(PVOID *ppMem, ULONG ulSize)
{
  ULONG		ulRC;
  LONG		lIdx;
  PVOID		pBlock;
  ULONG		cbBlock;

  if ( ulSize == 0 )
  {
    *ppMem = NULL;
    return NO_ERROR;
  }

  cbBlock = ulSize + sizeof(BLKINF);

  // Try to allocate block in existing pools.
  ulRC = DosRequestMutexSem( hmtxPools, SEM_INDEFINITE_WAIT );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosRequestMutexSem(), rc = %u", ulRC );
    return ulRC;
  }
  lIdx = cPools - 1;
  DosReleaseMutexSem( hmtxPools );

  ulRC = ERROR_DOSSUB_NOMEM;
  for( ; lIdx >= 0; lIdx-- )
  {
    ulRC = DosSubAllocMem( apPools[lIdx], &pBlock, cbBlock );
    if ( ulRC != ERROR_DOSSUB_NOMEM )
      break;
  }

  if ( ( ulRC == ERROR_DOSSUB_NOMEM ) && ( cPools < MAX_POOLS ) )
  {
    ULONG	cbPool;
    PVOID	pPool;

    if ( cbBlock < (MIN_POOL_SIZE - 64) )
      cbPool = MIN_POOL_SIZE;
    else
    {
      cbPool = cbBlock + 64;
      if ( (cbPool & 0xFFFF) != 0 )
        cbPool = (cbPool & ~0xFFFF) + 0x10000; // round up to 65536
    }

    // First, try to allocate pool in high memory.
    ulRC = DosAllocMem( &pPool, cbPool, PAG_READ | PAG_WRITE | OBJ_ANY );
    if ( ulRC == NO_ERROR )
      debug( "High memory%s used for a new pool (ptr: %p, size: %u)",
             IS_HIGH_PTR(pPool) ? "" : " NOT", pPool, cbPool );
    else
    {
      debug( "Cannot allocate pool in high memory: DosAllocMem(,%u,), rc = %u", cbPool, ulRC );

      // Failed - try to allocate pool in low memory.
      ulRC = DosAllocMem( &pPool, cbPool, PAG_READ | PAG_WRITE );
      if ( ulRC == NO_ERROR )
        debug( "Low memory used for a new pool (ptr: %p, size: %u)",
               pPool, cbPool );
      else
      {
        debug( "...and in low memory too, rc = %u", cbPool, ulRC );
        return ulRC;
      }
    }

    ulRC = DosSubSetMem( pPool,
                         DOSSUB_INIT | DOSSUB_SPARSE_OBJ | DOSSUB_SERIALIZE,
                         cbPool );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosSubSetMem(), rc = %u\n", ulRC );
      DosFreeMem( pPool );
      return ulRC;
    }

    // Store now pool to the list.
    ulRC = DosRequestMutexSem( hmtxPools, SEM_INDEFINITE_WAIT );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosRequestMutexSem(), rc = %u", ulRC );
      DosSubUnsetMem( pPool );
      DosFreeMem( pPool );
      return ulRC;
    }
    lIdx = cPools++;
    apPools[lIdx] = pPool;
    DosReleaseMutexSem( hmtxPools );

    // Allocate block in the new pool.
    ulRC = DosSubAllocMem( pPool, &pBlock, cbBlock );
  }

  if ( ulRC != NO_ERROR )
    return ulRC;

  // Fill block's header and return pointer to the first byte after it.
  ((PBLKINF)pBlock)->cbBlock = cbBlock;
  ((PBLKINF)pBlock)->pPool = apPools[lIdx];
  *ppMem = (PVOID)( ((PBLKINF)pBlock) + 1 );

  debugIncNumb( "high mem", cbBlock );
//  debug( "Allocated: ptr: %p, size: %u", *ppMem, ulSize );
  return NO_ERROR;
}

// ULONG hmFreeBlock(PVOID pMem)
//
// Deallocates the memory block located by the argument pMem which points to a
// memory block previously allocated by hmAllocateBlock().
// Returns OS/2 error code.

ULONG hmFreeBlock(PVOID pMem)
{
  PBLKINF	pBlock = ((PBLKINF)pMem) - 1;
  ULONG		cbBlock = pBlock->cbBlock;
  ULONG		ulRC = DosSubFreeMem( pBlock->pPool, (PVOID)pBlock, cbBlock );

  if ( ulRC != NO_ERROR )
    debug( "DosSubFreeMem(), rc = %u", ulRC );
  else
  {
    debugIncNumb( "high mem", -cbBlock );
  }

  return ulRC;
}


int hmInit()
{
  ULONG		ulRC;

  // Already initialized?
  if ( ( hmtxPools != NULLHANDLE ) || ( cPools != 0 ) )
  {
    debug( "Already initialized" );
    return -1;
  }

  ulRC = DosCreateMutexSem( NULL, &hmtxPools, 0, FALSE );
  if ( ulRC != NO_ERROR )
    return -1;

  return 0;
}

void hmDone()
{
  ULONG		ulIdx;
  ULONG		ulRC;

  if ( hmtxPools == NULLHANDLE )
  {
    debug( "Was not initialized" );
    return;
  }

  for( ulIdx = 0; ulIdx < cPools; ulIdx++ )
  {
    ulRC = DosSubUnsetMem( apPools[ulIdx] );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosSubUnsetMem(), rc = %u", ulRC );
    }

    ulRC = DosFreeMem( apPools[ulIdx] );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosFreeMem(), rc = %u", ulRC );
    }
  }

  cPools = 0;
  DosCloseMutexSem( hmtxPools );
  hmtxPools = NULLHANDLE;
}

void* hmMAlloc(unsigned long ulSize)
{
  PVOID		pMem;
  ULONG		ulRC = hmAllocateBlock( &pMem, ulSize );

  if ( ulRC != NO_ERROR )
  {
    debug( "hmAllocateBlock(), rc = %u", ulRC );
    return NULL;
  }

  return pMem;
}

void hmFree(void *pMem)
{
  ULONG		ulRC = hmFreeBlock( pMem );

  if ( ulRC != NO_ERROR )
  {
    debug( "hmFreeBlock(), rc = %u", ulRC );
  }
}
