/*
    Network packets tracing.
*/

#include <stdlib.h>
#include <string.h>
#define INCL_DOSMISC
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#include <os2.h>
#include "linkseq.h"
#define IPTRACE_C
#include "iptrace.h"
#include <debug.h>     // Must be the last.

#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))

#define IPT_FRAGPKT_TIMEOUT	60000

// PROTO_xxxx - ETHIP.chProto
#define PROTO_ICMP		1
#define PROTO_TCP		6
#define PROTO_UDP		17

#define DATA_HEADER		'LAH_'

typedef struct _FRAGRANGE {
  ULONG		ulFirst;
  ULONG		ulLast;
} FRAGRANGE, *PFRAGRANGE;

typedef struct _FRAGPKT {
  SEQOBJ	lsObj;
  ULONG		ulProto;	// PROTO_xxxx
  ULLONG	ullSrcDstIP;
  ULONG		ulIdent;	// Identifying group of fragments.
  ULONG		cbPacket;
  ULONG		cbIPPacket;
  ULONG		ulFullLength;
  BYTE		abData[128];	// First bytes of protocol (bProto) packet.
  ULONG		ulTimestamp;
  ULONG		cRanges;
  FRAGRANGE	astRanges[128];
} FRAGPKT, *PFRAGPKT;

// usEtherType: http://en.wikipedia.org/wiki/EtherType
// IP packet:   http://en.wikipedia.org/wiki/IPv4
// bProto:     http://en.wikipedia.org/wiki/List_of_IP_protocol_numbers

#pragma pack(1)
typedef struct _ETHIP {
  // Ethernet
  CHAR		acMACDst[6];
  CHAR		acMACSrc[6];
  USHORT	usEtherType;

  // IP packet ( EtherType = 0x0800 [net ord.] ).
  BYTE		bVerIHL;	// 0-3: Version, 4-7: Internet Header Length
  BYTE		bDSCPnECN;	// 0-5: Differentiated Services Code Point
				// 6-7: Explicit Congestion Notification (ECN)
  USHORT	usTotalLen;
  USHORT	usIdent;	// Identifying group of fragments.
  USHORT	usFlOffset;	// 0-2: Flags, 3-15: Fragment Offset.
  BYTE		bTTL;
  BYTE		bProto;		// PROTO_xxxx
  USHORT	usHdrCRC;
  ULONG		ulSrcAddr;
  ULONG		ulDstAddr;

} ETHIP, *PETHIP;
#pragma pack()


LINKSEQ		lsIPTIF;
HMTX		hmtxIPTIF = NULLHANDLE;

VOID inline swpUL(PULONG pulVal1, PULONG pulVal2)
{
  __asm {
    push    esi
    push    edi
    push    eax
    mov     esi, pulVal1
    mov     edi, pulVal2
    mov     eax, [esi]
    xchg    eax, [edi]
    mov     [esi], eax
    pop     eax
    pop     edi
    pop     esi
  }
}

ULONG /*inline*/ fragOffs(USHORT usFlOffset);
#pragma aux fragOffs = \
  "xchg    ah, al          " \
  "and     eax, 00001FFFh  " \
  "shl     eax, 3          " \
  parm [ ax ] value [ eax ];


// _tifOnline(PIPTIF pTIF, BOOL fOnline)
//
// Sets a flag to trace interface.

