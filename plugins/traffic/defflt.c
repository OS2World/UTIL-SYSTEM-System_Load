/*
  Public function:

    VOID deffltCreate()

    Creates sample filter tree. Called from traffic.c/dsInit() if filter was
    not loaded from file.
*/

#include <stdio.h>
#include <string.h>
#include <types.h>
#include <sys\socket.h>
#include <sys\ioctl.h>
#include <net\route.h>
#include <net\if.h>
#include <net\if_arp.h>
#include <unistd.h>
#define INCL_DOSSEMAPHORES
#include <ds.h>
#include "iptrace.h"
#include "linkseq.h"
#include <debug.h>

typedef struct _IFIPADDR {
  ULONG			ulAddr;
  ULONG			ulMask;
  BOOL			fAlias;
} IFIPADDR, *PIFIPADDR;

typedef struct _IFADDR {
  struct _IFADDR	*pNext;
  CHAR			szIFName[IFNAMSIZ];
  ULONG			cAddr;
  IFIPADDR		aAddr[1];
} IFADDR, *PIFADDR;


#define MAX_STAT_ADDR		64

#pragma pack(1)

typedef struct _IOSTATATADDR {
  ULONG			ulAddr;
  USHORT		usIFIdx;
  ULONG			ulMask;
  ULONG			ulBroadcast;
} IOSTATATADDR, *PIOSTATATADDR;

typedef struct _IOSTATAT {
  USHORT		cAddr;
  IOSTATATADDR		aAddr[MAX_STAT_ADDR];
} IOSTATAT, *PIOSTATAT;

#pragma pack()

// BOOL _queryIFAddrList(PSZ pszIF, int iSock, PVOID pAllIFAddr,
//                       PULONG pcbBuf, PCHAR *ppcBuf)
//
// Called from queryAddrList(). Fills the buffer *ppcBuf with one record IFADDR
// for network interface pszIF.

static BOOL _queryIFAddrList(PSZ pszIF, int iSock, PIOSTATAT pAllIFAddr,
                             PULONG pcbBuf, PCHAR *ppcBuf)
{
  int		iRC;
  struct ifreq	stIFReq;
  ULONG		ulIdx;
  LONG		lIFIndex;
  ULONG		ulAddr, ulMask, ulBroadcast;
  PIFIPADDR	pIFIPAddr;

  strcpy( &stIFReq.ifr_name, pszIF );
  iRC = ioctl( iSock, SIOCGIFADDR, &stIFReq );
  if ( iRC < 0 )
    return TRUE;
  ulAddr = *((PULONG)&stIFReq.ifr_addr.sa_data[2]);

  iRC = ioctl( iSock, SIOCGIFNETMASK, &stIFReq );
  if ( iRC < 0 )
    return TRUE;
  ulMask = revUL( *((PULONG)&stIFReq.ifr_addr.sa_data[2]) );

  iRC = ioctl( iSock, SIOCGIFBRDADDR, &stIFReq );
  if ( iRC < 0 )
    return TRUE;
  ulBroadcast = *((PULONG)&stIFReq.ifr_addr.sa_data[2]);


  lIFIndex = -1;
  for( ulIdx = 0; ulIdx < pAllIFAddr->cAddr; ulIdx++ )
  {
    if ( ( pAllIFAddr->aAddr[ulIdx].ulAddr == ulAddr ) &&
         ( pAllIFAddr->aAddr[ulIdx].ulMask == ulMask ) &&
         ( pAllIFAddr->aAddr[ulIdx].ulBroadcast == ulBroadcast ) )
    {
      lIFIndex = pAllIFAddr->aAddr[ulIdx].usIFIdx;
      break;
    }
  }
  if ( lIFIndex == -1 )
    return TRUE;

  if ( *pcbBuf < sizeof(IFADDR) )
    return FALSE;
  *pcbBuf -= ( sizeof(IFADDR) - sizeof(IFIPADDR) );

  strcpy( &((PIFADDR)*ppcBuf)->szIFName, &stIFReq.ifr_name );
  ((PIFADDR)*ppcBuf)->cAddr = 0;
  pIFIPAddr = &((PIFADDR)*ppcBuf)->aAddr;

  for( ulIdx = 0; ulIdx < pAllIFAddr->cAddr; ulIdx++ )
  {
    if ( lIFIndex != pAllIFAddr->aAddr[ulIdx].usIFIdx )
      continue;

    if ( *pcbBuf < sizeof(IFADDR) )
      return FALSE;

    pIFIPAddr->ulAddr = pAllIFAddr->aAddr[ulIdx].ulAddr;
    pIFIPAddr->ulMask = revUL( pAllIFAddr->aAddr[ulIdx].ulMask );
    pIFIPAddr->fAlias = pAllIFAddr->aAddr[ulIdx].ulAddr != ulAddr;
    pIFIPAddr++;
    ((PIFADDR)*ppcBuf)->cAddr++;
  }

  *ppcBuf = (PCHAR)pIFIPAddr;
  return TRUE;
}

