#include <os2.h>
#include "linkseq.h"

typedef unsigned long long	ULLONG;
typedef ULLONG			*PULLONG;

typedef ULONG (*PSLOADFN)(ULONG cbBuf, PVOID pBuf, ULONG ulUser);
typedef ULONG (*PSSTOREFN)(ULONG cbBuf, PVOID pBuf, ULONG ulUser);

typedef struct _IPTIF *PIPTIF;

// STPROTO_xxxx : SCNODE.ulProto
#define STPROTO_ICMP		1
#define STPROTO_TCP		2
#define STPROTO_UDP		4
#define STPROTO_ANY		(STPROTO_ICMP | STPROTO_TCP | STPROTO_UDP)

// SC_ADDR_xxxxxx : SCADDR.ulPresForm
#define SC_ADDR_RANGE		0
#define SC_ADDR_MASK_BITS	1
#define SC_ADDR_MASK_VAL	2

typedef struct _SCPKTINFO {
  BOOL		fReversChk;
  ULONG		ulProto;	// STPROTO_xxxx
  ULONG		ulSrcAddr;
  ULONG		ulDstAddr;
  ULONG		ulSrcPort;
  ULONG		ulDstPort;
  ULONG		cbIPPacket;
  ULONG		cbPacket;
} SCPKTINFO, *PSCPKTINFO;


typedef struct _SCADDR {
  ULONG		ulFirst;
  ULONG		ulLast;
  ULONG		ulPresForm;	// SC_ADDR_xxxxxx
} SCADDR, *PSCADDR;

typedef struct _SCPORT {
  ULONG		ulFirst;
  ULONG		ulLast;
} SCPORT, *PSCPORT;

typedef struct _SCNODE {
  SEQOBJ	lsObj;
  LINKSEQ	lsNodes;

  PSZ		pszName;
  PSZ		pszComment;
  BOOL		fHdrIncl;
  BOOL		fStop;
  ULONG		ulProto;	// STPROTO_xxxx
  PSCADDR	pSrcAddrLst;
  PSCADDR	pDstAddrLst;
  PSCPORT	pSrcPortLst;
  PSCPORT	pDstPortLst;

  ULLONG	ullSent;
  ULLONG	ullRecv;

  PVOID		pUser;
} SCNODE, *PSCNODE;


typedef struct _IPTSNAPSHOT {
  PIPTIF	pTIF;
  PSCNODE	pNode;
  ULLONG	ullSent;
  ULLONG	ullRecv;
} IPTSNAPSHOT, *PIPTSNAPSHOT;

typedef struct _SCSNAPSHOTPARAM {
  PIPTIF	pTIF;
  PIPTSNAPSHOT	pSnapshot;
  ULONG		cbSnapshot;
  ULONG		cbActual;
} SCSNAPSHOTPARAM, *PSCSNAPSHOTPARAM;


// scTraceQuery() result record.

typedef struct _SCTRACEPKT {
  ULONG		ulIndex;
  SCPKTINFO	stPktInfo;
} SCTRACEPKT, *PSCTRACEPKT;

#define ULFromIP(a,b,c,d) \
  ((ULONG)(a) << 24) | ((ULONG)(b) << 16) | ((ULONG)(c) << 8) | ((d) & 0x0F)

VOID scInit();
VOID scDone();
PSCNODE scNewNode(PSCNODE pSrcNode);
VOID scFreeNode(PSCNODE pSrcNode);
VOID scCalc(PLINKSEQ plsNodes, PSCPKTINFO pInfo);
VOID scPrintNode(PSCNODE pNode, ULONG ulLevel);
VOID scPrint(PLINKSEQ plsNodes, ULONG ulLevel);
ULONG scLoad(PSLOADFN pfnLoad, ULONG ulUser, PLINKSEQ plsNodes);
ULONG scStore(PSSTOREFN pfnStore, ULONG ulUser, PLINKSEQ plsNodes);
VOID scSnapshot(PLINKSEQ plsNodes, PSCSNAPSHOTPARAM pParam);
VOID scResetCounters(PLINKSEQ plsNodes);

// Parse filter nodes.

ULONG scIPAddrToStr(ULONG ulIPAddr, ULONG cbBuf, PCHAR pcBuf);
ULONG scAddrToStr(PSCADDR pAddr, ULONG cbBuf, PCHAR pcBuf);
ULONG scPortLstToStr(PSCPORT pPort, ULONG cbBuf, PCHAR pcBuf);

// Build filter nodes.

BOOL scStrToIPAddr(PULONG pulAddr, PSZ pszStr, PCHAR *ppcEnd);
BOOL scStrToAddr(PSZ pszStr, PSCADDR pAddr);
BOOL scStrToPortLst(PSZ pszStr, ULONG ulMaxPorts, PSCPORT pPortLst);

// Node tracing.

BOOL scTraceStart(PSCNODE pNode, ULONG ulMaxRecords);
VOID scTraceStop();
// scTraceQuery()
// Returns number of records placed in paPackets.
ULONG scTraceQuery(PSCNODE *ppNode, ULONG cPackets, PSCTRACEPKT paPackets);