static BOOL _tifOnline(PIPTIF pTIF, BOOL fOnline)
{
  if ( ( pTIF->iSock == -1 ) || ( pTIF->fOnline == fOnline ) )
    return TRUE;

  if ( fOnline )
  {
    if ( ioctl( pTIF->iSock, SIOCGIFEFLAGS, (caddr_t)&pTIF->stIFReq ) == -1 )
    {
      debugCP( "ioctl(,SIOCGIFEFLAGS,) failed" );
      return FALSE;
    }

    pTIF->ulEFlagsSave = (ULONG)pTIF->stIFReq.ifr_eflags;
    pTIF->stIFReq.ifr_eflags = (caddr_t)( (ULONG)pTIF->ulEFlagsSave | 1 );
  }
  else
    pTIF->stIFReq.ifr_eflags = (caddr_t)pTIF->ulEFlagsSave;

  if ( ioctl( pTIF->iSock, SIOCSIFEFLAGS, (caddr_t)&pTIF->stIFReq ) == -1 )
  {
    debugCP( "ioctl(,SIOCSIFEFLAGS,) failed" );
    return FALSE;
  }

//  pTIF->stTraceHdr.pt_len = sizeof(pTIF->acBuf);
  pTIF->stTraceHdr.pt_data = (caddr_t)&pTIF->acBuf;
  pTIF->stIFReq.ifr_data = (caddr_t)&pTIF->stTraceHdr;

  pTIF->fOnline = fOnline;
  return TRUE;
}

