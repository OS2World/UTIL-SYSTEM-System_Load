/*
  Public functions:

    BOOL traceStart(HWND hwndOwner, PSCNODE pNode)
      Opens packet tracer window for filter node pNode.

    BOOL traceCancel(PSCNODE pNode)
      Closes tracer window only if it was opened for filter node pNode.
*/

#define INCL_WIN
#include <ds.h>
#include <string.h>
#include <stdio.h>
#include "statcnt.h"
#include "traffic.h"
#include <debug.h>     // Must be the last.

#define TRACE_RECORDS		32

#define WM_TRACE_START		WM_USER + 100
#define TIMERID_UPDATE		1

extern HMODULE		hDSModule;	// Module handle, traffic.c.
extern HAB		hab;		// traffic.c.

// Helpers, traffic.c.
extern PHiniWriteULong		piniWriteULong;
extern PHiniReadULong		piniReadULong;
extern PHctrlDlgCenterOwner	pctrlDlgCenterOwner;
extern PHstrLoad		pstrLoad;

HWND		hwndTrace = NULLHANDLE;

typedef struct _TRACEREC {
  MINIRECORDCORE	stRecCore;
  BOOL			fInserted;

  ULONG			ulIndex;
  PSZ			pszRxTx;
  PSZ			pszProto;
  PSZ			pszSrc;
  PSZ			pszDst;
  ULONG			ulSize;

  CHAR			szSrc[24];
  CHAR			szDst[24];
} TRACEREC, *PTRACEREC;

typedef struct _DLGDATA {
  ULONG		ulTimer;
  PTRACEREC	pRecords;
  PTRACEREC	apRecords[TRACE_RECORDS];
} DLGDATA, *PDLGDATA;

static PSZ		apszRecStr[] = { "RX", "TX", "ICMP", "TCP", "UDP", "?" };

static VOID _traceUpdate(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PTRACEREC	pRecord;
  HWND		hwndCnt = WinWindowFromID( hwnd, ID_CONT_RECORDS );
  PSCNODE	pNode;
  SCTRACEPKT	aPackets[TRACE_RECORDS];
  PSCTRACEPKT	pPacket;
  ULONG		ulIdx;
  ULONG		cRecords;
  RECORDINSERT	stRecIns;
  ULONG		cbStr;
  PSZ		pszSrcBuf, pszDstBuf;

  // Query captured packets and current node.
  cRecords = scTraceQuery( &pNode, TRACE_RECORDS, &aPackets );
  if ( cRecords == 0 )
  {
    debugCP( "scTraceQuery(): No records returned" );
    return;
  }

  // Do not update container records if no new packets.
  if ( pDlgData->apRecords[cRecords - 1]->ulIndex ==
       aPackets[cRecords - 1].ulIndex )
    return;

  // Fill container's records with information about packets,

  WinEnableWindowUpdate( hwndCnt, FALSE );
  pRecord = pDlgData->pRecords;
  for( ulIdx = 0, pPacket = &aPackets; ulIdx < cRecords; ulIdx++, pPacket++ )
  {
    pRecord = pDlgData->apRecords[ulIdx];
    pRecord->ulIndex = pPacket->ulIndex;
    pRecord->ulSize = pNode->fHdrIncl ? pPacket->stPktInfo.cbIPPacket :
                                        pPacket->stPktInfo.cbPacket;

    switch( pPacket->stPktInfo.ulProto )
    {
      case STPROTO_ICMP:
        pRecord->pszProto = apszRecStr[2];
        break;

      case STPROTO_TCP:
        pRecord->pszProto = apszRecStr[3];
        break;

      case STPROTO_UDP:
        pRecord->pszProto = apszRecStr[4];
        break;

      default:
        pRecord->pszProto = apszRecStr[5];
    }

    if ( pPacket->stPktInfo.fReversChk )
    {
      pRecord->pszRxTx = apszRecStr[0]; // "RX"
      pszSrcBuf = &pRecord->szDst;
      pszDstBuf = &pRecord->szSrc;
    }
    else
    {
      pRecord->pszRxTx = apszRecStr[1]; // "TX"
      pszSrcBuf = &pRecord->szSrc;
      pszDstBuf = &pRecord->szDst;
    }

    cbStr = scIPAddrToStr( pPacket->stPktInfo.ulSrcAddr, 16, pszSrcBuf );
    if ( pPacket->stPktInfo.ulProto != STPROTO_ICMP )
      sprintf( &pszSrcBuf[cbStr], ":%u", pPacket->stPktInfo.ulSrcPort );

    cbStr = scIPAddrToStr( pPacket->stPktInfo.ulDstAddr, 16, pszDstBuf );
    if ( pPacket->stPktInfo.ulProto != STPROTO_ICMP )
      sprintf( &pszDstBuf[cbStr], ":%u", pPacket->stPktInfo.ulDstPort );

    if ( !pRecord->fInserted )
    {
      // Insert record to the container.
      stRecIns.cb = sizeof(RECORDINSERT);
      stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
      stRecIns.pRecordParent = NULL;
      stRecIns.zOrder = (USHORT)CMA_TOP;
      stRecIns.cRecordsInsert = 1;
      stRecIns.fInvalidateRecord = FALSE;
      WinSendMsg( hwndCnt, CM_INSERTRECORD, (PRECORDCORE)pRecord, &stRecIns );
      pRecord->fInserted = TRUE;
    }
  }

  WinEnableWindowUpdate( hwndCnt, TRUE );
  // Update records.
  WinSendMsg( hwndCnt, CM_INVALIDATERECORD, 0, 0 );
  // Scroll container to the last record.
  WinSendMsg( hwndCnt, CM_SCROLLWINDOW, MPFROMSHORT( CMA_VERTICAL ),
              MPFROMLONG( 4096 ) );
}