// BOOL queryAddrList(ULONG cbBuf, PCHAR pcBuf)
//
// Fill buffer pcBuf with records IFADDR. Returns TRUE on success.

static BOOL queryAddrList(ULONG cbBuf, PCHAR pcBuf)
{
  int		iSock = socket( PF_INET, SOCK_STREAM, 0 );
  int		iRC;
  IOSTATAT	stAllIFAddr;
  CHAR		szIFName[IFNAMSIZ];
  static PSZ	pszIFName[3] = { "lan%u", "ppp%u", "sl%u" };
  ULONG		ulName, ulIdx;
  PIFADDR	*ppNext = NULL;

  if ( cbBuf < sizeof(IFADDR) )
    return FALSE;

  iRC = os2_ioctl( iSock, SIOSTATAT, (caddr_t)&stAllIFAddr, sizeof(stAllIFAddr) );
  if ( iRC < 0 )
    return FALSE;

  for( ulName = 0; ulName < sizeof(pszIFName) / sizeof(PSZ); ulName++ )
  {
    for( ulIdx = 0; ulIdx < 42; ulIdx++ )
    {
      PCHAR	pcIFBegin = pcBuf;

      sprintf( &szIFName, pszIFName[ulName], ulIdx );
      if ( !_queryIFAddrList( &szIFName, iSock, &stAllIFAddr, &cbBuf, &pcBuf ) )
        return FALSE;

      if ( pcIFBegin != pcBuf )
      {
        if ( ppNext != NULL )
          *ppNext = (PIFADDR)pcIFBegin;
        ppNext = (PIFADDR *)pcIFBegin;
      }
    }
  }

  soclose( iSock );
  if ( ppNext != NULL )
    *ppNext = NULL;

  return TRUE;
}


// _createIFFilter(PIFADDR pIFAddr)
//
// Runs tracing and creates default filter tree for given network interface.