static VOID _tifTrace(PIPTIF pTIF)
{
  ULONG      ulIPHdrLen;
  PETHIP     pEthIP = (PETHIP)&pTIF->acBuf;
  PFRAGPKT   pFragPkt;
  PVOID      pPacket;
  SCPKTINFO  stPktInfo;
  ULONG      ulProto;

  while( pTIF->iSock != -1 )
  {
    pTIF->stTraceHdr.pt_len = sizeof(pTIF->acBuf);
    if ( ioctl( pTIF->iSock, SIOCGIFTRACE, (caddr_t)&pTIF->stIFReq ) == -1 )
    {
      debug( "%s: ioctl(%u,SIOCGIFTRACE,,) failed, errno = %d\n",
             pTIF->stIFReq.ifr_name, pTIF->iSock, sock_errno() );
      psock_errno( "ioctl" );
      break;
    }

    if ( pTIF->stTraceHdr.pt_htype != HT_ETHER )
      continue;

  // cbBuf = IPTIF_PKT_LENGTH( &stTIF )

    // usEtherType <= 1500  - usEtherType is PayLoad size, LLC packet in PayLoad.
    // usEtherType > 1535   - EtherType. 0x0800 - IPv4
    // usEtherType in 1501..1535 - Invalid.
    if ( pEthIP->usEtherType != 0x0008 ) // Not IPv4 (0x0800).
      continue;

    if ( ( pEthIP->bVerIHL & 0xF0 ) != 0x40 ) // Invalid IP hdr ver.
      continue;

    ulProto = pEthIP->bProto;
    // We work only with ICMP, TCP and UDP.
    if ( ( ulProto != PROTO_TCP ) && ( ulProto != PROTO_UDP ) &&
         ( ulProto != PROTO_ICMP ) )
      continue;

    ulIPHdrLen = ( pEthIP->bVerIHL & 0x0F ) << 2;
    if ( ( ulIPHdrLen < 20 ) || ( ulIPHdrLen > 60 ) )  // Invalid IHL field.
      continue;

    stPktInfo.cbIPPacket = revUS( pEthIP->usTotalLen );

    if ( stPktInfo.cbIPPacket < 20 ) // Invalid Total Length field.
      continue;

    stPktInfo.cbPacket = stPktInfo.cbIPPacket - ulIPHdrLen;
    pPacket = &pEthIP->bVerIHL + ulIPHdrLen;
    pFragPkt = NULL;

    if ( ( pEthIP->usFlOffset & 0xFF3F ) != 0 )
    do
    {
      // Fragment offset is not zero or MF flag set - packet is part of datagram.

      ULONG	ulFragOffs = fragOffs( pEthIP->usFlOffset );
      ULLONG	ullSrcDstIP = *((PULLONG)&pEthIP->ulSrcAddr);
      ULONG	ulIdent = pEthIP->usIdent;
      PFRAGPKT	pNextFragPkt;
      ULONG	ulTimestamp;

      DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulTimestamp,
                       sizeof(ULONG) );

      // Search fragmented packet record.
      pFragPkt = (PFRAGPKT)lnkseqGetFirst( &pTIF->lsFrags );
      while( ( pFragPkt != NULL ) &&
             ( ( pFragPkt->ulIdent != ulIdent ) ||
               ( pFragPkt->ullSrcDstIP != ullSrcDstIP ) ||
               ( pFragPkt->ulProto != ulProto ) ) )
      {
        pNextFragPkt = (PFRAGPKT)lnkseqGetNext( pFragPkt );

        if ( (ulTimestamp - pFragPkt->ulTimestamp) > IPT_FRAGPKT_TIMEOUT )
        {
          // debug( "Fragment packet timedout" );
          lnkseqRemove( &pTIF->lsFrags, pFragPkt );
          free( pFragPkt );
        }

        pFragPkt = pNextFragPkt;
      }

      if ( pFragPkt == NULL )
      {
        // Not found - create a new record.

        pFragPkt = malloc( sizeof(FRAGPKT) );
        if ( pFragPkt == NULL )
        {
          debugCP( "Not enough memory" );
          return;
        }

        pFragPkt->ulProto = ulProto;
        pFragPkt->ulIdent = ulIdent;
        pFragPkt->ullSrcDstIP = ullSrcDstIP;
        pFragPkt->cRanges = 0;
        pFragPkt->cbIPPacket = stPktInfo.cbIPPacket;
        pFragPkt->cbPacket = stPktInfo.cbPacket;
        pFragPkt->ulFullLength = 0;
        pFragPkt->ulTimestamp = ulTimestamp;
        lnkseqAdd( &pTIF->lsFrags, pFragPkt );
      }
      else
      {
        pFragPkt->cbIPPacket += stPktInfo.cbIPPacket;
        pFragPkt->cbPacket += stPktInfo.cbPacket;
      }

      if ( ( pEthIP->usFlOffset & 0x20 ) == 0 )
      {
        // Last packet of datagram. Now we know the full length of the datagram.
        pFragPkt->ulFullLength = ulFragOffs + stPktInfo.cbPacket;
      }

      if ( stPktInfo.cbPacket != 0 )
      {
        ULONG          ulIdx, ulMin, ulMax, ulLenRange, ulLenIdx;
        BOOL           fUnion;
        FRAGRANGE      stRange;

        // Store a chunk of datagram

        if ( ulFragOffs < sizeof(pFragPkt->abData) )
        {
          memcpy( &pFragPkt->abData[ulFragOffs], pPacket,
                  min( stPktInfo.cbPacket, sizeof(pFragPkt->abData) -
                                           ulFragOffs ) );
        }

        // Union - bytes range of new packet with listed ranges

        stRange.ulFirst = ulFragOffs;
        stRange.ulLast = ulFragOffs + stPktInfo.cbPacket - 1;
        do
        {
          ulLenRange = stRange.ulLast - stRange.ulFirst + 1;
          fUnion = FALSE;
          for( ulIdx = 0; ulIdx < pFragPkt->cRanges; ulIdx++ )
          {
            ulMin = min( stRange.ulFirst, pFragPkt->astRanges[ulIdx].ulFirst );
            ulMax = max( stRange.ulLast, pFragPkt->astRanges[ulIdx].ulLast );
            ulLenIdx = pFragPkt->astRanges[ulIdx].ulLast -
                       pFragPkt->astRanges[ulIdx].ulFirst + 1;
            if ( ( ulLenIdx + ulLenRange ) >= ( ulMax - ulMin + 1 ) )
            {
              stRange.ulFirst = ulMin;
              stRange.ulLast = ulMax;
              pFragPkt->astRanges[ulIdx] =
                                     pFragPkt->astRanges[--pFragPkt->cRanges];
              fUnion = TRUE;
              break;
            }
          }
        }
        while( fUnion );

        if ( ( pFragPkt->cRanges == 0 ) && ( stRange.ulFirst == 0 ) &&
             ( stRange.ulLast == (pFragPkt->ulFullLength - 1) ) )
        {
          // Full packet received
          stPktInfo.cbIPPacket = pFragPkt->cbIPPacket;
          stPktInfo.cbPacket = pFragPkt->cbPacket;
          pPacket = &pFragPkt->abData;
          break;
        }
        else
        {
          if ( pFragPkt->cRanges == ARRAYSIZE( pFragPkt->astRanges ) )
          {
            debugCP( "Too many byte ranges of fragments" );
            lnkseqRemove( &pTIF->lsFrags, pFragPkt );
            free( pFragPkt );
          }
          else
            pFragPkt->astRanges[pFragPkt->cRanges++] = stRange;
        }
      }  // if ( stPktInfo.cbPacket != 0 )

      pPacket = NULL;  // Packet not ready yet.
    }
    while( FALSE );  // if ( ( pEthIP->usFlOffset & 0xFF3F ) != 0 ) do

    if ( pPacket == NULL )
      continue;

    // Get protocol info (ports)

    if ( ulProto == PROTO_ICMP )
    {
      stPktInfo.ulSrcPort = 0;
      stPktInfo.ulDstPort = 0;
      stPktInfo.ulProto = STPROTO_ICMP;
    }
    else
    {
      stPktInfo.ulSrcPort = revUS( ((PUSHORT)pPacket)[0] );
      stPktInfo.ulDstPort = revUS( ((PUSHORT)pPacket)[1] );
      stPktInfo.ulProto = ulProto == PROTO_TCP ? STPROTO_TCP : STPROTO_UDP;
    }

    // Remove record of fragmented packet.
    if ( pFragPkt != NULL )
    {
      lnkseqRemove( &pTIF->lsFrags, pFragPkt );
      free( pFragPkt );
    }

    stPktInfo.ulSrcAddr = revUL( pEthIP->ulSrcAddr );
    stPktInfo.ulDstAddr = revUL( pEthIP->ulDstAddr );
    stPktInfo.fReversChk = FALSE;

    // Check packet with nodes.

    DosRequestMutexSem( pTIF->hmtxNodes, SEM_INDEFINITE_WAIT );

    // Increment "Send" values.
    scCalc( &pTIF->lsNodes, &stPktInfo );

    // Increment "Received" values.
    swpUL( &stPktInfo.ulSrcAddr, &stPktInfo.ulDstAddr );
    swpUL( &stPktInfo.ulSrcPort, &stPktInfo.ulDstPort );
    stPktInfo.fReversChk = TRUE;
    scCalc( &pTIF->lsNodes, &stPktInfo );

    DosReleaseMutexSem( pTIF->hmtxNodes );
  }  // while( pTIF->iSock != -1 )
}

