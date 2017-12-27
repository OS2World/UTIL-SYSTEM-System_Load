/*
  Filter/counters for network packets tracing.
*/

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <arpa\inet.h>
#define INCL_DOSERRORS
#include "iptrace.h"
#include "strutils.h"
#include <debug.h>

#define NODE_HEADER	'00vN'

static PSCNODE		pTraceNode = NULL;
static ULONG		ulMaxTracePkt = 0;
static PSCTRACEPKT	paTracePkt = NULL;
static ULONG		ulNextTracePkt;
static ULONG		ulTracePktIdx;
static HMTX		hmtxTrace = NULLHANDLE;

static BOOL _testAddr(PSCADDR pAddr, ULONG ulAddr)
{
  for( ; pAddr->ulLast != 0; pAddr++ )
  {
    if ( ( ulAddr >= pAddr->ulFirst ) && ( ulAddr <= pAddr->ulLast ) )
      return TRUE;
  }

  return FALSE;
}

static BOOL _testPort(PSCPORT pPort, ULONG ulPort)
{
  for( ; pPort->ulLast != 0; pPort++ )
  {
    if ( ( ulPort >= pPort->ulFirst ) && ( ulPort <= pPort->ulLast ) )
      return TRUE;
  }

  return FALSE;
}

static PSCADDR _newAddrLst(PSCADDR pAddrLst)
{
  PSCADDR	pAddr;
  ULONG		cAddr = 0;
  ULONG		cbAddr;

  if ( pAddrLst == NULL )
    return NULL;

  for( pAddr = pAddrLst; pAddr->ulLast != 0; pAddr++ )
    cAddr++;

  if ( cAddr == 0 )
    return NULL;

  cbAddr = ( cAddr + 1 ) * sizeof(SCADDR);
  pAddr = debugMAlloc( cbAddr );
  if ( pAddr != NULL )
    memcpy( pAddr, pAddrLst, cbAddr );

  return pAddr;
}

static PSCPORT _newPortLst(PSCPORT pPortLst)
{
  PSCPORT	pPort;
  ULONG		cPort = 0;
  ULONG		cbPort;

  if ( pPortLst == NULL )
    return NULL;

  for( pPort = pPortLst; pPort->ulLast != 0; pPort++ )
    cPort++;

  if ( cPort == 0 )
    return NULL;

  cbPort = ( cPort + 1 ) * sizeof(SCPORT);
  pPort = debugMAlloc( cbPort );
  if ( pPort != NULL )
    memcpy( pPort, pPortLst, cbPort );

  return pPort;
}

VOID scInit()
{
  ULONG		ulRC;

  if ( hmtxTrace == NULLHANDLE )
  {
    ulRC = DosCreateMutexSem( NULL, &hmtxTrace, 0, FALSE );
    if ( ulRC != NO_ERROR )
      debug( "DosCreateMutexSem(), rc = %u", ulRC );
  }
}

VOID scDone()
{
  if ( hmtxTrace != NULLHANDLE )
  {
    DosCloseMutexSem( hmtxTrace );
    hmtxTrace = NULLHANDLE;
  }
}


PSCNODE scNewNode(PSCNODE pSrcNode)
{
  PSCNODE	pNode = debugMAlloc( sizeof(SCNODE) );

  if ( pNode == NULL )
    return NULL;

  lnkseqInit( &pNode->lsNodes );
  pNode->pszName =
    ( pSrcNode->pszName == NULL ) || ( *pSrcNode->pszName == '\0' ) ?
        NULL : debugStrDup( pSrcNode->pszName );
  pNode->pszComment =
    ( pSrcNode->pszComment == NULL ) || ( *pSrcNode->pszComment == '\0' ) ?
        NULL : debugStrDup( pSrcNode->pszComment );
  pNode->fHdrIncl = pSrcNode->fHdrIncl;
  pNode->fStop = pSrcNode->fStop;
  pNode->ulProto = pSrcNode->ulProto;
  pNode->pSrcAddrLst = _newAddrLst( pSrcNode->pSrcAddrLst );
  pNode->pDstAddrLst = _newAddrLst( pSrcNode->pDstAddrLst );
  pNode->pSrcPortLst = _newPortLst( pSrcNode->pSrcPortLst );
  pNode->pDstPortLst = _newPortLst( pSrcNode->pDstPortLst );
  pNode->ullSent = pSrcNode->ullSent;
  pNode->ullRecv = pSrcNode->ullRecv;
  pNode->pUser = pSrcNode->pUser;

  return pNode;
}