static BOOL _createIFFilter(PIFADDR pIFAddr)
{
  BOOL		fSuccess = FALSE;
		// Start tracing on interface.
  PIPTIF	pTIF = iptStart( &pIFAddr->szIFName );
  SCNODE	stSCNode;
  CHAR		szName[128];
  CHAR		szComment[256];
  SCADDR	astSrcAddr[256];
  SCADDR	astDstAddr[256];
  SCPORT	astSrcPort[256];
  SCPORT	astDstPort[256];
  ULONG		ulIdx;
  PSCNODE	pSCNode0, pSCNode1, pSCNode2;

  if ( pTIF == NULL )
  {
    debug( "iptStart(%s) failed", &pIFAddr->szIFName );
    return FALSE;
  }

  stSCNode.pszName = &szName;
  stSCNode.pszComment = &szComment;
  stSCNode.ullSent = 0;
  stSCNode.ullRecv = 0;
  stSCNode.pUser = NULL;

  iptLockNodes( pTIF );
  do
  {
    // Make node pSCNode0 "All packets from XXXXX" - child for interface.

    sprintf( &szName, "All packets from %s", &pIFAddr->szIFName );
    strcpy( &szComment,
            "This is created automatically example. You can use it to create "
            "your own filter." );

    stSCNode.fHdrIncl = TRUE;
    stSCNode.fStop = TRUE;
    stSCNode.ulProto = STPROTO_ANY;

    for( ulIdx = 0; ulIdx < min( pIFAddr->cAddr, ARRAYSIZE(astSrcAddr) - 1 );
         ulIdx++ )
    {
      astSrcAddr[ulIdx].ulFirst = revUL( pIFAddr->aAddr[ulIdx].ulAddr );
      astSrcAddr[ulIdx].ulLast = astSrcAddr[ulIdx].ulFirst;
      astSrcAddr[ulIdx].ulPresForm = SC_ADDR_RANGE;
    }
    astSrcAddr[ulIdx].ulFirst = 0;
    astSrcAddr[ulIdx].ulLast = 0;

    stSCNode.pSrcAddrLst = &astSrcAddr;
    stSCNode.pDstAddrLst = NULL;
    stSCNode.pSrcPortLst = NULL;
    stSCNode.pDstPortLst = NULL;

    // Allocate new filter node from stSCNode.
    pSCNode0 = scNewNode( &stSCNode );
    if ( pSCNode0 == NULL )
      break;
    // Insert node to interface.
    lnkseqAdd( iptGetTIFNodes( pTIF ), pSCNode0 );

    // Make node "Network Interfaces:XXXX, Local network" - child for
    // pSCNode0 "All packets from XXXX".

    sprintf( &szName, "Network Interfaces:%s, Local network", &pIFAddr->szIFName );
    strcpy( &szComment,
            "This node will catch all packets on the LAN. Such packets will "
            "pass through the child nodes but will not pass through the "
            "subsequent nodes. This is defined by switch \"Stop scan nodes at "
            "this level\"." );

    stSCNode.fHdrIncl = TRUE;
    stSCNode.fStop = TRUE;
    stSCNode.ulProto = STPROTO_ANY;

    for( ulIdx = 0; ulIdx < min( pIFAddr->cAddr, ARRAYSIZE(astSrcAddr) - 1 );
         ulIdx++ )
    {
      astDstAddr[ulIdx].ulFirst =
        revUL( pIFAddr->aAddr[ulIdx].ulAddr & pIFAddr->aAddr[ulIdx].ulMask );
      astDstAddr[ulIdx].ulLast =
        revUL( pIFAddr->aAddr[ulIdx].ulAddr | ~pIFAddr->aAddr[ulIdx].ulMask );
      astDstAddr[ulIdx].ulPresForm = SC_ADDR_MASK_VAL;
    }
    astDstAddr[ulIdx].ulFirst = 0;
    astDstAddr[ulIdx].ulLast = 0;

    stSCNode.pSrcAddrLst = NULL;
    stSCNode.pDstAddrLst = &astDstAddr;
    stSCNode.pSrcPortLst = NULL;
    stSCNode.pDstPortLst = NULL;

    // Allocate new filter node from stSCNode.
    pSCNode1 = scNewNode( &stSCNode );
    if ( pSCNode1 == NULL )
      break;
    // Insert node to interface.
    lnkseqAdd( &pSCNode0->lsNodes, pSCNode1 );

    // Make node "Local traffic:XXXX, ICMP" - child for
    // pSCNode1 "Network Interfaces:XXXX, Local network".

    sprintf( &szName, "Local traffic:%s, ICMP", &pIFAddr->szIFName );
    szComment[0] = '\0';
    stSCNode.fHdrIncl = FALSE;
    stSCNode.fStop = TRUE;
    stSCNode.ulProto = STPROTO_ICMP;
    stSCNode.pSrcAddrLst = NULL;
    stSCNode.pDstAddrLst = NULL;
    stSCNode.pSrcPortLst = NULL;
    stSCNode.pDstPortLst = NULL;

    pSCNode2 = scNewNode( &stSCNode );
    if ( pSCNode2 != NULL )
      lnkseqAdd( &pSCNode1->lsNodes, pSCNode2 );

    // Make node "Local traffic:XXXX, TCP" - child for
    // pSCNode1 "Network Interfaces:XXXX, Local network".

    sprintf( &szName, "Local traffic:%s, TCP", &pIFAddr->szIFName );
    stSCNode.ulProto = STPROTO_TCP;
    pSCNode2 = scNewNode( &stSCNode );
    if ( pSCNode2 != NULL )
      lnkseqAdd( &pSCNode1->lsNodes, pSCNode2 );

    // Make node "Local traffic:XXXX, DNS" - child for
    // pSCNode1 "Network Interfaces:XXXX, Local network".

    sprintf( &szName, "Local traffic:%s, DNS", &pIFAddr->szIFName );
    stSCNode.ulProto = STPROTO_TCP | STPROTO_UDP;
    astDstPort[0].ulFirst = 53;
    astDstPort[0].ulLast = 53;
    astDstPort[1].ulFirst = 0;
    astDstPort[1].ulLast = 0;
    stSCNode.pDstPortLst = &astDstPort;
    pSCNode2 = scNewNode( &stSCNode );
    if ( pSCNode2 != NULL )
      lnkseqAdd( &pSCNode1->lsNodes, pSCNode2 );

    // Make node "Network Interfaces:XXXX, External traffic" - child for
    // pSCNode0 "All packets from XXXX".

    sprintf( &szName, "Network Interfaces:%s, External traffic", &pIFAddr->szIFName );
    szComment[0] = '\0';

    stSCNode.fHdrIncl = TRUE;
    stSCNode.fStop = TRUE;
    stSCNode.ulProto = STPROTO_ANY;
    stSCNode.pSrcAddrLst = NULL;
    stSCNode.pDstAddrLst = NULL;
    stSCNode.pSrcPortLst = NULL;
    stSCNode.pDstPortLst = NULL;

    pSCNode1 = scNewNode( &stSCNode );
    if ( pSCNode1 == NULL )
      break;
    lnkseqAdd( &pSCNode0->lsNodes, pSCNode1 );

    // Make node "External traffic:XXXX, HTTP" - child for
    // pSCNode1 "Network Interfaces:XXXX, External traffic".

    sprintf( &szName, "External traffic:%s, HTTP/HTTPS", &pIFAddr->szIFName );
    stSCNode.ulProto = STPROTO_TCP;
    astDstPort[0].ulFirst = 80;
    astDstPort[0].ulLast = 80;
    astDstPort[1].ulFirst = 443;
    astDstPort[1].ulLast = 443;
    astDstPort[2].ulFirst = 0;
    astDstPort[2].ulLast = 0;
    stSCNode.pDstPortLst = &astDstPort;
    pSCNode2 = scNewNode( &stSCNode );
    if ( pSCNode2 != NULL )
      lnkseqAdd( &pSCNode1->lsNodes, pSCNode2 );

    // Make node "External traffic:XXXX, DNS" - child for
    // pSCNode1 "Network Interfaces:XXXX, External traffic".

    sprintf( &szName, "External traffic:%s, DNS", &pIFAddr->szIFName );
    stSCNode.ulProto = STPROTO_TCP | STPROTO_UDP;
    astDstPort[0].ulFirst = 53;
    astDstPort[0].ulLast = 53;
    astDstPort[1].ulFirst = 0;
    astDstPort[1].ulLast = 0;
    stSCNode.pDstPortLst = &astDstPort;
    pSCNode2 = scNewNode( &stSCNode );
    if ( pSCNode2 != NULL )
      lnkseqAdd( &pSCNode1->lsNodes, pSCNode2 );

    // "Sample" node.

    if ( pIFAddr->pNext == NULL )
    {
      sprintf( &szName, "Hidden node:", &pIFAddr->szIFName );
      strcpy( &szComment,
              "Hidden node name followed by a colon. This node will not show "
              "up in the list.\r\nTip: You can change the order of addresses "
              " in lists by pressing PLUS and MINUS on numpad." );

      stSCNode.fHdrIncl = FALSE;
      stSCNode.fStop = FALSE;
      stSCNode.ulProto = STPROTO_UDP;

      astSrcAddr[0].ulFirst = ULFromIP(192,168,33,10);
      astSrcAddr[0].ulLast = ULFromIP(192,168,33,15);
      astSrcAddr[0].ulPresForm = SC_ADDR_RANGE;
      astSrcAddr[1].ulFirst = ULFromIP(192,168,35,0);
      astSrcAddr[1].ulLast = ULFromIP(192,168,35,255);
      astSrcAddr[1].ulPresForm = SC_ADDR_MASK_BITS;
      astSrcAddr[2].ulFirst = ULFromIP(192,168,37,0);
      astSrcAddr[2].ulLast = ULFromIP(192,168,37,255);
      astSrcAddr[2].ulPresForm = SC_ADDR_MASK_VAL;
      astSrcAddr[3].ulFirst = 0;
      astSrcAddr[3].ulLast = 0;

      astDstAddr[0].ulFirst = ULFromIP(192,168,39,123);
      astDstAddr[0].ulLast = ULFromIP(192,168,39,123);
      astDstAddr[0].ulPresForm = SC_ADDR_RANGE;
      astDstAddr[1].ulFirst = 0;
      astDstAddr[1].ulLast = 0;

      astSrcPort[0].ulFirst = 1025;
      astSrcPort[0].ulLast = 65535;
      astSrcPort[1].ulFirst = 0;
      astSrcPort[1].ulLast = 0;

      astDstPort[0].ulFirst = 10;
      astDstPort[0].ulLast = 10;
      astDstPort[1].ulFirst = 20;
      astDstPort[1].ulLast = 20;
      astDstPort[2].ulFirst = 100;
      astDstPort[2].ulLast = 123;
      astDstPort[3].ulFirst = 0;
      astDstPort[3].ulLast = 0;

      stSCNode.pSrcAddrLst = &astSrcAddr;
      stSCNode.pDstAddrLst = &astDstAddr;
      stSCNode.pSrcPortLst = &astSrcPort;
      stSCNode.pDstPortLst = &astDstPort;

      // Allocate new filter node from stSCNode.
      pSCNode0 = scNewNode( &stSCNode );
      if ( pSCNode0 == NULL )
        break;
      // Insert node to interface.
      lnkseqAdd( iptGetTIFNodes( pTIF ), pSCNode0 );
    }

    fSuccess = TRUE;
  }
  while( FALSE );

  iptUnlockNodes( pTIF );
  return fSuccess;
}


VOID deffltCreate()
{
  CHAR		acBuf[4096];
  PIFADDR	pIFAddr = (PIFADDR)acBuf;

  // Query all network interfaces ip-adresses.
  if ( !queryAddrList( sizeof(acBuf), &acBuf ) )
  {
    debug( "queryAddrList() failed" );
    return;
  }

  // Create sample filter tree for each interface.
  while( pIFAddr != NULL )
  {
    _createIFFilter( pIFAddr );
    pIFAddr = pIFAddr->pNext;
  }
}