// _tifDelay(PIPTIF pTIF, ULONG ulDelay)
//
// Interruptible pause which can be canceled using the function so_cancel().

static VOID _tifDelay(PIPTIF pTIF, ULONG ulDelay)
{
  if ( pTIF->iSock != -1 )
  {
    int		iRC = os2_select( &pTIF->iSock, 0, 0, 1, ulDelay );
    // os2_select() returned zero               - timed out, OK.
    // os2_select() = -1 & sock_errno() = 10006 - interrupted from other thread
    //                                            by so_cancel(), OK.
    // Otherwise - The function failed for some reason, we use DosSleep().

    if ( ( iRC > 0 ) || ( ( pTIF->iSock != -1 ) && ( iRC == -1 ) &&
                          ( sock_errno() != 10006 ) ) )
    {
      debug( "os2_select(%d,,,) failed, use DosSleep().", pTIF->iSock );
      DosSleep( ulDelay );
    }
  }
}

VOID APIENTRY threadRead(ULONG ulData)
{
  PIPTIF	pTIF = (PIPTIF)ulData;

  while( pTIF->iSock != -1 )
  {
    if ( _tifOnline( pTIF, TRUE ) )
      _tifTrace( pTIF );

    // The interface may be non-existent currently. Try to re-use after a pause.
    _tifDelay( pTIF, 1000 );
  }
}


