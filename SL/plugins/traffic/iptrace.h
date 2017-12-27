#include <types.h>
#include <unistd.h>
#include <sys\socket.h>
#include <sys\ioctl.h>
#include <net\route.h>
#include <net\if_arp.h>
#include <net\if.h>
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include "linkseq.h"
#include "statcnt.h"

#define IPT_OK			0
#define IPT_NOT_ENOUGH_MEMORY	1
#define IPT_END_OF_DATA		2
#define IPT_IO_ERROR		3
#define IPT_OVERFLOW		4
#define IPT_INVALID_DATA	5

typedef struct _IPTIF {
  SEQOBJ		lsObj;

  TID			tid;
  HMTX			hmtxNodes;
  LINKSEQ		lsNodes;

  BOOL			fOnline;
  ULONG			ulEFlagsSave;
  int			iSock;
  LINKSEQ		lsFrags;
  struct ifreq		stIFReq;
  struct pkt_trace_hdr	stTraceHdr;
  ULONG			cbBuf;
  CHAR			acBuf[1526];
} IPTIF, *PIPTIF;

#ifndef IPTRACE_C
extern HMTX		hmtxIPTIF;
extern LINKSEQ		lsIPTIF;
#endif

#define iptLockTIFs() DosRequestMutexSem( hmtxIPTIF, SEM_INDEFINITE_WAIT )
#define iptUnlockTIFs() DosReleaseMutexSem( hmtxIPTIF )

// PLINKSEQ iptGetTIFs()
#define iptGetTIFs() (&lsIPTIF)

// PLINKSEQ iptGetNodes(PIPTIF pTIF)
#define iptGetTIFNodes(pTIF) (&(pTIF)->lsNodes)

// PSZ iptGetTIFName(PIPTIF pTIF)
#define iptGetTIFName(pTIF) (&(pTIF)->stIFReq.ifr_name)

#define iptLockNodes(pTIF) \
  DosRequestMutexSem( (pTIF)->hmtxNodes, SEM_INDEFINITE_WAIT )

#define iptUnlockNodes(pTIF) \
  DosReleaseMutexSem( (pTIF)->hmtxNodes )


// USHORT revUS(USHORT word) - swap bytes of word
USHORT /*inline*/ revUS(USHORT word);
#pragma aux revUS = "xchg ah, al" parm [ ax ] value [ ax ];

// USHORT revUS(USHORT word) - returns dwords's bytes, swapped end for begin.
ULONG /*inline*/ revUL(ULONG ulVal);
#pragma aux revUL = \
  "xchg    ah, al          " \
  "rol     eax, 16         " \
  "xchg    ah, al          " \
  parm [ eax ] value [ eax ];


BOOL iptInit();
VOID iptDone();
PIPTIF iptStart(PSZ pszIFName);
VOID iptStop(PIPTIF pTIF);
PIPTIF iptGetTIF(PSZ pszName);
ULONG iptLoad(PSLOADFN pfnLoad, ULONG ulUser);
ULONG iptStore(PSSTOREFN pfnStore, ULONG ulUser);
BOOL iptLoadFile(PSZ pszFile);
BOOL iptStoreFile(PSZ pszFile);
ULONG iptSnapshot(PIPTSNAPSHOT pSnapshot, ULONG cbSnapshot, PULONG pcbActual);