static BOOL _traceStart(HWND hwnd, PSCNODE pNode)
{
  HWND		hwndCnt = WinWindowFromID( hwnd, ID_CONT_RECORDS );
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  ULONG		ulIdx;
  CHAR		szBuf[64];

  if ( pDlgData == NULL )
    return FALSE;

  if ( pDlgData->ulTimer != 0 )
    WinStopTimer( hab, hwnd, pDlgData->ulTimer );

  // Remove all records from container (without freeing the memory).
  WinSendMsg( hwndCnt, CM_REMOVERECORD, 0, MPFROM2SHORT( 0, CMA_INVALIDATE ) );
  for( ulIdx = 0; ulIdx < TRACE_RECORDS; ulIdx++ )
  {
    pDlgData->apRecords[ulIdx]->fInserted = FALSE;
    pDlgData->apRecords[ulIdx]->ulIndex = 0;
  }

  if ( !scTraceStart( pNode, TRACE_RECORDS ) )
  {
    debugCP( "scTraceStart() failed" );
    return FALSE;
  }

  // Set window title - name of filter node.
  WinSetWindowText( hwnd, pNode->pszName );
  // Set Pause/Tracing button title: "Pause".
  pstrLoad( hDSModule, IDS_PAUSE, sizeof(szBuf), &szBuf );
  WinSetDlgItemText( hwnd, ID_PB_PAUSE, &szBuf );
  // Run timer for updates.
  pDlgData->ulTimer = WinStartTimer( hab, hwnd, TIMERID_UPDATE, 1000 );
  WinSetWindowPos( hwndTrace, HWND_TOP, 0, 0, 0, 0, SWP_SHOW | SWP_ACTIVATE );

  return TRUE;
}

static VOID _traceStop(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );

  if ( pDlgData == NULL )
    return;

  if ( pDlgData->ulTimer != 0 )
  {
    WinStopTimer( hab, hwnd, pDlgData->ulTimer );
    pDlgData->ulTimer = 0;
  }

  scTraceStop();
}