/* Create a new trace interface object. */
static PIPTIF _iptNew(PSZ pszIFName)
{
  PIPTIF	pTIF = calloc( 1, sizeof(IPTIF) );
  ULONG		ulRC;

  if ( pTIF == NULL )
  {
    debugCP( "Not enough memory" );
    return NULL;
  }

  lnkseqInit( &pTIF->lsFrags );
  lnkseqInit( &pTIF->lsNodes );
  pTIF->iSock = -1;

  ulRC = DosCreateMutexSem( NULL, &pTIF->hmtxNodes, 0, FALSE );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosCreateMutexSem(), rc = %u", ulRC );
    free( pTIF );
    return NULL;
  }

  strlcpy( pTIF->stIFReq.ifr_name, pszIFName, sizeof(pTIF->stIFReq.ifr_name) );

  return pTIF;
}

static PIPTIF _iptGetTIF(PSZ pszName)
{
  PIPTIF	pTIF;

  for( pTIF = (PIPTIF)lnkseqGetFirst( &lsIPTIF );
       pTIF != NULL; pTIF = (PIPTIF)lnkseqGetNext( pTIF ) )
    if ( stricmp( &pTIF->stIFReq.ifr_name, pszName ) == 0 )
      break;

  return pTIF;
}

static BOOL _iptStart(PIPTIF pTIF)
{
  ULONG		ulRC;

  if ( pTIF != NULL )
  do
  {
    pTIF->iSock = socket( PF_INET, SOCK_RAW, 0 );
    if ( pTIF->iSock == -1 )
    {
      debugCP( "Cannot create a socket" );
      break;
    }

    ulRC = DosCreateThread( &pTIF->tid, threadRead, (ULONG)pTIF, 0, 65536 );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosCreateThread(), rc = %u", ulRC );
      break;
    }

    DosRequestMutexSem( hmtxIPTIF, SEM_INDEFINITE_WAIT );
    if ( _iptGetTIF( pTIF->stIFReq.ifr_name ) == NULL )
    {
      lnkseqAdd( &lsIPTIF, pTIF );
      DosReleaseMutexSem( hmtxIPTIF );
      return TRUE;
    }
    DosReleaseMutexSem( hmtxIPTIF );
  }
  while( FALSE );

  return FALSE;
}

static VOID _iptFree(PIPTIF pTIF)
{
  ULONG		ulRC;
  ULONG		ulIdx;

  if ( pTIF == NULL )
    return;

  if ( pTIF->iSock != -1 )
  {
    int      iSock = pTIF->iSock;

    // Cancel pause on socket ( see _tifDelay() ).
    so_cancel( iSock );
    // Return extended flags on interface.
    _tifOnline( pTIF, FALSE );
    // Set "end of work" mark for thread.
    pTIF->iSock = -1;
    // Close socket.
    soclose( iSock );
  }

  if ( pTIF->tid != 0 )
  {
    debug( "Wait thread %u...", pTIF->tid );
    for( ulIdx = 0; ulIdx < 20; ulIdx++ )
    {
      ulRC = DosWaitThread( &pTIF->tid, DCWW_NOWAIT );
      if ( ulRC != ERROR_THREAD_NOT_TERMINATED )
        break;
      DosSleep( 50 );
    }

    if ( ( ulRC != NO_ERROR ) && ( ulRC != ERROR_INVALID_THREADID ) )
    {
      debug( "DosWaitThread(), rc = %u. Use DosKillThread()...", ulRC );
      ulRC = DosKillThread( pTIF->tid );
      if ( ulRC != NO_ERROR )
        debug( "DosKillThread(), rc = %u", ulRC );
    }
    else
      debugCP( "  Ok" );
  }

  lnkseqFree( &pTIF->lsFrags, PFRAGPKT, free );
  lnkseqFree( &pTIF->lsNodes, PSCNODE, scFreeNode );

  if ( pTIF->hmtxNodes != NULLHANDLE )
  {
    ulRC = DosCloseMutexSem( pTIF->hmtxNodes );
    if ( ulRC != NO_ERROR )
      debug( "DosCloseMutexSem(), rc = %u", ulRC );
  }

  free( pTIF );
}