VOID scFreeNode(PSCNODE pNode)
{
  if ( pNode == NULL )
    return;

  lnkseqFree( &pNode->lsNodes, PSCNODE, scFreeNode );

  if ( pNode->pszName != NULL )
    debugFree( pNode->pszName );

  if ( pNode->pszComment != NULL )
    debugFree( pNode->pszComment );

  if ( pNode->pSrcAddrLst != NULL )
    debugFree( pNode->pSrcAddrLst );

  if ( pNode->pDstAddrLst != NULL )
    debugFree( pNode->pDstAddrLst );

  if ( pNode->pSrcPortLst != NULL )
    debugFree( pNode->pSrcPortLst );

  if ( pNode->pDstPortLst != NULL )
    debugFree( pNode->pDstPortLst );

  debugFree( pNode ); 
}

VOID scCalc(PLINKSEQ plsNodes, PSCPKTINFO pInfo)
{
  PSCNODE	pNode;

  pNode = (PSCNODE)lnkseqGetFirst( plsNodes );
  while( pNode != NULL )
  {
    if (
         ( (pNode->ulProto & pInfo->ulProto) != 0 )
       &&
         ( ( pNode->pSrcAddrLst == NULL ) ||
           _testAddr( pNode->pSrcAddrLst, pInfo->ulSrcAddr ) )
       &&
         ( ( pNode->pDstAddrLst == NULL ) ||
           _testAddr( pNode->pDstAddrLst, pInfo->ulDstAddr ) )
       &&
         ( ( pNode->pSrcPortLst == NULL ) ||
           _testPort( pNode->pSrcPortLst, pInfo->ulSrcPort ) )
       &&
         ( ( pNode->pDstPortLst == NULL ) ||
           _testPort( pNode->pDstPortLst, pInfo->ulDstPort ) )
       )
    {
      PULLONG	pullCounter = pInfo->fReversChk ?
                                &pNode->ullRecv : &pNode->ullSent;

      *pullCounter += pNode->fHdrIncl ? pInfo->cbIPPacket : pInfo->cbPacket;

      DosRequestMutexSem( hmtxTrace, SEM_INDEFINITE_WAIT );
      if ( pTraceNode == pNode )
      {
        paTracePkt[ulNextTracePkt].ulIndex = (++ulTracePktIdx);
        paTracePkt[ulNextTracePkt].stPktInfo = *pInfo;
        ulNextTracePkt = (ulNextTracePkt + 1) % ulMaxTracePkt;
      }
      DosReleaseMutexSem( hmtxTrace );

      if ( !lnkseqIsEmpty( &pNode->lsNodes ) )
        scCalc( &pNode->lsNodes, pInfo );

      if ( pNode->fStop )
        break;
    }

    pNode = (PSCNODE)lnkseqGetNext( pNode );
  }
}

static VOID _prnSp(ULONG cSpaces)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cSpaces; ulIdx++ )
    putchar( ' ' );
}

VOID scPrintNode(PSCNODE pNode, ULONG ulLevel)
{
  PSCADDR	pAddr;
  PSCPORT	pPort;

  _prnSp( ulLevel ); printf( "Name: %s\n", pNode->pszName );
  _prnSp( ulLevel ); printf( "%s%s%s| HdrIncl: %u, fStop: %u\n",
          (pNode->ulProto & STPROTO_ICMP) != 0 ? "ICMP " : "",
          (pNode->ulProto & STPROTO_TCP) != 0 ? "TCP " : "",
          (pNode->ulProto & STPROTO_UDP) != 0 ? "UDP " : "",
          pNode->fHdrIncl, pNode->fStop );

  _prnSp( ulLevel ); printf( "Src addr:\n" );
  if ( pNode->pSrcAddrLst == NULL )
  {
    _prnSp( ulLevel ); puts( "  ANY" );
  }
  else
  for( pAddr = pNode->pSrcAddrLst; pAddr->ulLast != 0; pAddr++ )
  {
    _prnSp( ulLevel ); printf( "  %u.%u.%u.%u - %u.%u.%u.%u\n",
            (pAddr->ulFirst >> 24) & 0xFF, (pAddr->ulFirst >> 16) & 0xFF,
            (pAddr->ulFirst >> 8) & 0xFF, (pAddr->ulFirst) & 0xFF,
            (pAddr->ulLast >> 24) & 0xFF, (pAddr->ulLast >> 16) & 0xFF,
            (pAddr->ulLast >> 8) & 0xFF, (pAddr->ulLast) & 0xFF );
  }

  _prnSp( ulLevel ); printf( "Src ports:\n" );
  if ( pNode->pSrcPortLst == NULL )
  {
    _prnSp( ulLevel ); puts( "  ANY" );
  }
  else
  for( pPort = pNode->pSrcPortLst; pPort->ulLast != 0; pPort++ )
  {
    _prnSp( ulLevel );
    printf( "  %u - %u\n", pPort->ulFirst, pPort->ulLast );
  }

  _prnSp( ulLevel ); printf( "Dst addr:\n" );
  if ( pNode->pDstAddrLst == NULL )
  {
    _prnSp( ulLevel ); puts( "  ANY" );
  }
  else
  for( pAddr = pNode->pDstAddrLst; pAddr->ulLast != 0; pAddr++ )
  {
    _prnSp( ulLevel ); printf( "  %u.%u.%u.%u - %u.%u.%u.%u\n",
            (pAddr->ulFirst >> 24) & 0xFF, (pAddr->ulFirst >> 16) & 0xFF,
            (pAddr->ulFirst >> 8) & 0xFF, (pAddr->ulFirst) & 0xFF,
            (pAddr->ulLast >> 24) & 0xFF, (pAddr->ulLast >> 16) & 0xFF,
            (pAddr->ulLast >> 8) & 0xFF, (pAddr->ulLast) & 0xFF );
  }

  _prnSp( ulLevel ); printf( "Dst ports:\n" );
  if ( pNode->pDstPortLst == NULL )
  {
    _prnSp( ulLevel ); puts( "  ANY" );
  }
  else
  for( pPort = pNode->pDstPortLst; pPort->ulLast != 0; pPort++ )
  {
    _prnSp( ulLevel ); printf( "  %u - %u\n", pPort->ulFirst, pPort->ulLast );
  }

  _prnSp( ulLevel ); printf( "Sent: %llu", pNode->ullSent );
  printf( ", Recv: %llu\n\n", pNode->ullRecv );
}

