#define INCL_DOSMODULEMGR
#include <os2.h>

#define SSPROC_TYPE_FS		0
#define SSPROC_TYPE_DOS		1
#define SSPROC_TYPE_VIO		2
#define SSPROC_TYPE_PM		3
#define SSPROC_TYPE_DET		4
#define SSPROC_TYPE_WINVDM	7

#define SSTHREAD_READY		1
#define SSTHREAD_BLOCKED	2
#define SSTHREAD_RUNNING	5
#define SSTHREAD_LOADED		9

#define SS_PROCESS      0x0001
#define SS_SEMAPHORE    0x0002
#define SS_MTE          0x0004
#define SS_FILESYS      0x0008
#define SS_SHMEMORY     0x0010
#define SS_DISK         0x0020
#define SS_NAMEDPIPE    0x0080

typedef struct _SSPROCESS {
  PID		pid;
  PID		pidParent;
  ULONG		ulType;			// SSPROC_TYPE_*
  ULONG		ulStatus;
  ULONG		ulScrGroupId;
  HMODULE	hModule;
  ULONG		cThreads;
  ULONG		ulMaxFiles;
} SSPROCESS, *PSSPROCESS;

typedef struct _SSTHREAD {
  TID		tid;
  ULONG		ulPriority;
  ULONG		ulSysTime;
  ULONG		ulUserTime;
  ULONG		ulState;		// SSTHREAD_*
} SSTHREAD, *PSSTHREAD;

typedef struct _SSPFILE {
  ULONG		ulHandle;
  ULONG		ulFlags;
  ULONG		ulMode;
  ULONG		ulAttr;
  PSZ		pszName;
} SSPFILE, *PSSPFILE;

typedef struct _SSMODULE {
  PSZ		pszName;
  ULONG		cModules;
  PUSHORT	pusModules;
} SSMODULE, *PSSMODULE;

ULONG ssQueryData(PVOID pPtrRec, ULONG cbPtrRec, ULONG ulFlags);
PVOID ssFirstProcess(PVOID pPtrRec);
PVOID ssNextProcess(PVOID pPRec);
PVOID ssFindProcess(PVOID pPtrRec, PID pid); // returns pPRec or NULL
ULONG ssGetThreadsCount(PVOID pPRec);
VOID ssGetProcess(PVOID pPRec, PSSPROCESS pstProcess);
ULONG ssGetThreads(PVOID pPRec, PSSTHREAD paThreads); // return number of threads
ULONG ssGetPFiles(PVOID pPtrRec, PVOID pPRec, PSSPFILE paFiles, ULONG cbFiles);
PVOID ssFindModule(PVOID pPtrRec, HMODULE hModule); // returns pLRec or NULL
HMODULE ssGetModuleHandle(PVOID pLRec);
VOID ssGetModule(PVOID pLRec, PSSMODULE pstModule);