static ULONG fnLoad(ULONG cbBuf, PVOID pBuf, HFILE hFile)
{
  ULONG		ulRC;
  ULONG		cbActual;

  ulRC = DosRead( hFile, pBuf, cbBuf, &cbActual );
  if ( ulRC == NO_ERROR )
    return cbActual == cbBuf ? IPT_OK : IPT_END_OF_DATA;

  return IPT_IO_ERROR;
}

static ULONG fnStore(ULONG cbBuf, PVOID pBuf, HFILE hFile)
{
  ULONG		ulRC;
  ULONG		cbActual;

  ulRC = DosWrite( hFile, pBuf, cbBuf, &cbActual );
  if ( ulRC != NO_ERROR )
    return IPT_IO_ERROR;

  return IPT_OK;
}


BOOL iptInit()
{
  ULONG		ulRC;

  ulRC = DosCreateMutexSem( NULL, &hmtxIPTIF, 0, FALSE );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosCreateMutexSem(), rc = %u", ulRC );
    return FALSE;
  }

  lnkseqInit( &lsIPTIF );
  sock_init();
  scInit();

  return TRUE;
}

VOID iptDone()
{
  ULONG		ulRC = DosCloseMutexSem( hmtxIPTIF );

  if ( ulRC != NO_ERROR )
    debug( "DosCloseMutexSem(), rc = %u", ulRC );
  else
  {
    lnkseqFree( &lsIPTIF, PIPTIF, _iptFree );
    hmtxIPTIF = NULLHANDLE;
  }

  scDone();
}

PIPTIF iptStart(PSZ pszIFName)
{
  PIPTIF	pTIF = _iptNew( pszIFName );

  if ( ( pTIF != NULL ) && !_iptStart( pTIF ) )
  {
    _iptFree( pTIF );
    return NULL;
  }

  return pTIF;
}

VOID iptStop(PIPTIF pTIF)
{
  DosRequestMutexSem( hmtxIPTIF, SEM_INDEFINITE_WAIT );
  lnkseqRemove( &lsIPTIF, pTIF );
  DosReleaseMutexSem( hmtxIPTIF );

  _iptFree( pTIF );
}

PIPTIF iptGetTIF(PSZ pszName)
{
  PIPTIF	pTIF = (PIPTIF)lnkseqGetFirst( &lsIPTIF );

  while( ( pTIF != NULL ) && ( stricmp( iptGetTIFName(pTIF), pszName ) != 0 ) )
    pTIF = (PIPTIF)lnkseqGetNext( pTIF );

  return pTIF;
}