VOID scPrint(PLINKSEQ plsNodes, ULONG ulLevel)
{
  PSCNODE	pNode;
  PSCADDR	pAddr;
  PSCPORT	pPort;

  pNode = (PSCNODE)lnkseqGetFirst( plsNodes );
  while( pNode != NULL )
  {
    _prnSp( ulLevel ); printf( "Name: %s\n", pNode->pszName );
    _prnSp( ulLevel ); printf( "%s%s%s| HdrIncl: %u, fStop: %u\n",
            (pNode->ulProto & STPROTO_ICMP) != 0 ? "ICMP " : "",
            (pNode->ulProto & STPROTO_TCP) != 0 ? "TCP " : "",
            (pNode->ulProto & STPROTO_UDP) != 0 ? "UDP " : "",
            pNode->fHdrIncl, pNode->fStop );

    _prnSp( ulLevel ); printf( "Src addr:\n" );
    if ( pNode->pSrcAddrLst == NULL )
    {
      _prnSp( ulLevel ); puts( "  ANY" );
    }
    else
    for( pAddr = pNode->pSrcAddrLst; pAddr->ulLast != 0; pAddr++ )
    {
      _prnSp( ulLevel ); printf( "  %u.%u.%u.%u - %u.%u.%u.%u\n",
              (pAddr->ulFirst >> 24) & 0xFF, (pAddr->ulFirst >> 16) & 0xFF,
              (pAddr->ulFirst >> 8) & 0xFF, (pAddr->ulFirst) & 0xFF,
              (pAddr->ulLast >> 24) & 0xFF, (pAddr->ulLast >> 16) & 0xFF,
              (pAddr->ulLast >> 8) & 0xFF, (pAddr->ulLast) & 0xFF );
    }

    _prnSp( ulLevel ); printf( "Src ports:\n" );
    if ( pNode->pSrcPortLst == NULL )
    {
      _prnSp( ulLevel ); puts( "  ANY" );
    }
    else
    for( pPort = pNode->pSrcPortLst; pPort->ulLast != 0; pPort++ )
    {
      _prnSp( ulLevel );
      printf( "  %u - %u\n", pPort->ulFirst, pPort->ulLast );
    }

    _prnSp( ulLevel ); printf( "Dst addr:\n" );
    if ( pNode->pDstAddrLst == NULL )
    {
      _prnSp( ulLevel ); puts( "  ANY" );
    }
    else
    for( pAddr = pNode->pDstAddrLst; pAddr->ulLast != 0; pAddr++ )
    {
      _prnSp( ulLevel ); printf( "  %u.%u.%u.%u - %u.%u.%u.%u\n",
              (pAddr->ulFirst >> 24) & 0xFF, (pAddr->ulFirst >> 16) & 0xFF,
              (pAddr->ulFirst >> 8) & 0xFF, (pAddr->ulFirst) & 0xFF,
              (pAddr->ulLast >> 24) & 0xFF, (pAddr->ulLast >> 16) & 0xFF,
              (pAddr->ulLast >> 8) & 0xFF, (pAddr->ulLast) & 0xFF );
    }

    _prnSp( ulLevel ); printf( "Dst ports:\n" );
    if ( pNode->pDstPortLst == NULL )
    {
      _prnSp( ulLevel ); puts( "  ANY" );
    }
    else
    for( pPort = pNode->pDstPortLst; pPort->ulLast != 0; pPort++ )
    {
      _prnSp( ulLevel ); printf( "  %u - %u\n", pPort->ulFirst, pPort->ulLast );
    }

    _prnSp( ulLevel ); printf( "Sent: %llu", pNode->ullSent );
    printf( ", Recv: %llu\n\n", pNode->ullRecv );

    pNode = (PSCNODE)lnkseqGetNext( pNode );
  }
}