static VOID _dlgInit(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, ID_CONT_RECORDS );
  PFIELDINFO		pFldInf;
  PFIELDINFO		pFieldInfo;
  FIELDINFOINSERT	stFldInfIns = { 0 };
  CNRINFO		stCnrInf = { 0 };
  PDLGDATA		pDlgData;
  PTRACEREC		pRecords, pRecord, *ppRecord;
  CHAR			szBuf[128];
  ULONG			ulWidth;
  ULONG			ulHeight;

  ulWidth = piniReadULong( hDSModule, "traceWidth", 0 );
  ulHeight = piniReadULong( hDSModule, "traceHeight", 0 );
  if ( ( ulWidth > 300 ) && ( ulHeight > 200 ) )
    WinSetWindowPos( hwnd, 0, 0, 0, ulWidth, ulHeight, SWP_SIZE );

  // Fields: N, RX/TX, Protocol, Source, Destination, Size
  pFldInf = (PFIELDINFO)WinSendMsg( hwndCnt, CM_ALLOCDETAILFIELDINFO,
                                    MPFROMLONG( 6 ), NULL );
  if ( pFldInf == NULL )
  {
    debugCP( "CM_ALLOCDETAILFIELDINFO returns 0" );
    return;
  }
  pFieldInfo = pFldInf;

  pstrLoad( hDSModule, IDS_INDEX, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_ULONG | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = strdup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( TRACEREC, ulIndex );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_TXRX, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = strdup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( TRACEREC, pszRxTx );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_PROTOCOL, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = strdup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( TRACEREC, pszProto );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_SOURCE, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = strdup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( TRACEREC, pszSrc );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_DESTINATION, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = strdup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( TRACEREC, pszDst );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_PKT_SIZE, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_ULONG | CFA_HORZSEPARATOR | CFA_RIGHT;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = strdup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( TRACEREC, ulSize );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  stFldInfIns.cb = sizeof(FIELDINFOINSERT);
  stFldInfIns.pFieldInfoOrder = (PFIELDINFO)CMA_FIRST;
  stFldInfIns.cFieldInfoInsert = 6;
  WinSendMsg( hwndCnt, CM_INSERTDETAILFIELDINFO, MPFROMP( pFldInf ),
              MPFROMP( &stFldInfIns ) );

  stCnrInf.cb = sizeof(CNRINFO);
  stCnrInf.flWindowAttr = CV_DETAIL | CA_DETAILSVIEWTITLES;
  WinSendMsg( hwndCnt, CM_SETCNRINFO, &stCnrInf,
              MPFROMLONG( CMA_PFIELDINFOLAST | CMA_FLWINDOWATTR ) );

  // Allocate records.
  pRecords = (PTRACEREC)WinSendMsg( hwndCnt, CM_ALLOCRECORD,
                         MPFROMLONG( sizeof(TRACEREC) - sizeof(MINIRECORDCORE) ),
                         MPFROMLONG( TRACE_RECORDS ) );
  if ( pRecords == NULL )
  {
    debugCP( "CM_ALLOCRECORD returns 0" );
    return;
  }

  pDlgData = calloc( 1, sizeof(DLGDATA) );
  pDlgData->pRecords = pRecords;
  WinSetWindowULong( hwnd, QWL_USER, (ULONG)pDlgData );

  // Tune record's pointers to the text buffers, list all records at the array.
  for( pRecord = pRecords, ppRecord = &pDlgData->apRecords; pRecord != NULL;
       ppRecord++ )
  {
    pRecord->stRecCore.cb = sizeof(RECORDCORE);
    pRecord->pszSrc = &pRecord->szSrc;
    pRecord->pszDst = &pRecord->szDst;
    *ppRecord = pRecord;
    pRecord = (PTRACEREC)pRecord->stRecCore.preccNextRecord;
  }
}