ULONG iptLoad(PSLOADFN pfnLoad, ULONG ulUser)
{
  ULONG      ulRC, ulHdrSign;
  ULONG      cTIF = 0;
  PIPTIF     pTIF;
  CHAR       acName[sizeof(pTIF->stIFReq.ifr_name)];

  ulRC = pfnLoad( sizeof(ULONG), &ulHdrSign, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  if ( ulHdrSign != DATA_HEADER )
    return IPT_INVALID_DATA;

  ulRC = pfnLoad( 1, &cTIF, ulUser );
  if ( ulRC != IPT_OK )
    return FALSE;

  if ( cTIF > 42 )
    return IPT_INVALID_DATA;

  DosRequestMutexSem( hmtxIPTIF, SEM_INDEFINITE_WAIT );

  for( ; cTIF > 0; cTIF-- )
  {
    ulRC = pfnLoad( sizeof(acName), acName, ulUser );
    if ( ulRC != IPT_OK )
      break;

    pTIF = _iptNew( acName );
    if ( pTIF == NULL )
    {
      ulRC = IPT_NOT_ENOUGH_MEMORY;
      break;
    }

    ulRC = scLoad( pfnLoad, ulUser, &pTIF->lsNodes );
    if ( ulRC != IPT_OK )
    {
      _iptFree( pTIF );
      break;
    }

    if ( !_iptStart( pTIF ) )
      debug( "_iptStart() for %s failed", acName );
  }

  DosReleaseMutexSem( hmtxIPTIF );

  return ulRC;
}

ULONG iptStore(PSSTOREFN pfnStore, ULONG ulUser)
{
  ULONG		ulRC;
  PIPTIF	pTIF;
  ULONG		ulHdrSign = DATA_HEADER;

  ulRC = fnStore( sizeof(ULONG), &ulHdrSign, ulUser );
  if ( ulRC != IPT_OK )
    return ulRC;

  DosRequestMutexSem( hmtxIPTIF, SEM_INDEFINITE_WAIT );

  ulRC = pfnStore( 1, &lsIPTIF.ulCount, ulUser );

  for( pTIF = (PIPTIF)lnkseqGetFirst( &lsIPTIF );
       ulRC == IPT_OK && pTIF != NULL; pTIF = (PIPTIF)lnkseqGetNext( pTIF ) )
  {
    ulRC = pfnStore( sizeof(pTIF->stIFReq.ifr_name), &pTIF->stIFReq.ifr_name,
                     ulUser );
    if ( ulRC == IPT_OK )
    {
      DosRequestMutexSem( pTIF->hmtxNodes, SEM_INDEFINITE_WAIT );
      ulRC = scStore( pfnStore, ulUser, &pTIF->lsNodes );
      DosReleaseMutexSem( pTIF->hmtxNodes );
    }
  }

  DosReleaseMutexSem( hmtxIPTIF );

  if ( ulRC != IPT_OK )
    debug( "rc = %u", ulRC );

  return ulRC;
}

BOOL iptLoadFile(PSZ pszFile)
{
  ULONG      ulRC;
  ULONG      ulAction;
  HFILE      hFile;

  ulRC = DosOpen( pszFile, &hFile, &ulAction, 0,
                  FILE_ARCHIVED | FILE_NORMAL,
                  OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYWRITE  |
                  OPEN_ACCESS_READONLY,
                  0 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosOpen(), rc = %u", ulRC );
    return FALSE;
  }

  ulRC = iptLoad( fnLoad, hFile );
  if ( ulRC != IPT_OK )
    debug( "iptLoad(), rc = %u", ulRC );

  DosClose( hFile );

  return ulRC == IPT_OK;
}

BOOL iptStoreFile(PSZ pszFile)
{
  ULONG		ulRC;
  ULONG		ulAction;
  HFILE		hFile;

  ulRC = DosOpen( pszFile, &hFile, &ulAction, 0,
                  FILE_ARCHIVED | FILE_NORMAL,
                  OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
                  OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYWRITE  |
                  OPEN_ACCESS_WRITEONLY,
                  0 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosOpen(), rc = %u", ulRC );
    return FALSE;
  }

  ulRC = iptStore( fnStore, hFile );
  if ( ulRC != IPT_OK )
    debug( "scStore(), rc = %u", ulRC );

  DosClose( hFile );

  return ulRC == IPT_OK;
}

ULONG iptSnapshot(PIPTSNAPSHOT pSnapshot, ULONG cbSnapshot, PULONG pcbActual)
{
  SCSNAPSHOTPARAM	stParam;

  stParam.pSnapshot = pSnapshot;
  stParam.cbSnapshot = cbSnapshot;
  stParam.cbActual = 0;

  DosRequestMutexSem( hmtxIPTIF, SEM_INDEFINITE_WAIT );

  for( stParam.pTIF = (PIPTIF)lnkseqGetFirst( &lsIPTIF );
       stParam.pTIF != NULL;
       stParam.pTIF = (PIPTIF)lnkseqGetNext( stParam.pTIF ) )
  {
    DosRequestMutexSem( stParam.pTIF->hmtxNodes, SEM_INDEFINITE_WAIT );
    scSnapshot( &stParam.pTIF->lsNodes, &stParam );
    DosReleaseMutexSem( stParam.pTIF->hmtxNodes );
  }

  DosReleaseMutexSem( hmtxIPTIF );

  *pcbActual = stParam.cbActual;

  return stParam.cbActual > cbSnapshot ? IPT_OVERFLOW : IPT_OK;
}