//	Load list of nodes.
//	-------------------

static ULONG _loadString(PSLOADFN pfnLoad, ULONG ulUser, PSZ *ppszStr)
{
  ULONG		ulRC;
  ULONG		cbStr = 0;
  PSZ		pszStr;

  ulRC = pfnLoad( 2, &cbStr, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  if ( cbStr == 0 )
  {
    *ppszStr = NULL;
    return IPT_OK;
  }

  pszStr = debugMAlloc( cbStr + 1 );
  if ( pszStr == NULL )
    return IPT_NOT_ENOUGH_MEMORY;

  ulRC = pfnLoad( cbStr, pszStr, ulUser );
  if ( ulRC != IPT_OK )
  {
    debugFree( pszStr );
    return ulRC;
  }

  pszStr[cbStr] = '\0';
  *ppszStr = pszStr;

  return IPT_OK;
}

static ULONG _loadAddrLst(PSLOADFN pfnLoad, ULONG ulUser, PSCADDR *ppAddrLst)
{
  ULONG		ulRC;
  ULONG		cAddr = 0;
  PSCADDR	pAddrLst;

  ulRC = pfnLoad( 2, &cAddr, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  if ( cAddr == 0 )
  {
    *ppAddrLst = NULL;
    return IPT_OK;
  }

  pAddrLst = debugMAlloc( (cAddr + 1) * sizeof(SCADDR) );
  if ( pAddrLst == NULL )
    return IPT_NOT_ENOUGH_MEMORY;

  ulRC = pfnLoad( cAddr * sizeof(SCADDR), pAddrLst, ulUser );
  if ( ulRC != IPT_OK )
  {
    debug( "Cannot load %u addresses", cAddr );
    debugFree( pAddrLst );
    return ulRC;
  }

  // Set to zero last address record.
  memset( &pAddrLst[cAddr], 0, sizeof(SCADDR) );
  *ppAddrLst = pAddrLst;

  return IPT_OK;
}

static ULONG _loadPortLst(PSLOADFN pfnLoad, ULONG ulUser, PSCPORT *ppPortLst)
{
  ULONG		ulRC;
  ULONG		cPort = 0;
  PSCPORT	pPortLst;

  ulRC = pfnLoad( 2, &cPort, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  if ( cPort == 0 )
  {
    *ppPortLst = NULL;
    return IPT_OK;
  }

  pPortLst = debugMAlloc( (cPort + 1) * sizeof(SCPORT) );
  if ( pPortLst == NULL )
    return IPT_NOT_ENOUGH_MEMORY;

  ulRC = pfnLoad( cPort * sizeof(SCPORT), pPortLst, ulUser );
  if ( ulRC != IPT_OK )
  {
    debugFree( pPortLst );
    return ulRC;
  }

  memset( &pPortLst[cPort], 0, sizeof(SCPORT) );
  *ppPortLst = pPortLst;

  return IPT_OK;
}

static ULONG _loadNodeLst(PSLOADFN pfnLoad, ULONG ulUser, PLINKSEQ plsNodes);

static ULONG _loadNode(PSLOADFN pfnLoad, ULONG ulUser, PSCNODE *ppNode)
{
  ULONG		ulRC;
  ULONG		ulHdrSign;
  PSCNODE	pNode;

  ulRC = pfnLoad( sizeof(ULONG), &ulHdrSign, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  if ( ulHdrSign != NODE_HEADER )
  {
    debug( "Invalid node header" );
    return IPT_INVALID_DATA;
  }

  pNode = debugCAlloc( 1, sizeof(SCNODE) );

  if ( pNode == NULL )
    return IPT_NOT_ENOUGH_MEMORY;

  lnkseqInit( &pNode->lsNodes );

  do
  {
    ulRC = _loadString( pfnLoad, ulUser, &pNode->pszName );
    if ( ulRC != IPT_OK )
      break;

    ulRC = _loadString( pfnLoad, ulUser, &pNode->pszComment );
    if ( ulRC != IPT_OK )
      break;

    ulRC = pfnLoad( 1, &pNode->fHdrIncl, ulUser );
    if ( ulRC != IPT_OK )
      break;

    ulRC = pfnLoad( 1, &pNode->fStop, ulUser );
    if ( ulRC != IPT_OK )
      break;

    ulRC = pfnLoad( 1, &pNode->ulProto, ulUser );
    if ( ulRC != IPT_OK )
      break;

    ulRC = _loadAddrLst( pfnLoad, ulUser, &pNode->pSrcAddrLst );
    if ( ulRC != IPT_OK )
      break;

    ulRC = _loadAddrLst( pfnLoad, ulUser, &pNode->pDstAddrLst );
    if ( ulRC != IPT_OK )
      break;

    ulRC = _loadPortLst( pfnLoad, ulUser, &pNode->pSrcPortLst );
    if ( ulRC != IPT_OK )
      break;

    ulRC = _loadPortLst( pfnLoad, ulUser, &pNode->pDstPortLst );
    if ( ulRC != IPT_OK )
      break;

    ulRC = pfnLoad( sizeof(ULLONG), &pNode->ullSent, ulUser );
    if ( ulRC != IPT_OK )
      break;

    ulRC = pfnLoad( sizeof(ULLONG), &pNode->ullRecv, ulUser );
    if ( ulRC != IPT_OK )
      break;

    ulRC = _loadNodeLst( pfnLoad, ulUser, &pNode->lsNodes );
    if ( ulRC != IPT_OK )
      break;

    *ppNode = pNode;
    return IPT_OK;
  }
  while( FALSE );

  scFreeNode( pNode );
  return ulRC;
}

static ULONG _loadNodeLst(PSLOADFN pfnLoad, ULONG ulUser, PLINKSEQ plsNodes)
{
  ULONG		ulRC;
  ULONG		cNodes = 0;
  PSCNODE	pNode;

  ulRC = pfnLoad( 2, &cNodes, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  for( ; cNodes > 0; cNodes-- )
  {
    ulRC = _loadNode( pfnLoad, ulUser, &pNode );
    if ( ulRC != IPT_OK )
    {
      lnkseqFree( plsNodes, PSCNODE, scFreeNode );
      return ulRC;
    }

    lnkseqAdd( plsNodes, pNode );
  }

  return IPT_OK;
}

ULONG scLoad(PSLOADFN pfnLoad, ULONG ulUser, PLINKSEQ plsNodes)
{
  ULONG		ulRC;

  ulRC = _loadNodeLst( pfnLoad, ulUser, plsNodes );
  return ulRC;
}


//	Store list of nodes.
//	--------------------

static ULONG _storeString(PSSTOREFN pfnStore, ULONG ulUser, PSZ pszStr)
{
  ULONG		ulRC;
  ULONG		cbStr = pszStr == NULL ? 0 : strlen( pszStr );

  ulRC = pfnStore( 2, &cbStr, ulUser );

  if ( ( ulRC == IPT_OK ) && ( cbStr != 0 ) )
    ulRC = pfnStore( cbStr, pszStr, ulUser );

  return ulRC;
}

static ULONG _storeAddrLst(PSSTOREFN pfnStore, ULONG ulUser, PSCADDR pAddrLst)
{
  ULONG		ulRC;
  ULONG		cAddr = 0;
  PSCADDR	pAddr;

  if ( pAddrLst != NULL )
    for( pAddr = pAddrLst; pAddr->ulLast != 0; pAddr++ )
      cAddr++;

  ulRC = pfnStore( 2, &cAddr, ulUser );
  if ( ( ulRC != IPT_OK ) || ( cAddr == 0 ) )
    return ulRC;

  ulRC = pfnStore( cAddr * sizeof(SCADDR), pAddrLst, ulUser );

  return ulRC;
}

static ULONG _storePortLst(PSSTOREFN pfnStore, ULONG ulUser, PSCPORT pPortLst)
{
  ULONG		ulRC;
  ULONG		cPort = 0;
  PSCPORT	pPort;

  if ( pPortLst != NULL )
    for( pPort = pPortLst; pPort->ulLast != 0; pPort++ )
      cPort++;

  ulRC = pfnStore( 2, &cPort, ulUser );
  if ( ( ulRC != IPT_OK ) || ( cPort == 0 ) )
    return ulRC;

  ulRC = pfnStore( cPort * sizeof(SCPORT), pPortLst, ulUser );

  return ulRC;
}

static ULONG _storeNodeLst(PSSTOREFN pfnStore, ULONG ulUser, PLINKSEQ plsNodes);

static ULONG _storeNode(PSSTOREFN pfnStore, ULONG ulUser, PSCNODE pNode)
{
  ULONG		ulRC;
  ULONG		ulHdrSign = NODE_HEADER;

  if ( pNode == NULL )
    return IPT_OK;

  ulRC = pfnStore( sizeof(ULONG), &ulHdrSign, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = _storeString( pfnStore, ulUser, pNode->pszName );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = _storeString( pfnStore, ulUser, pNode->pszComment );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = pfnStore( 1, &pNode->fHdrIncl, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = pfnStore( 1, &pNode->fStop, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = pfnStore( 1, &pNode->ulProto, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = _storeAddrLst( pfnStore, ulUser, pNode->pSrcAddrLst );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = _storeAddrLst( pfnStore, ulUser, pNode->pDstAddrLst );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = _storePortLst( pfnStore, ulUser, pNode->pSrcPortLst );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = _storePortLst( pfnStore, ulUser, pNode->pDstPortLst );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = pfnStore( sizeof(ULLONG), &pNode->ullSent, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = pfnStore( sizeof(ULLONG), &pNode->ullRecv, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  ulRC = _storeNodeLst( pfnStore, ulUser, &pNode->lsNodes );

  return IPT_OK;
}

static ULONG _storeNodeLst(PSSTOREFN pfnStore, ULONG ulUser, PLINKSEQ plsNodes)
{
  ULONG		ulRC;
  PSCNODE	pNode = (PSCNODE)lnkseqGetFirst( plsNodes );

  ulRC = pfnStore( 2, &plsNodes->ulCount, ulUser );

  for( ; ( ulRC == IPT_OK ) && ( pNode != NULL );
       pNode = (PSCNODE)lnkseqGetNext( pNode ) )
  {
    ulRC = _storeNode( pfnStore, ulUser, pNode );
  }

  return ulRC;
}

ULONG scStore(PSSTOREFN pfnStore, ULONG ulUser, PLINKSEQ plsNodes)
{
  ULONG		ulRC;

  ulRC = _storeNodeLst( pfnStore, ulUser, plsNodes );
  return ulRC;
}

VOID scSnapshot(PLINKSEQ plsNodes, PSCSNAPSHOTPARAM pParam)
{
  PSCNODE	pNode = (PSCNODE)lnkseqGetFirst( plsNodes );
  
  for( ; pNode != NULL; pNode = (PSCNODE)lnkseqGetNext( pNode ) )
  {
    if ( pParam->cbSnapshot >= sizeof(IPTSNAPSHOT) )
    {
      pParam->pSnapshot->pTIF = pParam->pTIF;
      pParam->pSnapshot->pNode = pNode;
      pParam->pSnapshot->ullSent = pNode->ullSent;
      pParam->pSnapshot->ullRecv = pNode->ullRecv;
      pParam->pSnapshot++;
      pParam->cbSnapshot -= sizeof(IPTSNAPSHOT);
    }

    pParam->cbActual += sizeof(IPTSNAPSHOT);
    scSnapshot( &pNode->lsNodes, pParam );
  }
}

VOID scResetCounters(PLINKSEQ plsNodes)
{
  PSCNODE	pNode = (PSCNODE)lnkseqGetFirst( plsNodes );
  
  for( ; pNode != NULL; pNode = (PSCNODE)lnkseqGetNext( pNode ) )
  {
    pNode->ullSent = 0;
    pNode->ullRecv = 0;
    scResetCounters( &pNode->lsNodes );
  }
}

// Parse filter nodes

ULONG scIPAddrToStr(ULONG ulIPAddr, ULONG cbBuf, PCHAR pcBuf)
{
  LONG	lRC = _snprintf( pcBuf, cbBuf, "%u.%u.%u.%u",
                         ((PBYTE)&ulIPAddr)[3], ((PBYTE)&ulIPAddr)[2],
                         ((PBYTE)&ulIPAddr)[1], ((PBYTE)&ulIPAddr)[0] );

  return lRC < 0 ? 0 : lRC;
}

ULONG scAddrToStr(PSCADDR pAddr, ULONG cbBuf, PCHAR pcBuf)
{
  ULONG		cbResult = scIPAddrToStr( pAddr->ulFirst, cbBuf, pcBuf );
  LONG		cBytes;

  if ( cbResult == 0 )
    return 0;

  if ( pAddr->ulFirst != pAddr->ulLast )
  {
    cbBuf -= cbResult;
    pcBuf += cbResult;

    if ( pAddr->ulPresForm == SC_ADDR_RANGE )
    {
      if ( cbBuf <= 4 )
        return 0;

      *((PULONG)pcBuf) = (ULONG)'\0 - ';
      cbBuf -= 3;
      pcBuf += 3;
      cBytes = scIPAddrToStr( pAddr->ulLast, cbBuf, pcBuf );
    }
    else
    {
      ULONG		ulBits;
      ULONG		ulMask = ~0;

      if ( cbBuf <= 2 )
        return 0;

      *((PUSHORT)pcBuf) = (USHORT)'\0/';
      cbBuf -= 1;
      pcBuf += 1;

      // Detect mask bits.
      for( ulBits = 0; ulBits < 32; ulBits++ )
      {
        if ( ( ( (pAddr->ulFirst >> ulBits) & 0x01 ) != 0 ) ||
             ( ( (pAddr->ulLast >> ulBits) & 0x01 ) != 1 ) )
          break;

        ulMask <<= 1;
      }

      if ( pAddr->ulPresForm == SC_ADDR_MASK_BITS )
        cBytes = _snprintf( pcBuf, cbBuf, "%u", 32 - ulBits );
      else	// SC_ADDR_MASK_VAL
        cBytes = scIPAddrToStr( ulMask, cbBuf, pcBuf );
    }

    if ( cBytes <= 0 )
      return 0;
    cbResult += cBytes;
  }

  return cbResult;
}

ULONG scPortLstToStr(PSCPORT pPort, ULONG cbBuf, PCHAR pcBuf)
{
  CHAR		szItem[16];
  ULONG		cbItem;
  ULONG		cbTotal = 0;

  if ( pPort != NULL )
  {
    for( ; pPort->ulLast != 0; pPort++ )
    {
      if ( pPort->ulFirst == pPort->ulLast )
        cbItem = sprintf( &szItem, "%u", pPort->ulFirst );
      else
        cbItem = sprintf( &szItem, "%u-%u", pPort->ulFirst, pPort->ulLast );

      if ( cbTotal != 0 )
      {
        if ( cbBuf != 0 )
        {
          *(pcBuf++) = ',';
          cbBuf--;
        }
        cbTotal++;
      }
      cbTotal += cbItem;

      if ( cbBuf != 0 )
      {
        if ( cbItem > cbBuf )
          cbItem = cbBuf;
        memcpy( pcBuf, &szItem, cbItem );
        pcBuf += cbItem;
        cbBuf -= cbItem;
      }
    }
  }

  if ( cbBuf != 0 )
    *pcBuf = '\0';

  return cbTotal;
}


// Build filter nodes

BOOL scStrToIPAddr(PULONG pulAddr, PSZ pszStr, PCHAR *ppcEnd)
{
  CHAR		szAddr[16];
  PCHAR		pcAddr;

  _STR_skipSpaces( pszStr );

  if ( memcmp( pszStr, "255.255.255.255", 15 ) == 0 )
  {
    *ppcEnd = &pszStr[15];
    *pulAddr = 0xFFFFFFFF;
    return TRUE;
  }

  for( pcAddr = &szAddr;
       ( pcAddr < &szAddr[15] ) && ( *pszStr == '.' || isdigit( *pszStr ) );
       pcAddr++, pszStr++ )
    *pcAddr = *pszStr;

  *pcAddr = '\0';
  *pulAddr = revUL( inet_addr( &szAddr ) );

  *ppcEnd = pszStr;

  return ( *pulAddr != (ULONG)(-1) );
}

BOOL scStrToAddr(PSZ pszStr, PSCADDR pAddr)
{
  PCHAR		pcEnd;

  if ( !scStrToIPAddr( &pAddr->ulFirst, pszStr, &pcEnd ) )
    return FALSE;

  _STR_skipSpaces( pcEnd );

  if ( *pcEnd == '\0' )
  {
    // A.D.D.R
    pAddr->ulLast = pAddr->ulFirst;
  }
  else
  {
    pszStr = pcEnd + 1;

    if ( *pcEnd == '-' )
    {
      // A.D.D.R1 - A.D.D.R2
      if ( !scStrToIPAddr( &pAddr->ulLast, pszStr, &pcEnd ) ||
           ( pAddr->ulLast < pAddr->ulFirst ) )
        return FALSE;

      pAddr->ulPresForm = SC_ADDR_RANGE;
    }
    else if ( *pcEnd == '/' )
    {
      ULONG	ulMask;
      ULONG	ulMaskBits;

      _STR_skipSpaces( pszStr );
      ulMaskBits = strtoul( pszStr, &pcEnd, 10 );

      if ( pszStr == pcEnd )
        return FALSE;

      if ( *pcEnd != '.' )
      {
        // A.D.D.R / Bits
        if ( ulMaskBits > 32 )
          return FALSE;

        ulMask = ulMaskBits == 0 ? 0 : ( 0xFFFFFFFF << ( 32 - ulMaskBits ) );
        pAddr->ulPresForm = SC_ADDR_MASK_BITS;
      }
      else
      {
        // A.D.D.R / M.A.S.K
        if ( !scStrToIPAddr( &ulMask, pszStr, &pcEnd ) )
          return FALSE;
        pAddr->ulPresForm = SC_ADDR_MASK_VAL;
      }

      pAddr->ulFirst &= ulMask;
      pAddr->ulLast = pAddr->ulFirst | ~ulMask;
    }
    else
      return FALSE;
  }

  return TRUE;
}

BOOL scStrToPortLst(PSZ pszStr, ULONG ulMaxPorts, PSCPORT pPortLst)
{
  PCHAR		pcEnd = pszStr;
  ULONG		ulMin, ulMax;
  ULONG		ulLenRange, ulLenIdx;
  BOOL		fUnion;
  ULONG		ulIdx;
  ULONG		cPorts = 0;
  SCPORT	stPort;

  do
  {
    if ( cPorts == ulMaxPorts )
      return FALSE;

    // Parse element (range).

    _STR_skipSpaces( pszStr );
    if ( *pszStr == '\0' )
      break;

    stPort.ulFirst = strtoul( pszStr, &pcEnd, 10 );
    if ( ( pcEnd == pszStr ) || ( stPort.ulFirst > 0xFFFF ) )
      return FALSE;
    _STR_skipSpaces( pcEnd );
    pszStr = pcEnd + 1;

    if ( *pcEnd == '-' )
    {
      _STR_skipSpaces( pszStr );
      stPort.ulLast = strtoul( pszStr, &pcEnd, 10 );
      if ( ( pcEnd == pszStr ) || ( stPort.ulLast > 0xFFFF ) ||
           ( stPort.ulLast < stPort.ulFirst ) )
        return FALSE;
      _STR_skipSpaces( pcEnd );
      pszStr = pcEnd + 1;
    }
    else
      stPort.ulLast = stPort.ulFirst;

    // Union - new range with listed ranges.

    do
    {
      ulLenRange = stPort.ulLast - stPort.ulFirst + 1;
      fUnion = FALSE;
      for( ulIdx = 0; ulIdx < cPorts; ulIdx++ )
      {
        ulMin = min( stPort.ulFirst, pPortLst[ulIdx].ulFirst );
        ulMax = max( stPort.ulLast, pPortLst[ulIdx].ulLast );
        ulLenIdx = pPortLst[ulIdx].ulLast - pPortLst[ulIdx].ulFirst + 1;
        if ( ( ulLenIdx + ulLenRange ) >= ( ulMax - ulMin + 1 ) )
        {
          stPort.ulFirst = ulMin;
          stPort.ulLast = ulMax;
          pPortLst[ulIdx] = pPortLst[--cPorts];
          fUnion = TRUE;
          break;
        }
      }
    }
    while( fUnion );

    pPortLst[cPorts++] = stPort;
  }
  while( *pcEnd == ',' );

  if ( ( *pcEnd != '\0' ) || ( cPorts == ulMaxPorts ) )
    return FALSE;

  pPortLst[cPorts].ulFirst = 0;
  pPortLst[cPorts].ulLast = 0;
  return TRUE;
}


//	Node tracing.
// 	-------------

BOOL scTraceStart(PSCNODE pNode, ULONG ulMaxRecords)
{
  ULONG		ulRC;

  if ( ulMaxRecords == 0 )
    return FALSE;

  ulRC = DosRequestMutexSem( hmtxTrace, SEM_INDEFINITE_WAIT );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosRequestMutexSem(), rc = %u", ulRC );
    return FALSE;
  }

  if ( paTracePkt != NULL )
    debugFree( paTracePkt );

  paTracePkt = debugCAlloc( ulMaxRecords, sizeof(SCTRACEPKT) );
  if ( paTracePkt == NULL )
  {
    pTraceNode = NULL;
    DosReleaseMutexSem( hmtxTrace );
    debug( "Not enough memory." );
    return FALSE;
  }

  ulMaxTracePkt = ulMaxRecords;
  pTraceNode = pNode;
  ulNextTracePkt = 0;
  ulTracePktIdx = 0;

  DosReleaseMutexSem( hmtxTrace );

  return TRUE;
}

VOID scTraceStop()
{
  DosRequestMutexSem( hmtxTrace, SEM_INDEFINITE_WAIT );
  pTraceNode = NULL;
  ulMaxTracePkt = 0;

  if ( paTracePkt != NULL )
  {
    debugFree( paTracePkt );
    paTracePkt = NULL;
  }
  DosReleaseMutexSem( hmtxTrace );
}

ULONG scTraceQuery(PSCNODE *ppNode, ULONG cPackets, PSCTRACEPKT paPackets)
{
  LONG		lPktIdx;
  ULONG		ulRC;

  if ( ppNode != NULL )
    *ppNode = pTraceNode;
  if ( ( pTraceNode == NULL ) || ( cPackets == 0 ) )
    return 0;

  ulRC = DosRequestMutexSem( hmtxTrace, SEM_INDEFINITE_WAIT );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosRequestMutexSem(), rc = %u", ulRC );
    return 0;
  }

  lPktIdx = ulNextTracePkt - min( cPackets, ulMaxTracePkt );
  if ( lPktIdx < 0 )
    lPktIdx = ulMaxTracePkt + lPktIdx;

  cPackets = 0;
  do
  {
    if ( paTracePkt[lPktIdx].stPktInfo.cbIPPacket != 0 )
    {
      *(paPackets++) = paTracePkt[lPktIdx];
      cPackets++;
    }
    lPktIdx = (lPktIdx + 1) % ulMaxTracePkt;
  }
  while( lPktIdx != ulNextTracePkt );

  DosReleaseMutexSem( hmtxTrace );

  return cPackets;
}