static VOID _dlgDestroy(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  HWND		hwndCnt = WinWindowFromID( hwnd, ID_CONT_RECORDS );
  PFIELDINFO	pFieldInfo;
  SWP		swp;

  WinQueryWindowPos( hwnd, &swp );
  piniWriteULong( hDSModule, "traceWidth", swp.cx );
  piniWriteULong( hDSModule, "traceHeight", swp.cy );

  // We need call scTraceStop() for cases when WM_CLOSE was not received
  // (main program window closes while tracing runned).
  scTraceStop();

  if ( pDlgData != NULL )
  {
    if ( pDlgData->ulTimer != 0 )
      WinStopTimer( hab, hwnd, pDlgData->ulTimer );
    free( pDlgData );
  }

  // Free container's records.
  WinSendMsg( hwndCnt, CM_REMOVERECORD, 0, 0 );
  WinSendMsg( hwndCnt, CM_FREERECORD, MPFROMP( &pDlgData->apRecords ),
              MPFROMSHORT( TRACE_RECORDS ) );

  // Free titles strings of details view.
  pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO, 0,
                                       MPFROMSHORT( CMA_FIRST ) );
  while( ( pFieldInfo != NULL ) && ( (LONG)pFieldInfo != -1 ) )
  {
    if ( pFieldInfo->pTitleData != NULL )
      free( pFieldInfo->pTitleData );

    pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO,
                    MPFROMP( pFieldInfo ), MPFROMSHORT( CMA_NEXT ) );
  }
}

static VOID _cmdPBPause(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  ULONG		ulStrId;
  CHAR		szBuf[64];

  if ( pDlgData->ulTimer != 0 )
  {
    WinStopTimer( hab, hwnd, pDlgData->ulTimer );
    pDlgData->ulTimer = 0;
    ulStrId = IDS_TRACING;
  }
  else
  {
    _traceUpdate( hwnd );
    pDlgData->ulTimer = WinStartTimer( hab, hwnd, TIMERID_UPDATE, 1000 );
    ulStrId = IDS_PAUSE;
  }

  pstrLoad( hDSModule, ulStrId, sizeof(szBuf), &szBuf );
  WinSetDlgItemText( hwnd, ID_PB_PAUSE, &szBuf );
}

static VOID _dlgResized(HWND hwnd)
{
  HENUM		hEnum;
  HWND		hwndNext;
  HWND		hwndCnt;
  CHAR		szClass[128];
  RECTL		rect;
  ULONG		cButtons = 0;
  POINTL	aPt[2];
  struct BUTTON {
    HWND hwnd;
    SWP  swp;
  }		aButtons[4];

  hEnum = WinBeginEnumWindows( hwnd );

  while( ( hwndNext = WinGetNextWindow( hEnum ) ) != NULLHANDLE )
  {
    if ( WinQueryClassName( hwndNext, sizeof( szClass ), szClass ) == 0 )
      continue;

    if ( strcmp( &szClass, "#37" ) == 0 )
      hwndCnt = hwndNext;
    else if ( ( strcmp( szClass, "#3" ) == 0 ) &&
              ( cButtons < sizeof( aButtons ) / sizeof( struct BUTTON ) ) &&
              ( WinQueryWindowPos( hwndNext, &aButtons[cButtons].swp ) ) &&
              ( !( aButtons[cButtons].swp.fl & SWP_HIDE ) ) )
    {
      aButtons[cButtons].hwnd = hwndNext;
      cButtons++;
    }
  }

  WinEndEnumWindows( hEnum );

  if ( !WinQueryWindowRect( hwnd, &rect ) ||
       !WinCalcFrameRect( hwnd, &rect, TRUE ) )
    return;

  if( hwndCnt != NULLHANDLE )
  {
    aPt[0].x = rect.xLeft;
    aPt[0].y = rect.yBottom;
    aPt[1].x = rect.xRight;
    aPt[1].y = rect.yTop;

    if( cButtons != 0 )
    {
      WinMapDlgPoints( hwnd, aPt, 2, FALSE );
      aPt[0].y += 11;
      WinMapDlgPoints( hwnd, aPt, 2, TRUE  );
    }

    WinSetWindowPos( hwndCnt, 0, aPt[0].x,  aPt[0].y, aPt[1].x - aPt[0].x,
                     aPt[1].y - aPt[0].y, SWP_MOVE | SWP_SIZE );
  }

  // Adjust buttons.
  if ( cButtons != 0 )
  {
    ULONG	ulTotalWidth = cButtons * 2;
    ULONG	ulStart;
    ULONG	ulIdx;

    for( ulIdx = 0; ulIdx < cButtons; ulIdx++ )
      ulTotalWidth += aButtons[ulIdx].swp.cx;

    ulStart = ( rect.xRight - rect.xLeft + 1 - ulTotalWidth ) >> 1;

    for( ulIdx = 0; ulIdx < cButtons; ulIdx++ )
    {
      WinSetWindowPos( aButtons[ulIdx].hwnd, 0, ulStart, aButtons[ulIdx].swp.y,
                       0, 0, SWP_MOVE );
      WinInvalidateRect( aButtons[ulIdx].hwnd, NULL, FALSE );
      ulStart += aButtons[ulIdx].swp.cx + 2;
    }
  }

  // Scroll container to the last record.
  WinSendMsg( hwndCnt, CM_SCROLLWINDOW, MPFROMSHORT( CMA_VERTICAL ),
              MPFROMLONG( 4096 ) );
}


