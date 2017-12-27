#define INCL_DOSPROFILE
#define INCL_DOSERRORS
#define INCL_DOSMODULEMGR
#include <os2.h>
#include <stdlib.h>
#include <string.h>
#include "sysstate.h"
#include <debug.h>

typedef QSGREC		*PQSGREC;
typedef QSPREC		*PQSPREC;
typedef QSTREC		*PQSTREC;
typedef QSPTRREC	*PQSPTRREC;
typedef QSFREC		*PPQSFREC;
typedef QSFREC		*PQSFREC;
typedef QSSFT		*PQSSFT;
typedef QSLREC		*PQSLREC;
typedef QSEXLREC	*PQSEXLREC;
typedef QSLOBJREC	*PQSLOBJREC;

ULONG ssQueryData(PVOID pPtrRec, ULONG cbPtrRec, ULONG ulFlags)
{
  ULONG			ulRC;

  ulRC = DosQuerySysState( ulFlags, 0, 0, 0,
                           pPtrRec, cbPtrRec );
  if ( ulRC != NO_ERROR )
  {
//    debug( "DosQuerySysState(), rc = %u", ulRC );
  }

  return ulRC;
}

PVOID ssFirstProcess(PVOID pPtrRec)
{
  return ((PQSPTRREC)pPtrRec)->pProcRec;
}

PVOID ssNextProcess(PVOID pPRec)
{
  PQSPREC	pstPRec = (PQSPREC)pPRec;

  pstPRec = (PQSPREC)( (PBYTE)(pstPRec->pThrdRec) +
                       ( pstPRec->cTCB * sizeof(QSTREC) ) );

  return pstPRec->RecType != 1 ? NULL : (PVOID)pstPRec;
}

PVOID ssFindProcess(PVOID pPtrRec, PID pid)
{
  PQSPREC	pstPRec = ((PQSPTRREC)pPtrRec)->pProcRec;

  while( pstPRec->RecType == 1 )
  {
    if ( pstPRec->pid == pid )
      return pstPRec;
    pstPRec = (PQSPREC)( (PBYTE)(pstPRec->pThrdRec) +
                         ( pstPRec->cTCB * sizeof(QSTREC) ) );
  }

  return NULL;
}

VOID ssGetProcess(PVOID pPRec, PSSPROCESS pstProcess)
{
  PQSPREC	pstPRec = (PQSPREC)pPRec;

  if ( pstPRec != NULL )
  {
    pstProcess->pid = pstPRec->pid;
    pstProcess->pidParent = pstPRec->ppid;
    pstProcess->ulType = pstPRec->type;
    pstProcess->ulStatus = pstPRec->stat;
    pstProcess->ulScrGroupId = pstPRec->sgid;
    pstProcess->hModule = pstPRec->hMte;
    pstProcess->cThreads = pstPRec->cTCB;
    pstProcess->ulMaxFiles = ( (PBYTE)pstPRec->pThrdRec -
                               (PBYTE)pstPRec->pFSRec ) >> 1;
  }
}

ULONG ssGetThreadsCount(PVOID pPRec)
{
  PQSPREC	pstPRec = (PQSPREC)pPRec;

  return pstPRec->cTCB;
}

ULONG ssGetThreads(PVOID pPRec, PSSTHREAD paThreads)
{
  PQSPREC	pstPRec = (PQSPREC)pPRec;
  PQSTREC	pstTRec = pstPRec->pThrdRec;
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < pstPRec->cTCB; ulIdx++, paThreads++, pstTRec++ )
  {
    paThreads->tid = pstTRec->tid;
    paThreads->ulPriority = pstTRec->priority;
    paThreads->ulSysTime = pstTRec->systime;
    paThreads->ulUserTime = pstTRec->usertime;
    paThreads->ulState = pstTRec->state;
  }

  return ulIdx;
}

// ssGetPFiles()
// pstFiles: Pointer to the buffer, can be NULL.
// cbFiles: Size of buffer in bytes, can be 0.
// Returns size in bytes of the buffer required to hold all data.
ULONG ssGetPFiles(PVOID pPtrRec, PVOID pPRec, PSSPFILE pstFiles, ULONG cbFiles)
{
  PQSPTRREC	pstPtrRec = (PQSPTRREC)pPtrRec;
  PQSPREC	pstPRec = (PQSPREC)pPRec;
  PUSHORT	pusFN = pstPRec->pFSRec;
  ULONG		ulFN;
  ULONG		cFN;
  ULONG		cbResult = 0;
  ULONG		ulIdx, ulIdxFile;
  PQSFREC	pstFRec;
  PQSSFT	pstSft;
  BOOL		fFound;

  // Files numbers list length.
  cFN = ( (PBYTE)pstPRec->pThrdRec - (PBYTE)pstPRec->pFSRec ) >> 1;
  for( ulIdx = 0; ulIdx < cFN; ulIdx++, pusFN++ )
  {
    ulFN = *pusFN;			// File number in table of files.
    if ( ulFN == 0xFFFF || ulFN == 0 )  // 0xFFFF - handle closed, 0 - skip rec.
      continue;

    // Search file in table by number.
    pstFRec = pstPtrRec->pFSRec;
    fFound = FALSE;
    while( ( pstFRec != NULL ) && ( pstFRec->RecType == 8 ) )
    {
      pstSft = pstFRec->pSft;

      for( ulIdxFile = 0; ulIdxFile < pstFRec->ctSft; ulIdxFile++, pstSft++ )
      {
        if ( pstSft->sfn == ulFN )
        {
          fFound = TRUE;
          break;
        }
      }

      if ( fFound )
        break;
      pstFRec = pstFRec->pNextRec;
    }

    if ( !fFound )
      continue;

    // Store data to the user's buffer.
    if ( ( cbFiles >= ( cbResult + sizeof(SSPFILE) ) ) && ( pstFiles != NULL ) )
    {
      pstFiles->pszName = &((PCHAR)(pstFRec))[sizeof(QSFREC)];
      pstFiles->ulMode = *((PULONG)&pstSft->mode);
      pstFiles->ulFlags = *((PULONG)&pstSft->flags);
      pstFiles->ulAttr = pstSft->attr;
      pstFiles->ulHandle = ulIdx;
      pstFiles++;
    }

    // Increment counter of bytes.
    cbResult += sizeof(SSPFILE);
  }

  return cbResult;
}

PVOID ssFindModule(PVOID pPtrRec, HMODULE hModule)
{
  PQSLREC	pstLRec = ((PQSPTRREC)pPtrRec)->pLibRec;

  while( pstLRec != NULL )
  {
    if ( pstLRec->hmte == hModule )
      return pstLRec;

    pstLRec = pstLRec->pNextRec;
  }

  return NULL;
}

HMODULE ssGetModuleHandle(PVOID pLRec)
{
  PQSLREC	pstLRec = (PQSLREC)pLRec;

  return pstLRec->hmte;
}

VOID ssGetModule(PVOID pLRec, PSSMODULE pstModule)
{
  PQSLREC	pstLRec = (PQSLREC)pLRec;

  if ( pstLRec != NULL )
  {
    pstModule->pszName = pstLRec->pName;
    pstModule->cModules = pstLRec->ctImpMod;
    pstModule->pusModules = (PUSHORT)( (PBYTE)&pstLRec->pName + 4 );
  }
}