static MRESULT EXPENTRY wpfDialog(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _dlgInit( hwnd );
      _dlgResized( hwnd );
      break;

    case WM_DESTROY:
      _dlgDestroy( hwnd );
      break;

    case WM_TRACE_START:
      return (MRESULT)_traceStart( hwnd, (PSCNODE)mp1 );

    case WM_CLOSE:
      _traceStop( hwnd );
      break;

    case WM_TIMER:
      if ( SHORT1FROMMP(mp1) != TIMERID_UPDATE )
        break;
      _traceUpdate( hwnd );
      return (MRESULT)FALSE;

    case WM_ADJUSTWINDOWPOS:
      // Limit minimum window size.
      if ( ( (((PSWP)mp1)->fl & SWP_SIZE) != 0 ) &&
           ( (((PSWP)mp1)->cx < 300 ) || (((PSWP)mp1)->cy < 200 ) ) )
      {
        ((PSWP)mp1)->cx = max( 300, ((PSWP)mp1)->cx );
        ((PSWP)mp1)->cy = max( 200, ((PSWP)mp1)->cy );
      }
      break;

    case WM_WINDOWPOSCHANGED:
      if ( (((PSWP)mp1)[0].fl & SWP_SIZE) != 0 )
        _dlgResized( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case ID_PB_PAUSE:
          _cmdPBPause( hwnd );
          break;

        case ID_PB_CLOSE:
          WinSendMsg( hwnd, WM_CLOSE, 0, 0 );
          break;
      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


BOOL traceStart(HWND hwndOwner, PSCNODE pNode)
{
  if ( pNode == NULL )
  {
    debugCP( "pNode is NULL" );
    return FALSE;
  }

  if ( hwndTrace == NULLHANDLE )
  {
    // First call traceStart() - create dialog window.
    hwndTrace = WinLoadDlg( HWND_DESKTOP, hwndOwner, wpfDialog, hDSModule,
                            ID_DLG_TRACE, NULL );
    if ( hwndTrace == NULLHANDLE )
    {
      debugCP( "WinLoadDlg() fail" );
      return FALSE;
    }
    pctrlDlgCenterOwner( hwndTrace );
  }

  // Send start command, it will show a dialog window.
  return (BOOL)WinSendMsg( hwndTrace, WM_TRACE_START, MPFROMP(pNode), 0 );
}

BOOL traceCancel(PSCNODE pNode)
{
  PSCNODE	pCurrentNode;

  if ( ( hwndTrace == NULLHANDLE ) || ( pNode == NULL ) )
    return FALSE;

  scTraceQuery( &pCurrentNode, 0, NULL );
  if ( pNode != pCurrentNode )
    return FALSE;

  WinSendMsg( hwndTrace, WM_CLOSE, 0, 0 );
  return TRUE;
}
