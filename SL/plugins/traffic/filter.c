/*
  Filter building dialog.
*/

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#define INCL_WINDIALOGS
#define INCL_WINSYS
#define INCL_WINWORKPLACE
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <ds.h>
#include "traffic.h"
#include "iptrace.h"
#include "linkseq.h"
#include "strutils.h"
#include <debug.h>


extern HMODULE		hDSModule;	// Module handle, traffic.c.
extern HAB		hab;		// traffic.c.

// Helpers, traffic.c.
extern PHiniWriteULong			piniWriteULong;
extern PHiniReadULong			piniReadULong;
extern PHctrlDlgCenterOwner		pctrlDlgCenterOwner;
extern PHctrlQueryText			pctrlQueryText;
extern PHstrFromULL			pstrFromULL;
extern PHutilLoadInsertStr		putilLoadInsertStr;
extern PHhelpShow			phelpShow;

#define WM_LR_CTRLFOCUSED	WM_USER + 100
#define WM_LR_QUERYHWND		WM_USER + 101

#define WC_FILTERLAYER		"filterLayer"

// Container, tree view - node record
typedef struct _NODERECORD {
  MINIRECORDCORE	stRecCore;
  ULONG			ulOrder;
  PIPTIF		pTIF;
  PSCNODE		pSCNode;
} NODERECORD, *PNODERECORD;

typedef struct _DLGDATA {
  HWND		hwndMenuNew;
  HWND		hwndMenuNesting;
  HWND		hwndMenuNode;
  RECTL		rectSrcAddrSel;
  RECTL		rectDstAddrSel;
  HWND		hwndAddrEF;
  PNODERECORD	pCurRecord;
  PNODERECORD	pCtxMenuRecord;
  ULONG		ulCountersTimer;
  BOOL		fJumpToNode;
  BOOL		fEditorChanged;
} DLGDATA, *PDLGDATA;

typedef struct _FILTERCDATA {
  USHORT	cbData;
  PVOID		pSCNodeUser;
} FILTERCDATA, *PFILTERCDATA;

static PFNWP		pwpfEFOrg;
static PFNWP		pwpfAddrLBPrev;

// traceStart(), traceCancel() -  tracewin.c
extern BOOL traceStart(HWND hwndOwner, PSCNODE pNode);
extern BOOL traceCancel(PSCNODE pNode);

//		Layers
//		======

// MRESULT EXPENTRY wpfLayerControl(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
//
// Subclass window procedure for all controls in the layer.

static MRESULT EXPENTRY wpfLayerControl(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  PFNWP		pwpfOrg = (PFNWP)WinQueryWindowULong( hwnd, QWL_USER );

  switch( msg )
  {
    case WM_SETFOCUS:
      if ( SHORT1FROMMP(mp2) != 0 )
        WinSendMsg( WinQueryWindow( hwnd, QW_PARENT ), WM_LR_CTRLFOCUSED,
                    MPFROMHWND(hwnd), 0 );
      break;

    case WM_CHAR:
      {
        USHORT		usFlags = SHORT1FROMMP( mp1 );
        UCHAR		ucScan = SHORT2FROMMP( mp1 ) >> 8;

        if ( ( (usFlags & (KC_SCANCODE | KC_KEYUP)) == KC_SCANCODE ) &&
             ( ucScan == 15 ) )
        {
          HWND		hwndFocus;
          ULONG		ulCode;
          ULONG		ulQWOrd = (usFlags & KC_SHIFT) != 0 ? QW_PREV : QW_NEXT;
          ULONG		ulQWFirst = (usFlags & KC_SHIFT) != 0 ? QW_BOTTOM : QW_TOP;

          hwndFocus = WinQueryWindow( hwnd, ulQWOrd );
          while( hwndFocus != hwnd )
          {
            ulCode = (ULONG)WinSendMsg( hwndFocus, WM_QUERYDLGCODE, 0, 0 );
            if ( ulCode != 0 && ulCode != DLGC_STATIC )
            {
              WinFocusChange( HWND_DESKTOP, hwndFocus, 0 );
              WinSendMsg( hwndFocus, WM_SETFOCUS, (MPARAM)hwndFocus, (MPARAM)TRUE );
              WinSendMsg( WinQueryWindow( hwnd, QW_PARENT ), WM_LR_CTRLFOCUSED,
                          MPFROMHWND(hwndFocus), 0 );
              break;
            }
            hwndFocus = WinQueryWindow( hwndFocus, ulQWOrd );

            if ( hwndFocus == NULLHANDLE )
              hwndFocus = WinQueryWindow( WinQueryWindow( hwnd, QW_PARENT ),
                                          ulQWFirst );
          }

          return (MRESULT)TRUE;
        }
      }
  }

  return pwpfOrg( hwnd, msg, mp1, mp2 );
}

// MRESULT EXPENTRY wpfLayer(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
//
// Window procedure for layer.

static MRESULT EXPENTRY wpfLayer(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  HWND		hwndFocus;

  switch( msg )
  {
    case WM_LR_QUERYHWND:
      {
        ULONG		ulCtrlId = LONGFROMMP( mp1 );
        HWND		hwndFind = WinWindowFromID( hwnd, ulCtrlId );
        HWND		hwndControl;
        HENUM		hEnum;

        if ( hwndFind != NULLHANDLE )
          return (MRESULT)hwndFind;

        hEnum = WinBeginEnumWindows( hwnd );
        while( ( hwndControl = WinGetNextWindow( hEnum ) ) != NULLHANDLE )
        {
          hwndFind = (HWND)WinSendMsg( hwndControl, WM_LR_QUERYHWND, mp1, 0 );
          if ( hwndFind != NULLHANDLE )
            break;
        }
        WinEndEnumWindows( hEnum );

        return (MRESULT)hwndFind;
      }

    case WM_PAINT:
      {
        RECTL		rectLayer;
        HPS		hps = WinBeginPaint( hwnd, 0, &rectLayer );
        HWND		hwndControl;
        HENUM		hEnum;
        SWP		swp;
        RECTL		arectControl[30];
        ULONG		cControls = 0;
        HRGN		hrgnInvalid, hrgnValid, hrgnUpdate;
        CHAR		szClass[64];

        hEnum = WinBeginEnumWindows( hwnd );
        while( ( hwndControl = WinGetNextWindow( hEnum ) ) != NULLHANDLE )
        {
          if ( cControls == ARRAYSIZE(arectControl) )
          {
            WinEndEnumWindows( hEnum );
            WinFillRect( hps, &rectLayer, SYSCLR_DIALOGBACKGROUND );
            return (MRESULT)0;
          }

          if (
               ( WinQueryClassName( hwndControl, sizeof(szClass), &szClass ) > 0 )
             &&
               (
                 ( strcmp( &szClass, "#2" ) == 0 )
               ||
                 (
                   ( strcmp( &szClass, "#5" ) == 0 )
                 &&
                   ( (WinQueryWindowULong( hwndControl, QWL_STYLE ) & SS_GROUPBOX) != 0 )
                 )
               )
             )
              continue;

          WinQueryWindowPos( hwndControl, &swp );
          WinSetRect( hab, &arectControl[cControls++],
                      swp.x, swp.y, swp.x + swp.cx, swp.y + swp.cy );
        }
        WinEndEnumWindows( hEnum );

        // hrgnInvalid - invalid region, whole window
        hrgnInvalid = GpiCreateRegion( hps, 1, &rectLayer );
        // hrgnValid - rectangles of controls
        hrgnValid = GpiCreateRegion( hps, cControls, &arectControl );
        // hrgnUpdate - region to update is ( hrgnInvalid AND NOT hrgnValid )
        hrgnUpdate = GpiCreateRegion( hps, 0, NULL );
        GpiCombineRegion( hps, hrgnUpdate, hrgnInvalid, hrgnValid, CRGN_DIFF );
        GpiDestroyRegion( hps, hrgnValid );
        GpiDestroyRegion( hps, hrgnInvalid );
        // Fill result region
        GpiSetColor( hps, SYSCLR_DIALOGBACKGROUND );
        GpiPaintRegion( hps, hrgnUpdate );
        GpiDestroyRegion( hps, hrgnUpdate );
      }
      return (MRESULT)0;

    case WM_LR_CTRLFOCUSED:
      WinSetWindowULong( hwnd, 0, HWNDFROMMP(mp1) );
      return (MRESULT)FALSE;

    case WM_SETFOCUS:
      hwndFocus = WinQueryWindowULong( hwnd, 0 );
      if ( hwndFocus != NULLHANDLE )
        WinSendMsg( hwndFocus, WM_SETFOCUS, (MPARAM)mp1, (MPARAM)mp2 );
      return (MRESULT)FALSE;

    case WM_MATCHMNEMONIC:
      {
        HENUM		hEnum = WinBeginEnumWindows( hwnd );

        while( ( hwndFocus = WinGetNextWindow( hEnum ) ) != NULLHANDLE )
        {
          if ( (BOOL)WinSendMsg( hwndFocus, WM_MATCHMNEMONIC, mp1, mp2 ) )
          {
            WinSendMsg( hwndFocus, WM_SETFOCUS, (MPARAM)hwndFocus, (MPARAM)TRUE );
            WinSetWindowULong( hwnd, 0, hwndFocus );
            break;
          }
        }
        WinEndEnumWindows( hEnum );
        return (MRESULT)( hwndFocus != NULLHANDLE );
      }

    case WM_SETSELECTION:
      hwndFocus = WinQueryWindowULong( hwnd, 0 );
      if ( hwndFocus != NULLHANDLE )
        WinSendMsg( hwndFocus, WM_SETSELECTION, mp1, mp2 );
      return (MRESULT)FALSE;

    case WM_CHAR:
      hwndFocus = WinQueryWindowULong( hwnd, 0 );
      if ( ( hwndFocus != NULLHANDLE ) &&
           (BOOL)WinSendMsg( hwndFocus, WM_CHAR, mp1, mp2 ) )
        return (MRESULT)TRUE;

      return (MRESULT)TRUE;

    case WM_QUERYDLGCODE:
      return (MRESULT)DLGC_STATIC;

    case WM_QUERYCTLTYPE:
      return (MRESULT)CCT_STATICTEXT;
  }

//  return pwpfLayerOrg( hwnd, msg, mp1, mp2 );
  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}

// VOID _layerClientRect(HWND hwnd, PRECTL pRect)
//
// Detect rectangle of main layer which will contain all the other layers and
// controls.
// Called from _dialogSize(), _layerSetLayers().

static VOID _layerClientRect(HWND hwnd, PRECTL pRect)
{
  ULONG		ulWidth;

  WinQueryWindowRect( hwnd, pRect );
  ulWidth = pRect->xRight - pRect->xLeft;

  WinCalcFrameRect( hwnd, pRect, TRUE );

  pRect->yTop--;
  pRect->xLeft = WinQuerySysValue( HWND_DESKTOP, SV_CXDLGFRAME );
  pRect->xRight = ulWidth - pRect->xLeft;
}

// VOID _layerWinCtrlRect(HWND hwnd, PULONG aId, ULONG cId, PRECTL pRect)
//
// Detect rectangle for all controls listed in aId.

static VOID _layerWinCtrlRect(HWND hwnd, PULONG aId, ULONG cId, PRECTL pRect)
{
  ULONG		ulIdx;
  HWND		hControl;
  RECTL		rect;
  SWP		swp;

  WinSetRectEmpty( hab, pRect );
  for( ulIdx = 0; ulIdx < cId; ulIdx++ )
  {
    hControl = WinWindowFromID( hwnd, aId[ulIdx] );
    if ( hControl == NULLHANDLE )
      continue;

    WinQueryWindowPos( hControl, &swp );
    swp.x -= WinQuerySysValue( HWND_DESKTOP, SV_CXDLGFRAME );
    swp.y -= WinQuerySysValue( HWND_DESKTOP, SV_CYDLGFRAME );
    WinSetRect( hab, &rect, swp.x, swp.y - 1, swp.x + swp.cx, swp.y + swp.cy );
    WinUnionRect( hab, pRect, pRect, &rect );
  }
}

// HWND _layerCreate(HWND hwnd, ULONG ulId, PRECTL pRect)
//
// Creates layer and insert it in hwnd.

static HWND _layerCreate(HWND hwnd, ULONG ulId, PRECTL pRect)
{
  HWND		hwndLayer =
                  WinCreateWindow(
                    hwnd, WC_FILTERLAYER, NULL, WS_VISIBLE | SS_TEXT | WS_GROUP,
                    pRect->xLeft, pRect->yBottom,
                    pRect->xRight - pRect->xLeft,
                    pRect->yTop - pRect->yBottom,
                    hwnd, HWND_TOP, ulId, NULL, NULL);

  return hwndLayer;
}

// VOID _layerSetControl(HWND hwndLayer, HWND hwndControl)
//
// Moves control (set parent) to given layer.
// Called from _layerSetLayers(), _layerSetControls().

static VOID _layerSetControl(HWND hwndLayer, HWND hwndControl)
{
  POINTL	pt;
  SWP		swp;

  WinQueryWindowPos( hwndControl, &swp );
  pt.x = swp.x;
  pt.y = swp.y;
  WinMapWindowPoints( WinQueryWindow( hwndControl, QW_PARENT ), hwndLayer,
                      &pt, 1 );

  WinSetParent( hwndControl, hwndLayer, FALSE );
  WinSetWindowPos( hwndControl, HWND_TOP, pt.x, pt.y, 0, 0, SWP_MOVE );

  WinSetWindowULong( hwndControl, QWL_USER,
                     (ULONG)WinSubclassWindow( hwndControl, wpfLayerControl ) );
}

// VOID _layerSetControls(HWND hwnd, HWND hwndLayer, PULONG aId, ULONG cId)
//
// Moves dialog's controls listed in aId to layer hwndLayer.
// Called from _layerSetLayers().

static VOID _layerSetControls(HWND hwnd, HWND hwndLayer, PULONG aId, ULONG cId)
{
  for( ; cId > 0; cId-- )
    _layerSetControl( hwndLayer, WinWindowFromID( hwnd, aId[cId - 1] ) );
}

// static VOID _layerSetLayers(HWND hwnd)
//
// Creating a floating layers and move controls into these.
// Called from WM_INITDLG of main window.

static VOID _layerSetLayers(HWND hwnd)
{
  static ULONG		aidNodeEditor[] =
  {
    ID_GB_NODE, ID_ST_COMMENTS, ID_MLE_COMMENTS, ID_CB_STOP, ID_CB_IPHDR,
    ID_ST_PROTO, ID_CB_PROTO, ID_ST_SRCADDR, ID_LB_SRCADDR, ID_ST_SRCPORT,
    ID_EF_SRCPORT, ID_ST_DSTADDR, ID_LB_DSTADDR, ID_ST_DSTPORT, ID_EF_DSTPORT,
    ID_ST_SENT, ID_ST_SENTVAL, ID_ST_RECV, ID_ST_RECVVAL, ID_PB_RESET, ID_PB_APPLY
  };

  static ULONG		aidNodesButtons[] =
  {
    ID_PB_NEW, ID_PB_MOVE_UP, ID_PB_MOVE_DOWN, ID_PB_NESTING, ID_PB_DELETE,
    ID_PB_HELP
  };

  HWND			hwndMainLayer;
  HWND			hwndLayer;
  HWND			hwndRightLayer;
  RECTL			rectMainLayer;
  RECTL			rectNodeEditorLayer;
  RECTL			rectRightLayer;
  RECTL			rectBottomLayer;
  HWND			hwndControl;

  WinRegisterClass( hab, WC_FILTERLAYER, wpfLayer, 0, sizeof(ULONG) );

  // Main layer.

  _layerClientRect( hwnd, &rectMainLayer );
  hwndMainLayer = _layerCreate( hwnd, ID_LAYER_MAIN, &rectMainLayer );

  // Bottom layer (buttons).

  _layerWinCtrlRect( hwnd, &aidNodesButtons, ARRAYSIZE(aidNodesButtons), &rectBottomLayer );
  rectBottomLayer.xRight = rectMainLayer.xRight - rectMainLayer.xLeft;
  rectBottomLayer.yBottom = 0;
  rectBottomLayer.xLeft = 0;
  rectBottomLayer.yTop += WinQuerySysValue( HWND_DESKTOP, SV_CYDLGFRAME );
  hwndLayer = _layerCreate( hwndMainLayer, ID_LAYER_BOTTOM, &rectBottomLayer );
  _layerSetControls( hwnd, hwndLayer, &aidNodesButtons, ARRAYSIZE(aidNodesButtons) );

  // Right layer.

  _layerWinCtrlRect( hwnd, &aidNodeEditor, ARRAYSIZE(aidNodeEditor), &rectNodeEditorLayer );
  rectRightLayer.xLeft = rectNodeEditorLayer.xLeft - WinQuerySysValue( HWND_DESKTOP, SV_CXDLGFRAME );
  rectRightLayer.yBottom = rectBottomLayer.yTop;
  rectRightLayer.xRight = rectMainLayer.xRight - rectMainLayer.xLeft;
  rectRightLayer.yTop = rectMainLayer.yTop - rectMainLayer.yBottom;
  hwndRightLayer = _layerCreate( hwndMainLayer, ID_LAYER_RIGHT, &rectRightLayer );

  // Node editor layer in the right layer.

  WinOffsetRect( hab, &rectNodeEditorLayer,
                 -rectRightLayer.xLeft, -rectRightLayer.yBottom );
  rectNodeEditorLayer.yTop = rectRightLayer.yTop - rectRightLayer.yBottom;
  hwndLayer = _layerCreate( hwndRightLayer, ID_LAYER_NODEEDITOR, &rectNodeEditorLayer );

  _layerSetControls( hwnd, hwndLayer, &aidNodeEditor, ARRAYSIZE(aidNodeEditor) );

  // Container.

  hwndControl = WinWindowFromID( hwnd, ID_CONT_NODES );
  _layerSetControl( hwndMainLayer, hwndControl );
  WinSetWindowPos( hwndControl, 0, 0, rectBottomLayer.yTop,
                   rectRightLayer.xLeft,
                   rectRightLayer.yTop - rectRightLayer.yBottom,
                   SWP_SIZE | SWP_MOVE );
}

// HWND _layerCtrlFromId(HWND hwnd, ULONG ulCtrlId)
//
// Returns window handle for control id ulCtrlId.

static HWND _layerCtrlFromId(HWND hwnd, ULONG ulCtrlId)
{
  HWND		hwndLayer = WinWindowFromID( hwnd, ID_LAYER_MAIN );

  return (HWND)WinSendMsg( hwndLayer, WM_LR_QUERYHWND, MPFROMLONG(ulCtrlId), 0 );
}

#define LayerEnableControl(hwndDlg, usId, fEnable) \
  WinEnableWindow( _layerCtrlFromId( hwndDlg, usId ), fEnable )

#define LayerSendItemMsg(hwndDlg, usId, msg, mp1, mp2) \
  WinSendMsg( _layerCtrlFromId( hwndDlg, usId ), msg, mp1, mp2 )

#define LayerCheckButton(hwndDlg, usId, usChkstate) \
  ((USHORT)LayerSendItemMsg( hwndDlg, usId, BM_SETCHECK, \
                                MPFROMSHORT(usChkstate), 0 ))

#define LayerQueryButtonCheckstate(hwndDlg, usId) \
  ((USHORT)LayerSendItemMsg( hwndDlg, usId, BM_QUERYCHECK, 0, 0 ))

#define LayerSetDlgItemText(hwndDlg, usId, pszText) \
  WinSetWindowText( _layerCtrlFromId( hwndDlg, usId ), pszText )

#define LayerQueryDlgItemText(hwndDlg, usId, lMaxText, pszText) \
  WinQueryWindowText( _layerCtrlFromId( hwndDlg, usId ), lMaxText, pszText )


//		Node editor
//		===========

static VOID __neFillAddr(HWND hwndLB, PSCADDR pAddr)
{
  CHAR		szBuf[34];

  WinSendMsg( hwndLB, LM_DELETEALL, 0, 0 );

  if ( pAddr != NULL )
  {
    for( ; pAddr->ulLast != 0; pAddr++ )
    {
      scAddrToStr( pAddr, sizeof(szBuf), &szBuf );
      WinSendMsg( hwndLB, LM_INSERTITEM, MPFROMLONG( LIT_END ), MPFROMP( &szBuf ) );
    }
  }
  WinSendMsg( hwndLB, LM_INSERTITEM, MPFROMLONG( LIT_END ), MPFROMP( "" ) );
}

static VOID __neFillPorts(HWND hwndEF, PSCPORT pPortLst)
{
  ULONG		cbBuf;
  PCHAR		pcBuf;

  cbBuf = scPortLstToStr( pPortLst, 0, NULL );
  if ( cbBuf == 0 )
  {
    WinSetWindowText( hwndEF, NULL );
    return;
  }

  cbBuf++; // + zero
  pcBuf = debugMAlloc( cbBuf );
  if ( pcBuf != NULL )
  {
    scPortLstToStr( pPortLst, cbBuf, pcBuf );
    WinSetWindowText( hwndEF, pcBuf );
    debugFree( pcBuf );
  }
}

static VOID __neFillCounter(HWND hwndEF, ULLONG ullVal)
{
  CHAR		szBuf[32];

  pstrFromULL( &szBuf, sizeof(szBuf), ullVal );
  WinSetWindowText( hwndEF, &szBuf );
}

static BOOL __neLBToAddrLst(HWND hwndLB, ULONG ulMaxAddrs, PSCADDR pAddrLst)
{
  ULONG		ulCount = (SHORT)WinSendMsg( hwndLB, LM_QUERYITEMCOUNT, 0, 0 );
  ULONG		ulIdx;
  CHAR		szBuf[35];

  for( ulIdx = 0; ulIdx < ulCount; ulIdx++ )
  {
    if ( (SHORT)WinSendMsg( hwndLB, LM_QUERYITEMTEXT,
                            MPFROM2SHORT( ulIdx, sizeof(szBuf) ),
                            MPFROMP( &szBuf ) ) == 0 )
      continue;

    if ( ( ulMaxAddrs <= 1 ) || !scStrToAddr( &szBuf, pAddrLst ) )
      return FALSE;

    ulMaxAddrs--;
    pAddrLst++;
  }

  pAddrLst->ulFirst = 0;
  pAddrLst->ulLast = 0;
  return TRUE;
}

static VOID _neChanged(HWND hwnd, BOOL fChanged)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );

  if ( pDlgData->fEditorChanged == fChanged )
    return;

  LayerEnableControl( hwnd, ID_PB_APPLY, fChanged );
  pDlgData->fEditorChanged = fChanged;
}

// VOID _neProtoChanged(HWND hwnd, ULONG ulCtrlId, HWND hwndLB)
//
// Called from WM_CONTROL of main window.

static VOID _neProtoChanged(HWND hwnd, ULONG ulCtrlId, HWND hwndLB)
{
  BOOL	fICMP = (SHORT)LayerSendItemMsg( hwnd, ID_CB_PROTO, LM_QUERYSELECTION,
                                         MPFROMSHORT( LIT_CURSOR ), 0 ) >= 3;

  LayerEnableControl( hwnd, ID_EF_SRCPORT, !fICMP );
  LayerEnableControl( hwnd, ID_EF_DSTPORT, !fICMP );
  _neChanged( hwnd, TRUE );
}

static ULONG __neMessageBox(HWND hwnd, ULONG ulId, ULONG ulStyle)
{
  CHAR		szTitle[32];
  CHAR		szMsg[256];

  WinLoadMessage( hab, hDSModule, ulId, sizeof(szMsg), &szMsg );
  WinQueryWindowText( hwnd, sizeof(szTitle), &szTitle );
  return WinMessageBox( HWND_DESKTOP, hwnd, &szMsg, &szTitle, 0, ulStyle );
}

static VOID _neResetCounters(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PNODERECORD	pCurRecord = pDlgData->pCurRecord;
  ULONG		ulAnswer;

  if ( ( pCurRecord == NULL ) || ( pCurRecord->pSCNode == NULL ) )
    return;

  if ( lnkseqGetCount( &pCurRecord->pSCNode->lsNodes ) == 0 )
    ulAnswer = __neMessageBox( hwnd, IDM_RESET_CONF,
                               MB_MOVEABLE | MB_OKCANCEL | MB_ICONQUESTION );
  else
    ulAnswer = __neMessageBox( hwnd, IDM_RESET_BRANCH_CONF,
                               MB_MOVEABLE | MB_YESNOCANCEL | MB_ICONQUESTION );

  if ( ulAnswer == MBID_CANCEL )
    return;

  iptLockNodes( pCurRecord->pTIF );
  pCurRecord->pSCNode->ullSent = 0;
  pCurRecord->pSCNode->ullRecv = 0;
  if ( ulAnswer == MBID_YES )
    scResetCounters( &pCurRecord->pSCNode->lsNodes );
  iptUnlockNodes( pCurRecord->pTIF );

  __neFillCounter( _layerCtrlFromId( hwnd, ID_ST_SENTVAL ), 0 );
  __neFillCounter( _layerCtrlFromId( hwnd, ID_ST_RECVVAL ), 0 );
}

// BOOL _neApplyNode(HWND hwnd, BOOL fConfirm)
//
// Apply changes for current edit filter node.
// Returns TRUE on success.

static BOOL _neApplyNode(HWND hwnd, BOOL fConfirm)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PNODERECORD	pCurRecord = pDlgData->pCurRecord;
  SCNODE	stSCNode = { 0 };
  SCADDR	astSrcAddr[256];
  SCADDR	astDstAddr[256];
  SCPORT	astSrcPort[256];
  SCPORT	astDstPort[256];
  HWND		hwndControl;
  CHAR		szBuf[512];
  PSCNODE	pNewSCNode;

  if ( !pDlgData->fEditorChanged || pCurRecord == NULL ||
       pCurRecord->pSCNode == NULL )
    return TRUE;

  if ( fConfirm &&
       ( __neMessageBox( hwnd, IDM_APPLY_CHANGES_CONF,
                         MB_MOVEABLE | MB_YESNO | MB_ICONQUESTION ) !=
         MBID_YES ) )
  {
    return TRUE;
  }

  stSCNode.pszName = pCurRecord->pSCNode->pszName;
  stSCNode.pUser = pCurRecord->pSCNode->pUser;
  stSCNode.pszComment = &szBuf;
  stSCNode.pSrcAddrLst = &astSrcAddr;
  stSCNode.pDstAddrLst = &astDstAddr;
  stSCNode.pSrcPortLst = &astSrcPort;
  stSCNode.pDstPortLst = &astDstPort;

  stSCNode.fHdrIncl = (BOOL)LayerQueryButtonCheckstate( hwnd, ID_CB_IPHDR );
  stSCNode.fStop = (BOOL)LayerQueryButtonCheckstate( hwnd, ID_CB_STOP );

  switch( (SHORT)LayerSendItemMsg( hwnd, ID_CB_PROTO, LM_QUERYSELECTION,
                                      MPFROMSHORT( LIT_CURSOR ), 0 ) )
  {
    case 0:
      stSCNode.ulProto = STPROTO_TCP;
      break;

    case 1:
      stSCNode.ulProto = STPROTO_UDP;
      break;

    case 2:
      stSCNode.ulProto = STPROTO_TCP | STPROTO_UDP;
      break;

    case 3:
      stSCNode.ulProto = STPROTO_ICMP;
      break;

    default:
      stSCNode.ulProto = STPROTO_TCP | STPROTO_UDP | STPROTO_ICMP;
  };

  hwndControl = _layerCtrlFromId( hwnd, ID_LB_SRCADDR );
  if ( !__neLBToAddrLst( hwndControl, ARRAYSIZE( astSrcAddr ), &astSrcAddr ) )
  {
    __neMessageBox( hwnd, IDM_INVALID_SRCADDR,
                    MB_MOVEABLE | MB_OK | MB_ICONHAND );
    WinSetFocus( HWND_DESKTOP, hwndControl );
    return FALSE;
  }

  hwndControl = _layerCtrlFromId( hwnd, ID_EF_SRCPORT );
  if ( pctrlQueryText( hwndControl, sizeof(szBuf), &szBuf ) == 0 )
    stSCNode.pSrcPortLst = NULL;
  else if ( !scStrToPortLst( &szBuf, ARRAYSIZE( astSrcPort ), &astSrcPort ) )
  {
    __neMessageBox( hwnd, IDM_INVALID_SRCPORT,
                    MB_MOVEABLE | MB_OK | MB_ICONHAND );
    WinSetFocus( HWND_DESKTOP, hwndControl );
    return FALSE;
  }

  hwndControl = _layerCtrlFromId( hwnd, ID_LB_DSTADDR );
  if ( !__neLBToAddrLst( hwndControl, ARRAYSIZE( astDstAddr ), &astDstAddr ) )
  {
    __neMessageBox( hwnd, IDM_INVALID_DSTADDR,
                    MB_MOVEABLE | MB_OK | MB_ICONHAND );
    WinSetFocus( HWND_DESKTOP, hwndControl );
    return FALSE;
  }

  hwndControl = _layerCtrlFromId( hwnd, ID_EF_DSTPORT );
  if ( pctrlQueryText( hwndControl, sizeof(szBuf), &szBuf ) == 0 )
    stSCNode.pDstPortLst = NULL;
  else if ( !scStrToPortLst( &szBuf, ARRAYSIZE( astDstPort ), &astDstPort ) )
  {
    __neMessageBox( hwnd, IDM_INVALID_DSTPORT,
                    MB_MOVEABLE | MB_OK | MB_ICONHAND );
    WinSetFocus( HWND_DESKTOP, hwndControl );
    return FALSE;
  }

  LayerQueryDlgItemText( hwnd, ID_MLE_COMMENTS, sizeof(szBuf), &szBuf );

  // Allocate new filter node from stSCNode.
  pNewSCNode = scNewNode( &stSCNode );
  if ( pNewSCNode != NULL )
  {
    HWND		hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
    PNODERECORD		pParent = (PNODERECORD)WinSendMsg( hwndNodes,
                                    CM_QUERYRECORD, MPFROMP( pCurRecord ),
                                    MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );
    PLINKSEQ		plsNodes;

    plsNodes = pParent->pSCNode == NULL ?
                 iptGetTIFNodes( pParent->pTIF ) : &pParent->pSCNode->lsNodes;

    iptLockNodes( pParent->pTIF );
    // Copy counters from old filter node to new.
    pNewSCNode->ullSent = pCurRecord->pSCNode->ullSent;
    pNewSCNode->ullRecv = pCurRecord->pSCNode->ullRecv;
    // Move subnodes list from old filter node to new.
    lnkseqMove( &pNewSCNode->lsNodes, &pCurRecord->pSCNode->lsNodes );
    // Replace old filter node with new
    lnkseqReplace( plsNodes, pCurRecord->pSCNode, pNewSCNode );
    iptUnlockNodes( pParent->pTIF );

    if ( traceCancel( pCurRecord->pSCNode ) )
      traceStart( WinQueryWindow( hwnd, QW_OWNER ), pNewSCNode );

    scFreeNode( pCurRecord->pSCNode );
    pCurRecord->pSCNode = pNewSCNode;
    pCurRecord->stRecCore.pszIcon = pNewSCNode->pszName;
  }

  _neChanged( hwnd, FALSE );
  return TRUE;
}

static VOID _neUpdateCounters(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PNODERECORD	pCurRecord = pDlgData->pCurRecord;
  ULLONG	ullSent;
  ULLONG	ullRecv;

  if ( ( pCurRecord == NULL ) || ( pCurRecord->pSCNode == NULL ) )
    return;

  iptLockNodes( pCurRecord->pTIF );
  ullSent = pCurRecord->pSCNode->ullSent;
  ullRecv = pCurRecord->pSCNode->ullRecv;
  iptUnlockNodes( pCurRecord->pTIF );

  __neFillCounter( _layerCtrlFromId( hwnd, ID_ST_SENTVAL ), ullSent );
  __neFillCounter( _layerCtrlFromId( hwnd, ID_ST_RECVVAL ), ullRecv );
}

// BOOL _neSetNode(HWND hwnd, PNODERECORD pRecord)
//
// Called from _nodesCursored()

static BOOL _neSetNode(HWND hwnd, PNODERECORD pRecord)
{
  static SCNODE	stNullSCNode = { 0 };
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  BOOL		fEmptyEditor = ( pRecord == NULL ) ||
                               ( pRecord->pSCNode == NULL );
  PSCNODE	pSCNode = fEmptyEditor ? &stNullSCNode : pRecord->pSCNode;

  pDlgData->pCurRecord = pRecord;
  LayerEnableControl( hwnd, ID_LAYER_NODEEDITOR, !fEmptyEditor );

  LayerSetDlgItemText( hwnd, ID_MLE_COMMENTS, pSCNode->pszComment );
  LayerCheckButton( hwnd, ID_CB_STOP, pSCNode->fStop ? 1 : 0 );
  LayerCheckButton( hwnd, ID_CB_IPHDR, pSCNode->fHdrIncl ? 1 : 0 );
  LayerSendItemMsg( hwnd, ID_CB_PROTO, LM_SELECTITEM,
                       MPFROMSHORT(
                         pSCNode->ulProto == STPROTO_TCP ? 0 :
                         pSCNode->ulProto == STPROTO_UDP ? 1 :
                         pSCNode->ulProto == (STPROTO_TCP | STPROTO_UDP) ? 2 :
                         pSCNode->ulProto == STPROTO_ICMP ? 3 : 4
                       ),
                       MPFROMLONG( TRUE ) );

  __neFillAddr( _layerCtrlFromId( hwnd, ID_LB_SRCADDR ), pSCNode->pSrcAddrLst );
  __neFillPorts( _layerCtrlFromId( hwnd, ID_EF_SRCPORT ), pSCNode->pSrcPortLst );

  __neFillAddr( _layerCtrlFromId( hwnd, ID_LB_DSTADDR ), pSCNode->pDstAddrLst );
  __neFillPorts( _layerCtrlFromId( hwnd, ID_EF_DSTPORT ), pSCNode->pDstPortLst );

  _neUpdateCounters( hwnd );
  _neChanged( hwnd, FALSE );

  return TRUE;
}


//		Filter's nodes container (tree view)
//		====================================

// Workaround with CM_MOVETREE which does not work for moves to last position
// and moves with same parent. We will uses CM_MOVETREE only for change parent
// and field ulOrder + CM_SORTRECORD to move on (new) siblings.
//
// __nodesMoveTree() - replacement for CM_MOVETREE,
// __nodesInsertRecord() - replacement for CM_INSERTRECORD.

static SHORT EXPENTRY __nodesCompare(PNODERECORD p1, PNODERECORD p2,
                                    PVOID pStorage)
{
  return p1->ulOrder - p2->ulOrder;
}

static BOOL __nodesMoveTree(HWND hwndNodes, PTREEMOVE pTreeMove)
{
  PNODERECORD	pNewNext;
  PNODERECORD	pOldNext = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pTreeMove->preccMove ),
                                   MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );
  PNODERECORD	pOldParent = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pTreeMove->preccMove ),
                                   MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );
  ULONG		ulOrder;

  if ( pTreeMove->flMoveSiblings )
  {
    debug( "pTreeMove->flMoveSiblings must be FALSE" );
    return FALSE;
  }

  if ( pTreeMove->pRecordOrder == (PRECORDCORE)CMA_FIRST )
  {
    ulOrder = 0;
    pNewNext = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                              MPFROMP( pTreeMove->preccNewParent ),
                              MPFROM2SHORT( CMA_FIRSTCHILD, CMA_ITEMORDER ) );
  }
  else if ( pTreeMove->pRecordOrder != (PRECORDCORE)CMA_LAST )
  {
    ulOrder = ((PNODERECORD)pTreeMove->pRecordOrder)->ulOrder + 1;
    pNewNext = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                              MPFROMP( pTreeMove->pRecordOrder ),
                              MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );
  }
  else
  {
    pNewNext = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                MPFROMP( pTreeMove->preccNewParent ),
                                MPFROM2SHORT( CMA_LASTCHILD, CMA_ITEMORDER ) );
    ulOrder = pNewNext == NULL ? 0 : ( pNewNext->ulOrder + 1 );
    pNewNext = NULL;
  }

  if ( pTreeMove->preccNewParent != (PRECORDCORE)pOldParent )
  {
    pTreeMove->pRecordOrder = (PRECORDCORE)CMA_FIRST;
    if ( !(BOOL)WinSendMsg( hwndNodes, CM_MOVETREE, MPFROMP( pTreeMove ), 0 ) )
    {
      debug( "CM_MOVETREE failed" );
      return FALSE;
    }
  }

  while( pOldNext != NULL )
  {
    pOldNext->ulOrder--;
    pOldNext = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                     MPFROMP( pOldNext ),
                                     MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );
  }

  ((PNODERECORD)pTreeMove->preccMove)->ulOrder = (ulOrder++);
  while( pNewNext != NULL )
  {
    pNewNext->ulOrder = (ulOrder++);
    pNewNext = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                     MPFROMP( pNewNext ),
                                     MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );
  }

  WinSendMsg( hwndNodes, CM_SORTRECORD, MPFROMP( __nodesCompare ), 0 );
  return TRUE;
}

static ULONG __nodesInsertRecord(HWND hwndNodes, PNODERECORD pRecords,
                               PRECORDINSERT pRecIns)
{
  ULONG		ulOrder;
  ULONG		ulResult;

  if ( pRecIns->pRecordOrder == (PRECORDCORE)CMA_FIRST )
    ulOrder = 0;
  else if ( pRecIns->pRecordOrder != (PRECORDCORE)CMA_END )
    ulOrder = ((PNODERECORD)pRecIns->pRecordOrder)->ulOrder + 1;
  else
  {
    PNODERECORD	 pRecord = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                MPFROMP( pRecIns->pRecordParent ),
                                MPFROM2SHORT( CMA_LASTCHILD, CMA_ITEMORDER ) );
    ulOrder = pRecord == NULL ? 0 : pRecord->ulOrder + 1;
  }

  ulResult = (ULONG)WinSendMsg( hwndNodes, CM_INSERTRECORD, MPFROMP( pRecords ),
                                MPFROMP( pRecIns ) );
  if ( ulResult != 0 )
  {
    while( pRecords != NULL )
    {
      pRecords->ulOrder = (ulOrder++);
      pRecords = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                      MPFROMP( pRecords ),
                                      MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );
    }
  }

  return ulResult;
}

// VOID _nodesAdd(PNODESADDDATA pAddData, PNODERECORD pParent,
//                PLINKSEQ plsSCNodes)
//
// Inserts filter nodes tree to the container. If SCNODE in list plsSCNodes
// with pUser == pAddData->pSCNodeUser found then pAddData->pSelRecord points
// to corresponding PNODERECORD.
// Called from _dialogInit()

typedef struct _NODESADDDATA {
  HWND		hwndNodes;	// in:  Container window handle.
  RECORDINSERT	stRecIns;	// in
  PVOID		pSCNodeUser;	// in:  Search for pUser of SCNODE.
  PNODERECORD	pSelRecord;	// out: Container's record for SCDATA with
                                //      pUser == pSCNodeUser.
} NODESADDDATA, *PNODESADDDATA;

static VOID _nodesAdd(PNODESADDDATA pAddData, PNODERECORD pParent,
                      PLINKSEQ plsSCNodes)
{
  PSCNODE	pSCNode;
  PNODERECORD	pRecords;
  PNODERECORD	pRecord;
  ULONG		ulOrder = 0;

  if ( lnkseqIsEmpty( plsSCNodes ) )
    return;

  // Allocate records.
  pRecords = (PNODERECORD)WinSendMsg( pAddData->hwndNodes, CM_ALLOCRECORD,
                     MPFROMLONG( sizeof(NODERECORD) - sizeof(MINIRECORDCORE) ),
                     MPFROMLONG( lnkseqGetCount( plsSCNodes ) ) );
  if ( pRecords == NULL )
  {
    debug( "CM_ALLOCRECORD returns 0" );
    return;
  }

  // Fill records.
  for( pSCNode = (PSCNODE)lnkseqGetFirst( plsSCNodes ), pRecord = pRecords;
       pSCNode != NULL; pSCNode = (PSCNODE)lnkseqGetNext( pSCNode ) )
  {
    if ( pAddData->pSCNodeUser != NULL &&
         pSCNode->pUser == pAddData->pSCNodeUser )
      pAddData->pSelRecord = pRecord;

    pRecord->stRecCore.cb = sizeof(MINIRECORDCORE);
    pRecord->stRecCore.flRecordAttr = CRA_EXPANDED;
    pRecord->stRecCore.pszIcon = pSCNode->pszName;
    pRecord->ulOrder = (ulOrder++);
    pRecord->pTIF = pParent->pTIF;
    pRecord->pSCNode = pSCNode;
    pRecord = (PNODERECORD)pRecord->stRecCore.preccNextRecord;
  }

  // Insert records.
  pAddData->stRecIns.cRecordsInsert = lnkseqGetCount( plsSCNodes );
  pAddData->stRecIns.pRecordParent = (PRECORDCORE)pParent;
  __nodesInsertRecord( pAddData->hwndNodes, pRecords, &pAddData->stRecIns );

  // Insert records for children nodes.
  for( pSCNode = (PSCNODE)lnkseqGetFirst( plsSCNodes ), pRecord = pRecords;
       pSCNode != NULL; pSCNode = (PSCNODE)lnkseqGetNext( pSCNode ) )
  {
    _nodesAdd( pAddData, pRecord, &pSCNode->lsNodes );
    pRecord = (PNODERECORD)pRecord->stRecCore.preccNextRecord;
  }
}

// BOOL _nodesCursored(HWND hwnd, HWND hwndNodes, PNODERECORD pRecord)
//
// Set enable/disable states for buttons, fill editor with given filter node.

static BOOL _nodesCursored(HWND hwnd, HWND hwndNodes, PNODERECORD pRecord)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PNODERECORD	pParent;
  BOOL		fIsTIF, fPrev, fNext;

  if ( pRecord == NULL )
    pRecord = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORDEMPHASIS,
                        MPFROMLONG( CMA_FIRST ), MPFROMSHORT( CRA_CURSORED ) );

  _neSetNode( hwnd, pRecord );

  if ( pRecord == NULL )
  {
    fIsTIF = TRUE;
    fPrev = FALSE;
    fNext = FALSE;
  }
  else
  {
    pParent = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD, MPFROMP(pRecord),
                                     MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );
    fIsTIF = pParent == NULL;
    fPrev = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD, MPFROMP(pRecord),
                            MPFROM2SHORT( CMA_PREV, CMA_ITEMORDER ) ) != NULL;
    fNext = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD, MPFROMP(pRecord),
                            MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) ) != NULL;
  }

  LayerEnableControl( hwnd, ID_PB_MOVE_UP, fPrev );
  LayerEnableControl( hwnd, ID_PB_MOVE_DOWN, fNext );
  LayerEnableControl( hwnd, ID_PB_NESTING, !fIsTIF );
  LayerEnableControl( hwnd, ID_PB_DELETE, pRecord != NULL );

  WinSendMsg( pDlgData->hwndMenuNew, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NEW_TIF, TRUE ),
              MPFROM2SHORT( MIA_DISABLED, 0 ) );
  WinSendMsg( pDlgData->hwndMenuNew, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NEW_BEFORE, TRUE ),
              MPFROM2SHORT( MIA_DISABLED, fIsTIF ? MIA_DISABLED : 0 ) );
  WinSendMsg( pDlgData->hwndMenuNew, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NEW_AFTER, TRUE ),
              MPFROM2SHORT( MIA_DISABLED, fIsTIF ? MIA_DISABLED : 0 ) );
  WinSendMsg( pDlgData->hwndMenuNew, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NEW_AT, TRUE ),
              MPFROM2SHORT( MIA_DISABLED, pRecord == NULL ? MIA_DISABLED : 0 ) );

  WinSendMsg( pDlgData->hwndMenuNesting, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NESTING_TOPREV, TRUE ),
              MPFROM2SHORT( MIA_DISABLED, !fPrev ? MIA_DISABLED : 0 ) );
  WinSendMsg( pDlgData->hwndMenuNesting, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NESTING_TONEXT, TRUE ),
              MPFROM2SHORT( MIA_DISABLED, !fNext ? MIA_DISABLED : 0 ) );
  WinSendMsg( pDlgData->hwndMenuNesting, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NESTING_UP, TRUE ),
              MPFROM2SHORT( MIA_DISABLED,
                fIsTIF || pParent->pSCNode == NULL ? MIA_DISABLED : 0 ) );
  return TRUE;
}

static VOID _nodesSetCursor(HWND hwnd, HWND hwndNodes, PNODERECORD pRecord)
{
  if ( _nodesCursored( hwnd, hwndNodes, pRecord ) )
  {
    PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );

    pDlgData->fJumpToNode = TRUE;
    WinSendMsg( hwndNodes, CM_SETRECORDEMPHASIS, MPFROMP( pRecord ),
                MPFROM2SHORT( 1, CRA_CURSORED ) );
    pDlgData->fJumpToNode = FALSE;
  }
}

// VOID _nodesEmphasis(HWND hwnd, PNOTIFYRECORDEMPHASIS pNotifyRecordEmphasis)
//
// Called from WM_CONTROL of main window.

static VOID _nodesEmphasis(HWND hwnd,
                           PNOTIFYRECORDEMPHASIS pNotifyRecordEmphasis)
{
  PDLGDATA    pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PRECORDCORE pRecord = (PRECORDCORE)WinSendMsg( pNotifyRecordEmphasis->hwndCnr,
                                                 CM_QUERYRECORDEMPHASIS,
                                                 MPFROMLONG( CMA_FIRST ),
                                                 MPFROMSHORT( CRA_CURSORED ) );

  if ( ( (pNotifyRecordEmphasis->fEmphasisMask & CRA_CURSORED) == 0 ) ||
       pDlgData->fJumpToNode || ( pRecord != pNotifyRecordEmphasis->pRecord ) )
    return;

  if ( !_neApplyNode( hwnd, TRUE ) )
  {
    pDlgData->fJumpToNode = TRUE;
    // Return to invalid node.
    WinSendMsg( pNotifyRecordEmphasis->hwndCnr, CM_SETRECORDEMPHASIS,
                MPFROMP( pDlgData->pCurRecord ), MPFROM2SHORT( 1, CRA_CURSORED ) );
    pDlgData->fJumpToNode = FALSE;
  }
  else
    _nodesCursored( hwnd, pNotifyRecordEmphasis->hwndCnr, (PNODERECORD)pRecord );
}

// BOOL _nodesReallocPSZ(HWND hwnd, PCNREDITDATA pEditData)
//
// Allocates buffer for new node name. Called from WM_CONTROL of main window.

static BOOL _nodesReallocPSZ(HWND hwnd, PCNREDITDATA pEditData)
{
  PNODERECORD	pRecord = (PNODERECORD)pEditData->pRecord;
  PSZ		pszName;

  if ( ( pRecord == NULL ) || ( pRecord->pSCNode == NULL ) ||
       ( pEditData->cbText <= 1 ) )
    return FALSE;

  pszName = debugMAlloc( pEditData->cbText );
  if ( pszName == NULL )
    return FALSE;

  pRecord->stRecCore.pszIcon = pszName;
  return TRUE;
}

// BOOL _nodesReallocPSZ(HWND hwnd, PCNREDITDATA pEditData)
//
// Applies new node name. Called from WM_CONTROL of main window.

static VOID _nodesEndEdit(HWND hwnd, PCNREDITDATA pEditData)
{
  PNODERECORD	pRecord = (PNODERECORD)pEditData->pRecord;
  PSZ		pszNewName = pRecord->stRecCore.pszIcon;
  PCHAR		pCh;
  ULONG		cbNewName;

  // Replace CRLFs with spaces.
  for( pCh = pszNewName; ; )
  {
    pCh = strchr( pCh, '\r' );
    if ( pCh == NULL )
      break;
    strcpy( pCh, &pCh[1] );
    *pCh = ' ';
  }
  // Skip leading spaces
  _STR_skipSpaces( pszNewName );
  // Remove trailing spaces
  pCh = strchr( pRecord->stRecCore.pszIcon, '\0' );
  while( ( pCh > pszNewName ) && isspace( *(pCh - 1) ) )
    pCh--;
  *pCh = '\0';

  cbNewName = strlen( pszNewName );

  // The name of node can not be empty /*, begin or end with a colon.*/
  if ( ( cbNewName == 0 ) /*|| ( pszNewName[0] == ':' ) ||
       ( pszNewName[cbNewName-1] == ':' )*/ )
  {
    __neMessageBox( hwnd, IDM_INVALID_NAME, MB_MOVEABLE | MB_OK | MB_ICONHAND );
  }
  else
  {
    // Allocate new (corrected) name name.
    pszNewName = debugStrDup( pszNewName );
    if ( pszNewName == NULL )
      debug( "Not enough memory" );
    else
    {
      // Free old name.
      debugFree( pRecord->pSCNode->pszName );
      // Set new name for node.
      pRecord->pSCNode->pszName = pszNewName;
    }
  }

  // Set filter's node name to the container's node name.
  debugFree( pRecord->stRecCore.pszIcon );
  pRecord->stRecCore.pszIcon = pRecord->pSCNode->pszName;
}

static VOID _nodesRename(HWND hwnd, PNODERECORD pRecord)
{
  HWND			hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
  CNREDITDATA		stEditData;

  if ( pRecord->pSCNode == NULL )
    return;

  stEditData.cb = sizeof(CNREDITDATA);
  stEditData.hwndCnr = hwndNodes;
  stEditData.pRecord = (PRECORDCORE)pRecord;
  stEditData.pFieldInfo = NULL;
  stEditData.ppszText = &pRecord->stRecCore.pszIcon;
  stEditData.cbText = strlen( pRecord->stRecCore.pszIcon );
  stEditData.id = ID_CONT_NODES;
  WinSendMsg( stEditData.hwndCnr, CM_OPENEDIT, MPFROMP( &stEditData ), 0 );
}

// VOID _nodesEnter(HWND hwnd, PNOTIFYRECORDENTER pNotifyRecordEnter)
//
// Open edit window for record when Enter key is pressed or select button is
// double-clicked while key Ctrl is held.

static VOID _nodesEnter(HWND hwnd, PNOTIFYRECORDENTER pNotifyRecordEnter)
{
  PNODERECORD	pRecord = (PNODERECORD)pNotifyRecordEnter->pRecord;

  if ( pRecord->pSCNode == NULL )
    return;

  if ( (WinGetKeyState( HWND_DESKTOP, VK_CTRL ) & 0x8000) != 0 )
    _nodesRename( hwnd, pRecord );
}

// VOID _nodesCtxMenu(HWND hwnd, PNODERECORD pRecord)
//
// Show context menu for record. Called from WM_CONTROL of main window.

static VOID _nodesCtxMenu(HWND hwnd, PNODERECORD pRecord)
{
  PDLGDATA		pDlgData = 
                          (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  HWND			hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
  RECTL			rect;
  QUERYRECORDRECT	stQuery;
  MPARAM		mpDisable;

  if ( pRecord == NULL )
    return;

  stQuery.cb = sizeof(QUERYRECORDRECT);
  stQuery.pRecord = (PRECORDCORE)pRecord;
  stQuery.fRightSplitWindow = FALSE;
  stQuery.fsExtent = CMA_TEXT;
  WinSendMsg( hwndNodes, CM_QUERYRECORDRECT, MPFROMP( &rect ),
              MPFROMP( &stQuery ) );

  mpDisable = pRecord->pSCNode == NULL ?
                MPFROM2SHORT( MIA_DISABLED, MIA_DISABLED ) :
                MPFROM2SHORT( MIA_DISABLED, 0 );

  WinSendMsg( pDlgData->hwndMenuNode, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NODE_TRACE, TRUE ), mpDisable );
  WinSendMsg( pDlgData->hwndMenuNode, MM_SETITEMATTR,
              MPFROM2SHORT( ID_MI_NODE_RENAME, TRUE ), mpDisable );

  pDlgData->pCtxMenuRecord = pRecord;
  WinPopupMenu( hwndNodes, hwnd, pDlgData->hwndMenuNode,
                ( rect.xLeft + rect.xRight ) / 2,
                ( rect.yBottom + rect.yTop ) / 2, 0,
                PU_MOUSEBUTTON1 | PU_KEYBOARD | PU_HCONSTRAIN | PU_VCONSTRAIN );
}

static VOID _nodesDelete(HWND hwnd, PNODERECORD	pRecord)
{
  HWND		hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
  PNODERECORD	pParent = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pRecord ),
                                   MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );
  CHAR		szTitle[64];
  CHAR		szBuf[128];
  ULONG		cbBuf;

  if ( pRecord == NULL )
    return;

  WinQueryWindowText( hwnd, sizeof(szTitle), &szTitle );
  cbBuf = putilLoadInsertStr( hDSModule, FALSE,
            ( pParent == NULL ? IDM_TIF_DELETE_CONF : IDM_NODE_DELETE_CONF ),
            1, &pRecord->stRecCore.pszIcon, sizeof(szBuf) - 1, &szBuf );
  if ( cbBuf != 0 )
  {
    szBuf[cbBuf] = '\0';
    if ( WinMessageBox( HWND_DESKTOP, hwnd, &szBuf, &szTitle, 0,
                        MB_MOVEABLE | MB_ICONQUESTION | MB_YESNO ) != MBID_YES )
    {
      WinSetFocus( HWND_DESKTOP, hwndNodes );
      return;
    }
  }

  _neChanged( hwnd, FALSE );

  if ( pRecord->pSCNode == NULL )
  {
    if ( pParent != NULL )
    {
      debugCP( "Interface's node must be root!" );
      return;
    }

    iptStop( pRecord->pTIF );
  }
  else
  {
    PLINKSEQ		plsNodes;

    if ( pParent == NULL )
    {
      debugCP( "Filter's node cannot be root!" );
      return;
    }

    plsNodes = pParent->pSCNode == NULL ?
                 iptGetTIFNodes( pParent->pTIF ) : &pParent->pSCNode->lsNodes;

    iptLockNodes( pRecord->pTIF );
    lnkseqRemove( plsNodes, pRecord->pSCNode );
    iptUnlockNodes( pRecord->pTIF );
    scFreeNode( pRecord->pSCNode );
  }

  WinSendMsg( hwndNodes, CM_REMOVERECORD, MPFROMP( &pRecord ),
              MPFROM2SHORT( 1, CMA_FREE | CMA_INVALIDATE ) );
  _nodesCursored( hwnd, hwndNodes, NULL );
  WinSetFocus( HWND_DESKTOP, hwndNodes );
}

static BOOL _nodesResetCounters(HWND hwnd, PNODERECORD pRecord)
{
  ULONG		ulAnswer;

  if ( pRecord == NULL )
    return FALSE;

  if ( pRecord->pSCNode == NULL )
    ulAnswer = __neMessageBox( hwnd, IDM_RESET_TIF_CONF,
                               MB_MOVEABLE | MB_OKCANCEL | MB_ICONQUESTION );
  else if ( lnkseqGetCount( &pRecord->pSCNode->lsNodes ) == 0 )
    ulAnswer = __neMessageBox( hwnd, IDM_RESET_CONF,
                               MB_MOVEABLE | MB_OKCANCEL | MB_ICONQUESTION );
  else
    ulAnswer = __neMessageBox( hwnd, IDM_RESET_BRANCH_CONF,
                               MB_MOVEABLE | MB_YESNOCANCEL | MB_ICONQUESTION );

  if ( ulAnswer == MBID_CANCEL )
    return FALSE;

  iptLockNodes( pRecord->pTIF );
  if ( pRecord->pSCNode != NULL )
  {
    // pRecord is filter node.
    pRecord->pSCNode->ullSent = 0;
    pRecord->pSCNode->ullRecv = 0;

    if ( ulAnswer == MBID_YES )
      scResetCounters( &pRecord->pSCNode->lsNodes );
  }
  else
  {
    // pRecord is TIF.
    scResetCounters( iptGetTIFNodes( pRecord->pTIF ) );
  }
  iptUnlockNodes( pRecord->pTIF );

  return TRUE;
}


//		Filter's control buttons and menus
//		==================================

// VOID _cmdPBNew(HWND hwnd)
//
// Button "New". Show menu.

static VOID _cmdPBNew(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  HWND		hwndBtn = _layerCtrlFromId( hwnd, ID_PB_NEW );
  POINTL	pt = { 0 };
  RECTL		rect;

  WinQueryWindowRect( hwndBtn, &rect );
  pt.y = rect.yTop - rect.yBottom;
  WinMapWindowPoints( hwndBtn, hwnd, &pt, 1 );
  WinPopupMenu( hwnd, hwnd, pDlgData->hwndMenuNew, pt.x, pt.y, 0,
                PU_MOUSEBUTTON1| PU_KEYBOARD | PU_HCONSTRAIN | PU_VCONSTRAIN );
}

// VOID _cmdNewTIF(HWND hwnd)
//
// Create a new interface. To request a new name, the user will see a small
// dialog box.

static VOID _cmdNewTIF(HWND hwnd)
{
  HWND		hwndDlg;
  HWND		hwndControl;
  HWND		hwndNodes;
  PNODERECORD	pRecord = NULL;
  ULONG		ulIdx;
  CHAR		szName[IFNAMSIZ] = "lan";

  if ( !_neApplyNode( hwnd, TRUE ) )
    return;

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwnd, WinDefDlgProc,
                                      hDSModule, ID_DLG_NEW_TIF, NULL );
  if ( hwndDlg == NULLHANDLE )
  {
    debug( "WinLoadDlg() fail" );
    return;
  }
  pctrlDlgCenterOwner( hwndDlg );

  hwndControl = WinWindowFromID( hwndDlg, ID_CB_INTERFACE );
  hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );

  // Insert strings lan0..lan7 to combobox for not created interfaces.
  for( ulIdx = 0; ulIdx < 8; ulIdx++ )
  {
    *((PULONG)&szName[3]) = ulIdx + '0';

    if ( iptGetTIF( &szName ) == NULL )
      WinSendMsg( hwndControl, LM_INSERTITEM, MPFROMSHORT( LIT_END ),
                  MPFROMP( &szName ) );
  }

  if ( WinProcessDlg( hwndDlg ) == ID_PB_CREATE )
  {
    // Button "Create" pressed.

    pRecord = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD, 0,
                               MPFROM2SHORT( CMA_FIRST, CMA_ITEMORDER ) );

    if ( pctrlQueryText( hwndControl, sizeof(szName), &szName ) != 0 )
    {
      strlwr( &szName );

      // Search entered name in the container's root records.
      while( ( pRecord != NULL ) &&
             ( strcmp( pRecord->stRecCore.pszIcon, &szName ) != 0 ) )
        pRecord = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                     (PRECORDCORE)pRecord,
                                     MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );

      if ( pRecord == NULL )
      {
        // Record with same name not found - create a new interface and record.
        PIPTIF		pTIF = iptStart( &szName );

        // Allocate records.
        pRecord = (PNODERECORD)WinSendMsg( hwndNodes, CM_ALLOCRECORD,
                     MPFROMLONG( sizeof(NODERECORD) - sizeof(MINIRECORDCORE) ),
                     MPFROMLONG( 1 ) );

        if ( pRecord != NULL )
        {
          RECORDINSERT	stRecIns;
          // Fill record.
          pRecord->stRecCore.cb = sizeof(MINIRECORDCORE);
          pRecord->stRecCore.flRecordAttr = CRA_EXPANDED | CRA_RECORDREADONLY;
          pRecord->stRecCore.pszIcon = iptGetTIFName( pTIF );
          pRecord->pTIF = pTIF;
          pRecord->pSCNode = NULL;
          // Insert record.
          stRecIns.cb = sizeof(RECORDINSERT);
          stRecIns.cRecordsInsert = 1;
          stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
          stRecIns.zOrder = (USHORT)CMA_TOP;
          stRecIns.pRecordParent = (PRECORDCORE)NULL;
          __nodesInsertRecord( hwndNodes, pRecord, &stRecIns );
        }
      }

      // Select found/created record.
      if ( pRecord != NULL )
        _nodesSetCursor( hwnd, hwndNodes, pRecord );
    }
  }

  WinDestroyWindow( hwndDlg );
  WinSetFocus( HWND_DESKTOP, hwndNodes );
}

// VOID _cmdNewNode(HWND hwnd, ULONG ulCmd)
//
// Inserts new node into an interface nodes list. 
// ulCmd: NEWNODE_xxxxxx

// NEWNODE_AT     - Create child for cursored filter node or interface.
#define NEWNODE_AT	0
// NEWNODE_AFTER  - Create new node after cursored node, at the same level.
#define NEWNODE_AFTER	1
// NEWNODE_BEFORE - Create new node before cursored node, at the same level.
#define NEWNODE_BEFORE	2

static VOID _cmdNewNode(HWND hwnd, ULONG ulCmd)
{
  HWND		hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
  // pRecord - Current record.
  PNODERECORD	pRecord = (PNODERECORD)WinSendMsg( hwndNodes,
                                                 CM_QUERYRECORDEMPHASIS,
                                                 MPFROMLONG( CMA_FIRST ),
                                                 MPFROMSHORT( CRA_CURSORED ) );
  PNODERECORD	pParent;
  PLINKSEQ	plsNodes;
  SCNODE	stNode = { 0 };
  PSCNODE	pNode;
  PIPTIF	pTIF;

  // Only child record creation allowed for the interface's (top-level) record.
  if ( ( ( pRecord == NULL ) || ( ( pRecord->pSCNode == NULL ) &&
                                  ( ulCmd != NEWNODE_AT ) ) ) ||
       !_neApplyNode( hwnd, TRUE ) )
    return;

  // pParent - Parent record for new record.
  pParent = ulCmd == NEWNODE_AT ? pRecord :
        (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD, MPFROMP( pRecord ),
                                 MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );

  pTIF = pParent->pTIF;
  plsNodes = pParent->pSCNode == NULL ?
               iptGetTIFNodes( pTIF ) : &pParent->pSCNode->lsNodes;

  // Create new filter node.
  lnkseqInit( &stNode.lsNodes );
  stNode.pszName = "New:Node";
  stNode.ulProto = STPROTO_TCP;
  pNode = scNewNode( &stNode );
  if ( pNode != NULL )
  {
    RECORDINSERT	stRecIns;

    // By default flag fStop is set - "Stop scan nodes at this level" checked.
    pNode->fStop = TRUE;
    // Insert new filter node.
    iptLockNodes( pTIF );
    if ( ulCmd == NEWNODE_AT )
    {
      lnkseqAdd( plsNodes, pNode );
      // Will be inserted as child of current record.
      stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
    }
    else
    {
      if ( ulCmd == NEWNODE_AFTER )
      {
        lnkseqAddAfter( plsNodes, pRecord->pSCNode, pNode );
        // Will be inserted after the current record.
        stRecIns.pRecordOrder = (PRECORDCORE)pRecord;
      }
      else
      {
        lnkseqAddBefore( plsNodes, pRecord->pSCNode, pNode );
        // Query previous record.
        stRecIns.pRecordOrder = (PRECORDCORE)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                     MPFROMP( pRecord ),
                                     MPFROM2SHORT( CMA_PREV, CMA_ITEMORDER ) );
        if ( stRecIns.pRecordOrder == NULL )
           // There is no previous record - insert first.
          stRecIns.pRecordOrder = (PRECORDCORE)CMA_FIRST;
      }
    }
    iptUnlockNodes( pTIF );

    stRecIns.cb = sizeof(RECORDINSERT);
    stRecIns.cRecordsInsert = 1;
    stRecIns.zOrder = (USHORT)CMA_TOP;
    stRecIns.pRecordParent = (PRECORDCORE)pParent;

    // Allocate record.
    pRecord = (PNODERECORD)WinSendMsg( hwndNodes, CM_ALLOCRECORD,
                     MPFROMLONG( sizeof(NODERECORD) - sizeof(MINIRECORDCORE) ),
                     MPFROMLONG( 1 ) );

    if ( pRecord != NULL )
    {
      // Fill record.
      pRecord->stRecCore.cb = sizeof(MINIRECORDCORE);
      pRecord->stRecCore.flRecordAttr = CRA_EXPANDED;
      pRecord->stRecCore.pszIcon = pNode->pszName;
      pRecord->pTIF = pTIF;
      pRecord->pSCNode = pNode;
      // Insert record.
      __nodesInsertRecord( hwndNodes, pRecord, &stRecIns );
      // Set cursor on new record.
      _nodesSetCursor( hwnd, hwndNodes, pRecord );
      // Open edit window for new record.
      _nodesRename( hwnd, pRecord );
    }
  }
}

// VOID _cmdPBNesting(HWND hwnd)
//
// Button "Nesting". Show menu.

static VOID _cmdPBNesting(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  HWND		hwndBtn = _layerCtrlFromId( hwnd, ID_PB_NESTING );
  POINTL	pt = { 0 };
  RECTL		rect;

  WinQueryWindowRect( hwndBtn, &rect );
  pt.y = rect.yTop - rect.yBottom;
  WinMapWindowPoints( hwndBtn, hwnd, &pt, 1 );
  WinPopupMenu( hwnd, hwnd, pDlgData->hwndMenuNesting, pt.x, pt.y, 0,
                PU_NONE | PU_MOUSEBUTTON1 | PU_KEYBOARD | PU_HCONSTRAIN |
                PU_VCONSTRAIN );
}

// VOID _cmdNestingNode(HWND hwnd, ULONG ulCmd)
//
// Move cursored node in z-order.
// ulCmd - NESTINGNODE_xxxxxxx

// NESTINGNODE_TONEXT  - Make cursored node next node's first child.
#define NESTINGNODE_TONEXT	CMA_NEXT
// NESTINGNODE_TOPREV  - Make cursored node previous node's last child.
#define NESTINGNODE_TOPREV	CMA_PREV
// NESTINGNODE_UPLEVEL - Move cursored node to one level up, after parent node.
#define NESTINGNODE_UPLEVEL	CMA_PARENT

static VOID _cmdNestingNode(HWND hwnd, ULONG ulCmd)
{
  HWND		hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
                // pRecord - Current record.
  PNODERECORD	pRecord =
        (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORDEMPHASIS,
                                 MPFROMLONG( CMA_FIRST ),
                                 MPFROMSHORT( CRA_CURSORED ) );
  PNODERECORD	pNewParent = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                     MPFROMP( pRecord ),
                                     MPFROM2SHORT( ulCmd, CMA_ITEMORDER ) );
  PNODERECORD	pParent = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pRecord ),
                                   MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );
  PLINKSEQ	plsNodes;
  TREEMOVE	stTreeMove;

  if ( ( pParent == NULL ) || ( pRecord->pSCNode == NULL ) ||
       !_neApplyNode( hwnd, TRUE ) )
    return;

  if ( ulCmd == NESTINGNODE_UPLEVEL )
    pNewParent = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pNewParent ),
                                   MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );

  if ( pNewParent == NULL )
    return;

  plsNodes = pParent->pSCNode != NULL ?
               &pParent->pSCNode->lsNodes : iptGetTIFNodes( pRecord->pTIF );
  lnkseqRemove( plsNodes, pRecord->pSCNode );
  plsNodes = pNewParent->pSCNode != NULL ?
               &pNewParent->pSCNode->lsNodes : iptGetTIFNodes( pNewParent->pTIF );

  iptLockNodes( pRecord->pTIF );
  if ( ulCmd == NESTINGNODE_TONEXT )
  {
    lnkseqAddFirst( plsNodes, pRecord->pSCNode );
    stTreeMove.pRecordOrder = (PRECORDCORE)CMA_FIRST;
  }
  else if ( ulCmd == NESTINGNODE_TOPREV )
  {
    lnkseqAdd( plsNodes, pRecord->pSCNode );
    stTreeMove.pRecordOrder = (PRECORDCORE)CMA_LAST;
  }
  else
  {
    lnkseqAddAfter( plsNodes, pParent->pSCNode, pRecord->pSCNode );
    stTreeMove.pRecordOrder = (PRECORDCORE)pParent;
  }
  iptUnlockNodes( pRecord->pTIF );

  stTreeMove.preccMove = (PRECORDCORE)pRecord;       /*  Record to be moved. */
  stTreeMove.preccNewParent = (PRECORDCORE)pNewParent;  /*  New parent for preccMove. */
  stTreeMove.flMoveSiblings = FALSE; 
  __nodesMoveTree( hwndNodes, &stTreeMove );

  // Set cursor on moved record.
  _nodesSetCursor( hwnd, hwndNodes, pRecord );

  WinSetFocus( HWND_DESKTOP, hwndNodes );
}

static VOID _cmdMove(HWND hwnd, BOOL fDown)
{
  HWND		hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
                // pRecord - Current record.
  PNODERECORD	pRecord =
        (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORDEMPHASIS,
                                 MPFROMLONG( CMA_FIRST ),
                                 MPFROMSHORT( CRA_CURSORED ) );
  PNODERECORD	pParent = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pRecord ),
                                   MPFROM2SHORT( CMA_PARENT, CMA_ITEMORDER ) );
  PNODERECORD	pBase;
  PLINKSEQ	plsNodes;
  TREEMOVE	stTreeMove;

  if ( fDown )
  {
    pBase = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pRecord ),
                                   MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );
  }
  else
  {
    pBase = pRecord;
    pRecord = (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORD,
                                   MPFROMP( pRecord ),
                                   MPFROM2SHORT( CMA_PREV, CMA_ITEMORDER ) );
  }

  if ( ( pBase != NULL ) && ( pRecord != NULL ) )
  {
    if ( !_neApplyNode( hwnd, TRUE ) )
      return;

    if ( pParent == NULL )
    {
      // Move interface.

      iptLockTIFs();
      plsNodes = iptGetTIFs();
      lnkseqRemove( plsNodes, pRecord->pTIF );
      lnkseqAddAfter( plsNodes, pBase->pTIF, pRecord->pTIF );
      iptUnlockTIFs();
    }
    else
    {
      // Move filter node.

      iptLockNodes( pRecord->pTIF );
      plsNodes = pParent->pSCNode != NULL ?
                   &pParent->pSCNode->lsNodes : iptGetTIFNodes( pRecord->pTIF );
      lnkseqRemove( plsNodes, pRecord->pSCNode );
      lnkseqAddAfter( plsNodes, pBase->pSCNode, pRecord->pSCNode );
      iptUnlockNodes( pRecord->pTIF );
    }

    stTreeMove.preccMove = (PRECORDCORE)pRecord;       /*  Record to be moved. */
    stTreeMove.preccNewParent = (PRECORDCORE)pParent;  /*  New parent for preccMove. */
    stTreeMove.pRecordOrder = (PRECORDCORE)pBase;      /*  Record order for siblings. */
    stTreeMove.flMoveSiblings = FALSE; 
    __nodesMoveTree( hwndNodes, &stTreeMove );
    _nodesCursored( hwnd, hwndNodes, NULL );
  }

  WinSetFocus( HWND_DESKTOP, hwndNodes );
}

// VOID _cmdDelete(HWND hwnd)
//
// Delete selected filter node. Called on button "Delete".

static VOID _cmdDelete(HWND hwnd)
{
  HWND		hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
  PNODERECORD	pRecord =
        (PNODERECORD)WinSendMsg( hwndNodes, CM_QUERYRECORDEMPHASIS,
                                 MPFROMLONG( CMA_FIRST ),
                                 MPFROMSHORT( CRA_CURSORED ) );

  _nodesDelete( hwnd, pRecord );
}

static VOID _cmdNodeMenuTrace(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );

  traceStart( WinQueryWindow( hwnd, QW_OWNER ),
              pDlgData->pCtxMenuRecord->pSCNode );
}

static VOID _cmdNodeMenuRename(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );

  _nodesRename( hwnd, pDlgData->pCtxMenuRecord );
}

// VOID _cmdNodeMenuDelete(HWND hwnd)
//
// Apply changes in current node and delete filter node under context menu.
// Called on item "Delete" from context menu.

static VOID _cmdNodeMenuDelete(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );

  if ( ( pDlgData->pCurRecord != NULL ) &&
       ( pDlgData->pCurRecord != pDlgData->pCtxMenuRecord ) &&
       !_neApplyNode( hwnd, TRUE ) )
    return;

  _nodesDelete( hwnd, pDlgData->pCtxMenuRecord );
}

static VOID _cmdNodeMenuReset(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );

  _nodesResetCounters( hwnd, pDlgData->pCtxMenuRecord );
}


//		Address list box control for node editor
//		========================================

// MRESULT EXPENTRY wpfAddrLBEF(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
// Subclass window procedure for address entry field.

static MRESULT EXPENTRY wpfAddrLBEF(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  if ( msg == WM_CHAR )
  {
    USHORT		usFlags = SHORT1FROMMP( mp1 );
    USHORT		usVKey = SHORT2FROMMP( mp2 );

    if ( (usFlags & (KC_VIRTUALKEY | KC_KEYUP)) == KC_VIRTUALKEY )
    {
      if ( usVKey == VK_ESC )
      {
        // We use EN_MEMERROR code to notifiy owner - text entry is canceled.
        WinSendMsg( WinQueryWindow( hwnd, QW_OWNER ), WM_CONTROL,
                    MPFROM2SHORT( WinQueryWindowUShort( hwnd, QWS_ID ),
                                  EN_MEMERROR ),
                    MPFROMLONG( hwnd ) );
        return (MRESULT)TRUE;
      }
      else if ( usVKey == VK_ENTER || usVKey == VK_NEWLINE ||
                usVKey == VK_UP || usVKey == VK_DOWN )
      {
        WinSetFocus( HWND_DESKTOP, WinQueryWindow( hwnd, QW_PARENT ) );
        return (MRESULT)TRUE;
      }
    }
  }

  return pwpfEFOrg( hwnd, msg, mp1, mp2 );
}

// _addrLBEnter(HWND hwnd, ULONG ulCtrlId, HWND hwndLB)
//
// Called by address list box owner on LN_ENTER control code.
// Enter/double click in address list box. Create enterfield for selected item.
// hwnd - dialog window.
// ulCtrlId - address list box id.
// hwndLB - address list box window.

static VOID _addrLBEnter(HWND hwnd, ULONG ulCtrlId, HWND hwndLB)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PRECTL	pRect;
  SHORT		sIndex;
  CHAR		szBuf[35];
  ENTRYFDATA	stEFData;

  if ( ulCtrlId == ID_LB_SRCADDR )
    pRect = &pDlgData->rectSrcAddrSel;
  else if ( ulCtrlId == ID_LB_DSTADDR )
    pRect = &pDlgData->rectDstAddrSel;
  else
    return;

  sIndex = (SHORT)WinSendMsg( hwndLB, LM_QUERYSELECTION,
                              MPFROMSHORT( LIT_CURSOR ), 0 );
  if ( sIndex == LIT_NONE )
    sIndex = 0;
  WinSendMsg( hwndLB, LM_QUERYITEMTEXT,
              MPFROM2SHORT( sIndex, sizeof(szBuf) ), MPFROMP( &szBuf ) );

  stEFData.cb = sizeof(ENTRYFDATA);
  stEFData.cchEditLimit = sizeof(szBuf) - 1;
  stEFData.ichMinSel = sizeof(szBuf) - 1;
  stEFData.ichMaxSel = sizeof(szBuf) - 1;

  pDlgData->hwndAddrEF = WinCreateWindow( hwndLB, WC_ENTRYFIELD, &szBuf,
                                 WS_VISIBLE | ES_AUTOSCROLL | ES_MARGIN,
                                 pRect->xLeft + 1, pRect->yBottom,
                                 pRect->xRight - pRect->xLeft,
                                 pRect->yTop - pRect->yBottom,
                                 hwnd, HWND_TOP, ID_EF_ADDR, &stEFData, NULL );
  pwpfEFOrg = WinSubclassWindow( pDlgData->hwndAddrEF, wpfAddrLBEF );
  WinSetFocus( HWND_DESKTOP, pDlgData->hwndAddrEF );
}

// VOID _addrLBEFDone(HWND hwnd, BOOL fStore)
//
// End text input. When fStore is TRUE text stored to address list box.
// hwnd - dialog window.

static VOID _addrLBEFDone(HWND hwnd, BOOL fStore)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  HWND		hwndAddrEF = pDlgData->hwndAddrEF;
  HWND		hwndLB;
  CHAR		szBuf[35];
  ULONG		cbBuf;
  SHORT		sCount;
  SHORT		sIndex;
  SCADDR	stSCAddr;

  if ( hwndAddrEF == NULLHANDLE )	// Focus lost during destroying EF (1).
    return;
  hwndLB = WinQueryWindow( hwndAddrEF, QW_PARENT );

  sCount = (SHORT)WinSendMsg( hwndLB, LM_QUERYITEMCOUNT, 0, 0 );
  sIndex = (SHORT)WinSendMsg( hwndLB, LM_QUERYSELECTION,
                              MPFROMSHORT( LIT_FIRST ), 0 );

  cbBuf = WinQueryWindowText( hwndAddrEF, sizeof(szBuf), &szBuf );
  if ( fStore && ( cbBuf != 0 ) && !scStrToAddr( &szBuf, &stSCAddr ) )
  {
    // Invalid value entered.
    fStore = FALSE;
  }

  if ( fStore )
  {
    if ( sIndex == LIT_NONE )
      sIndex = 0;

    if ( cbBuf != 0 )
    {
      // Some text is entered.

      if ( sCount != 0 )
        WinSendMsg( hwndLB, LM_SETITEMTEXT,
                    MPFROMSHORT( sIndex ), MPFROMP( &szBuf ) );
      else
      {
        sIndex = (SHORT)WinSendMsg( hwndLB, LM_INSERTITEM,
                             MPFROMSHORT( LIT_END ), MPFROMP( &szBuf ) );
        if ( sIndex != LIT_MEMERROR && sIndex != LIT_ERROR )
          sCount++;
      }
    }
    else
    {
      // Text is not entered or deleted.

      sCount = (SHORT)WinSendMsg( hwndLB, LM_DELETEITEM,
                                  MPFROMSHORT( sIndex ), 0 );
      if ( sCount != 0 )
        WinSendMsg( hwndLB, LM_SELECTITEM,
                    MPFROMSHORT( sIndex == sCount ? sCount - 1 : sIndex ),
                    MPFROMSHORT( 1 ) );
    }

    if ( ( sCount == 0 ) ||
         ( (SHORT)WinSendMsg( hwndLB, LM_QUERYITEMTEXT,
                              MPFROM2SHORT( (sCount - 1), sizeof(szBuf) ),
                              MPFROMP( &szBuf ) ) != 0 ) )
      if ( (SHORT)WinSendMsg( hwndLB, LM_INSERTITEM,
                        MPFROMSHORT( LIT_END ), MPFROMP( "" ) ) == 0 )
        WinSendMsg( hwndLB, LM_SELECTITEM, MPFROMSHORT( 0 ), MPFROMSHORT( 1 ) );

    _neChanged( hwnd, TRUE );
  }
  else if ( ( sIndex < (sCount - 1) ) &&
            ( (SHORT)WinSendMsg( hwndLB, LM_QUERYITEMTEXT,
                                 MPFROM2SHORT( sIndex, sizeof(szBuf) ),
                                 MPFROMP( &szBuf ) ) == 0 ) )
  {
    // Text entry canceled for not last empty i.e. new field. Remove it.
    WinSendMsg( hwndLB, LM_DELETEITEM, MPFROMSHORT( sIndex ), 0 );
    WinSendMsg( hwndLB, LM_SELECTITEM, MPFROMSHORT( sIndex ), MPFROMSHORT(1) );
  }

  pDlgData->hwndAddrEF = NULLHANDLE;	// Avoid store value.
  WinDestroyWindow( hwndAddrEF );	// (1)
  WinSetFocus( HWND_DESKTOP, hwndLB );
}

// MRESULT EXPENTRY wpfAddrLB(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
//
// Subclass window procedure for address list box.
// Adds actions for buttons: <INSERT> - insert new item,
//                           Gray +/- - move selected item up and down in list.

static MRESULT EXPENTRY wpfAddrLB(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  if ( msg == WM_CHAR )
  {
    USHORT		usFlags = SHORT1FROMMP( mp1 );

    if ( (usFlags & (KC_VIRTUALKEY | KC_KEYUP)) == KC_VIRTUALKEY )
    {
      USHORT		usVKey = SHORT2FROMMP( mp2 );

      if ( usVKey == VK_INSERT )
      {
        SHORT	sIndex = (SHORT)WinSendMsg( hwnd, LM_QUERYSELECTION,
                                            MPFROMSHORT( LIT_CURSOR ), 0 );
        if ( sIndex == LIT_NONE )
          return (MRESULT)TRUE;
        // Insert empty record.
        WinSendMsg( hwnd, LM_INSERTITEM, MPFROMSHORT( sIndex ), MPFROMP( "" ) );
        WinSendMsg( hwnd, LM_SELECTITEM, MPFROMSHORT( sIndex ), MPFROMSHORT(1) );
        // Send LN_ENTER control code to owner.
        WinSendMsg( WinQueryWindow( hwnd, QW_OWNER ), WM_CONTROL,
                    MPFROM2SHORT( WinQueryWindowUShort( hwnd, QWS_ID ),
                                  LN_ENTER ),
                    MPFROMLONG( hwnd ) );
        return (MRESULT)TRUE;
      }
    }

    if ( (usFlags & (KC_SCANCODE | KC_KEYUP)) == KC_SCANCODE )
    do
    {
      CHAR		chScan = SHORT2FROMMP( mp1 ) >> 8;
      SHORT		sIndex, sNewIndex, sDirection;
      CHAR		szBuf1[35];
      CHAR		szBuf2[35];

      if ( chScan == 74 ) // Num pad '-'
        sDirection = -1;
      else if ( chScan == 78 ) // Num pad '+'
        sDirection = 1;
      else
        break;

      sIndex = (SHORT)WinSendMsg( hwnd, LM_QUERYSELECTION,
                                  MPFROMSHORT( LIT_CURSOR ), 0 );
      sNewIndex = sIndex + sDirection;
      if ( ( sNewIndex < 0 ) ||
           ( sNewIndex >=
             ( (SHORT)WinSendMsg( hwnd, LM_QUERYITEMCOUNT, 0, 0 ) - 1 ) ) ||
           ( (SHORT)WinSendMsg( hwnd, LM_QUERYITEMTEXT,
                                MPFROM2SHORT( sIndex, sizeof( szBuf1 ) ),
                                MPFROMP( &szBuf1 ) ) == 0 ) ||
           ( (SHORT)WinSendMsg( hwnd, LM_QUERYITEMTEXT,
                                MPFROM2SHORT( sNewIndex, sizeof( szBuf2 ) ),
                                MPFROMP( &szBuf2 ) ) == 0 ) )
        break;

      WinSendMsg( hwnd, LM_SETITEMTEXT,
                  MPFROMSHORT( sIndex ), MPFROMP( &szBuf2 ) );
      WinSendMsg( hwnd, LM_SETITEMTEXT,
                  MPFROMSHORT( sNewIndex ), MPFROMP( &szBuf1 ) );
      WinSendMsg( hwnd, LM_SELECTITEM, MPFROMSHORT(sNewIndex), MPFROMSHORT(1) );

      _neChanged( WinQueryWindow( hwnd, QW_OWNER ), TRUE );
    }
    while( FALSE );
  }

  return pwpfAddrLBPrev( hwnd, msg, mp1, mp2 );
}

// MRESULT _wmMeasureitem(HWND hwnd, ULONG ulCrtlId)
//
// Called by address list box owner on window message WM_MEASUREITEM.
// Returns height for an item in list box control.

static MRESULT _addrLBMeasureItem(HWND hwnd, ULONG ulCrtlId)
{
  FONTMETRICS	stFontMetrics;
  ULONG		ulCY;
  HPS		hps = WinGetPS( WinWindowFromID( hwnd, ulCrtlId ) );

  GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &stFontMetrics );
  WinReleasePS( hps );
  ulCY = stFontMetrics.lEmHeight - stFontMetrics.lInternalLeading;

  return MRFROM2SHORT( ulCY + (ulCY >> 1), 0 ); // 1.5 character's height
}

// MRESULT _addrLBDrawItem(HWND hwnd, ULONG ulCtrlId, POWNERITEM pstOI)
//
// Called by address list box owner on window message WM_DRAWITEM.
// We use this message for detect rectangle of selected item.

static MRESULT _addrLBDrawItem(HWND hwnd, ULONG ulCtrlId, POWNERITEM pstOI)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  PRECTL	pRect;

  if ( ulCtrlId == ID_LB_SRCADDR )
    pRect = &pDlgData->rectSrcAddrSel;
  else if ( ulCtrlId == ID_LB_DSTADDR )
    pRect = &pDlgData->rectDstAddrSel;
  else
    return (MRESULT)FALSE;

  if ( pstOI->fsState || WinIsRectEmpty( hab, pRect ) )
  {
    // Store rectangle of selected item.
    if ( ulCtrlId == ID_LB_SRCADDR )
      *pRect = pstOI->rclItem;
    else if ( ulCtrlId == ID_LB_DSTADDR )
      *pRect = pstOI->rclItem;
  }

  if ( pstOI->fsState != pstOI->fsStateOld )
  {
    pstOI->fsState = pstOI->fsStateOld;
  }
  else if ( pstOI->fsState != 0 )
  {
    pstOI->fsState = 0;
    pstOI->fsStateOld = 0;
  }

  return (MRESULT)FALSE; // Item was not painted, ask list box control to do it.
}


//		Dialog main window
//		==================

static VOID _dialogInit(HWND hwnd, PVOID pSCNodeUser)
{
  static PSZ	apszProtocols[] = { "TCP", "UDP", "TCP, UDP", "ICMP", "Any" };
  PDLGDATA	pDlgData = debugCAlloc( 1, sizeof(DLGDATA) );
  LONG		lColor = SYSCLR_DIALOGBACKGROUND;
  HWND		hwndControl;
  CNRINFO	stCnrInf = { 0 };
  PLINKSEQ	plsTIFs;
  PIPTIF	pTIF;
  ULONG		ulIdx;
  NODESADDDATA	stAddData;
  ULONG		ulWidth;
  ULONG		ulHeight;

  if ( pDlgData == NULL )
    return;

  ulWidth = piniReadULong( hDSModule, "filterWidth", 0 );
  ulHeight = piniReadULong( hDSModule, "filterHeight", 0 );
  if ( ( ulWidth > 300 ) && ( ulHeight > 200 ) )
    WinSetWindowPos( hwnd, 0, 0, 0, ulWidth, ulHeight, SWP_SIZE );

  // Menu for button "New...".
  pDlgData->hwndMenuNew = WinLoadMenu( HWND_DESKTOP, hDSModule, ID_MENU_NEW );
  // Menu for button "Nesting...".
  pDlgData->hwndMenuNesting = WinLoadMenu( HWND_DESKTOP, hDSModule, ID_MENU_NESTING );
  // Menu for nodes.
  pDlgData->hwndMenuNode = WinLoadMenu( HWND_DESKTOP, hDSModule, ID_MENU_NODE );

  WinSetWindowULong( hwnd, QWL_USER, (ULONG)pDlgData );

  // Disabled color for prts entry fields
  WinSetPresParam( _layerCtrlFromId( hwnd, ID_EF_SRCPORT ),
                   PP_DISABLEDFOREGROUNDCOLORINDEX, sizeof(LONG), &lColor );
  WinSetPresParam( _layerCtrlFromId( hwnd, ID_EF_DSTPORT ),
                   PP_DISABLEDFOREGROUNDCOLORINDEX, sizeof(LONG), &lColor );

  // Tune container.

  stAddData.hwndNodes = _layerCtrlFromId( hwnd, ID_CONT_NODES );
  stAddData.pSCNodeUser = pSCNodeUser;
  stAddData.pSelRecord = NULL;

  stCnrInf.cb = sizeof(CNRINFO);
  stCnrInf.flWindowAttr = CV_TREE | CV_TEXT | CA_TREELINE | CA_TITLEREADONLY |
                          CA_TITLESEPARATOR | CA_TITLELEFT;
  stCnrInf.slTreeBitmapOrIcon.cx = 14;
  stCnrInf.slTreeBitmapOrIcon.cy = 14;
  WinSendMsg( stAddData.hwndNodes, CM_SETCNRINFO, &stCnrInf,
              MPFROMLONG( CMA_PFIELDINFOLAST | CMA_XVERTSPLITBAR
                          | CMA_FLWINDOWATTR | CMA_SLTREEBITMAPORICON ) );
 
  // Fill container.

  plsTIFs = iptGetTIFs();
  if ( !lnkseqIsEmpty( plsTIFs ) )
  {
    PNODERECORD		pRecords, pRecord;

    // Allocate records.
    pRecords = (PNODERECORD)WinSendMsg( stAddData.hwndNodes, CM_ALLOCRECORD,
                       MPFROMLONG( sizeof(NODERECORD) - sizeof(MINIRECORDCORE) ),
                       MPFROMLONG( lnkseqGetCount( plsTIFs ) ) );
    if ( pRecords == NULL )
      debug( "CM_ALLOCRECORD returns 0" );
    else
    {
      // Fill records.
      pRecord = pRecords;
      for( ulIdx = 0, pTIF = (PIPTIF)lnkseqGetFirst( plsTIFs ); pTIF != NULL;
           pTIF = (PIPTIF)lnkseqGetNext( pTIF ), ulIdx++ )
      {
        pRecord->stRecCore.cb = sizeof(MINIRECORDCORE);
        pRecord->stRecCore.flRecordAttr = CRA_EXPANDED | CRA_RECORDREADONLY;
        pRecord->stRecCore.pszIcon = iptGetTIFName( pTIF );
        pRecord->ulOrder = ulIdx;
        pRecord->pSCNode = NULL;
        pRecord->pTIF = pTIF;
        pRecord = (PNODERECORD)pRecord->stRecCore.preccNextRecord;
      }

      // Insert records.
      stAddData.stRecIns.cb = sizeof(RECORDINSERT);
      stAddData.stRecIns.cRecordsInsert = lnkseqGetCount( plsTIFs );
      stAddData.stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
      stAddData.stRecIns.zOrder = (USHORT)CMA_TOP;
      stAddData.stRecIns.pRecordParent = (PRECORDCORE)NULL;
      __nodesInsertRecord( stAddData.hwndNodes, pRecords, &stAddData.stRecIns );

      // Insert records for nodes to interface's root record.
      pRecord = pRecords;
      for( pTIF = (PIPTIF)lnkseqGetFirst( plsTIFs ); pTIF != NULL;
           pTIF = (PIPTIF)lnkseqGetNext( pTIF ) )
      {
        _nodesAdd( &stAddData, pRecord, iptGetTIFNodes( pTIF ) );
        pRecord = (PNODERECORD)pRecord->stRecCore.preccNextRecord;
      }
    }
  }

  // Set text length limit for comment.
  hwndControl = _layerCtrlFromId( hwnd, ID_MLE_COMMENTS );
  WinSendMsg( hwndControl, MLM_SETTEXTLIMIT, MPFROMLONG( 511 ), 0 );

  // Fill combo box "Protocols".
  hwndControl = _layerCtrlFromId( hwnd, ID_CB_PROTO );
  for( ulIdx = 0; ulIdx < ARRAYSIZE( apszProtocols ); ulIdx++ )
    WinSendMsg( hwndControl, LM_INSERTITEM,
                MPFROMSHORT( LIT_END ), MPFROMP( apszProtocols[ulIdx] ) );

  // Subclass address lists and add empty records.

  hwndControl = _layerCtrlFromId( hwnd, ID_LB_SRCADDR );
  pwpfAddrLBPrev = WinSubclassWindow( hwndControl, wpfAddrLB );
  WinSendMsg( hwndControl, LM_INSERTITEM, MPFROMLONG(LIT_END), MPFROMP( "" ) );

  hwndControl = _layerCtrlFromId( hwnd, ID_LB_DSTADDR );
  pwpfAddrLBPrev = WinSubclassWindow( hwndControl, wpfAddrLB );
  WinSendMsg( hwndControl, LM_INSERTITEM, MPFROMLONG(LIT_END), MPFROMP( "" ) );

  _nodesSetCursor( hwnd, stAddData.hwndNodes, stAddData.pSelRecord );

  // Timer for updates counters in node editor.
  pDlgData->ulCountersTimer = WinStartTimer( hab, hwnd, 0, 1000 );
}

static VOID _dialogDestroy(HWND hwnd)
{
  PDLGDATA	pDlgData = (PDLGDATA)WinQueryWindowULong( hwnd, QWL_USER );
  SWP		swp;

  WinStopTimer( hab, hwnd, pDlgData->ulCountersTimer );

  if ( pDlgData != NULL )
  {
    WinDestroyWindow( pDlgData->hwndMenuNew );
    WinDestroyWindow( pDlgData->hwndMenuNesting );
    WinDestroyWindow( pDlgData->hwndMenuNode );
    debugFree( pDlgData );
  }

  WinQueryWindowPos( hwnd, &swp );
  piniWriteULong( hDSModule, "filterWidth", swp.cx );
  piniWriteULong( hDSModule, "filterHeight", swp.cy );
}

// VOID _dialogSize(HWND hwnd, PSWP pSWP)
//
// Called on WM_WINDOWPOSCHANGED from the main window.
// Positions and resizes the layers when owner resized.

static VOID _dialogSize(HWND hwnd, PSWP pSWP)
{
  HWND		hwndMainLayer = WinWindowFromID( hwnd, ID_LAYER_MAIN );
  HWND		hwndRightLayer = WinWindowFromID( hwndMainLayer, ID_LAYER_RIGHT );
  HWND		hwndNodeEditorLayer = WinWindowFromID( hwndRightLayer, ID_LAYER_NODEEDITOR );
  HWND		hwndBottomLayer = WinWindowFromID( hwndMainLayer, ID_LAYER_BOTTOM );
  HWND		hwndNodes = WinWindowFromID( hwndMainLayer, ID_CONT_NODES );
  ULONG		ulMainCX;
  ULONG		ulMainCY;
  ULONG		ulRightLayerLeft;
  ULONG		ulRightLayerCY;
  ULONG		ulBottomLayerTop;
  SWP		swp;
  RECTL		rectMainLayer;

  // Main layer.
  _layerClientRect( hwnd, &rectMainLayer );
  ulMainCX = rectMainLayer.xRight - rectMainLayer.xLeft;
  ulMainCY = rectMainLayer.yTop - rectMainLayer.yBottom;
  WinSetWindowPos( hwndMainLayer, HWND_BOTTOM,
                   rectMainLayer.xLeft, rectMainLayer.yBottom,
                   ulMainCX, ulMainCY, SWP_MOVE | SWP_SIZE );

  // Bottom layer.
  WinQueryWindowPos( hwndBottomLayer, &swp );
  ulBottomLayerTop = swp.cy;
  WinSetWindowPos( hwndBottomLayer, HWND_TOP, 0, 0, ulMainCX, swp.cy,
                   SWP_SIZE | SWP_MOVE );

  // Right layer.
  WinQueryWindowPos( hwndRightLayer, &swp );
  ulRightLayerCY = ulMainCY - ulBottomLayerTop;
  ulRightLayerLeft = ulMainCX - swp.cx;
  WinSetWindowPos( hwndRightLayer, HWND_TOP,
                   ulRightLayerLeft, ulBottomLayerTop, swp.cx,
                   ulRightLayerCY, SWP_MOVE | SWP_SIZE );

  // Node editor layer.
  WinQueryWindowPos( hwndNodeEditorLayer, &swp );
  WinSetWindowPos( hwndNodeEditorLayer, HWND_TOP,
                   swp.x, ulRightLayerCY - swp.cy, 0, 0, SWP_MOVE );

  // Container.
  WinSetWindowPos( hwndNodes, 0, 0, ulBottomLayerTop,
                   ulRightLayerLeft, ulRightLayerCY, SWP_SIZE | SWP_MOVE );

  WinUpdateWindow( hwndMainLayer );
}

static MRESULT EXPENTRY wpfDialog(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      {
         PFILTERCDATA	pCData = (PFILTERCDATA)mp2;

        _layerSetLayers( hwnd );
        _dialogInit( hwnd, pCData == NULL ? NULL : pCData->pSCNodeUser );
      }
      break;

    case WM_DESTROY:
      _dialogDestroy( hwnd );
      break;

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
        _dialogSize( hwnd, (PSWP)mp1 );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case ID_PB_NEW:
          _cmdPBNew( hwnd );
          break;

        case ID_MI_NEW_TIF:
          _cmdNewTIF( hwnd );
          break;

        case ID_MI_NEW_AT:
          _cmdNewNode( hwnd, NEWNODE_AT );
          break;

        case ID_MI_NEW_AFTER:
          _cmdNewNode( hwnd, NEWNODE_AFTER );
          break;

        case ID_MI_NEW_BEFORE:
          _cmdNewNode( hwnd, NEWNODE_BEFORE );
          break;

        case ID_PB_NESTING:
          _cmdPBNesting( hwnd );
          break;

        case ID_MI_NESTING_TOPREV:
          _cmdNestingNode( hwnd, NESTINGNODE_TOPREV );
          break;

        case ID_MI_NESTING_TONEXT:
          _cmdNestingNode( hwnd, NESTINGNODE_TONEXT );
          break;

        case ID_MI_NESTING_UP:
          _cmdNestingNode( hwnd, NESTINGNODE_UPLEVEL );
          break;

        case ID_PB_MOVE_UP:
          _cmdMove( hwnd, FALSE );
          break;

        case ID_PB_MOVE_DOWN:
          _cmdMove( hwnd, TRUE );
          break;

        case ID_PB_DELETE:
          _cmdDelete( hwnd );
          break;

        case ID_PB_HELP:
          phelpShow( hDSModule, 20804 );
          break;

        case ID_PB_RESET:
          _neResetCounters( hwnd );
          break;

        case ID_PB_APPLY:
          _neApplyNode( hwnd, FALSE );
          break;

        case ID_MI_NODE_TRACE:
          _cmdNodeMenuTrace( hwnd );
          break;

        case ID_MI_NODE_RENAME:
          _cmdNodeMenuRename( hwnd );
          break;

        case ID_MI_NODE_DELETE:
          _cmdNodeMenuDelete( hwnd );
          break;

        case ID_MI_NODE_RESET:
          _cmdNodeMenuReset( hwnd );
          break;
      }
      return (MRESULT)FALSE;

    case WM_MEASUREITEM:
      return _addrLBMeasureItem( hwnd, SHORT1FROMMP( mp1 ) );

    case WM_DRAWITEM:
      return _addrLBDrawItem( hwnd, SHORT1FROMMP( mp1 ),
                          (POWNERITEM)PVOIDFROMMP( mp2 ) );

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case ID_CONT_NODES:

          switch( SHORT2FROMMP( mp1 ) )
          {
            case CN_EMPHASIS:
              _nodesEmphasis( hwnd, (PNOTIFYRECORDEMPHASIS)mp2 );
              return (MRESULT)FALSE;

            case CN_REALLOCPSZ:
              return (MRESULT)_nodesReallocPSZ( hwnd, (PCNREDITDATA)mp2 );

            case CN_ENDEDIT:
              _nodesEndEdit( hwnd, (PCNREDITDATA)mp2 );
              return (MRESULT)FALSE;

            case CN_ENTER:
              _nodesEnter( hwnd, (PNOTIFYRECORDENTER)mp2 );
              return (MRESULT)FALSE;

            case CN_CONTEXTMENU:
              _nodesCtxMenu( hwnd, (PNODERECORD)mp2 );
              return (MRESULT)FALSE;
          }
          return (MRESULT)FALSE;

        case ID_MLE_COMMENTS:
          if ( SHORT2FROMMP( mp1 ) == MLN_CHANGE )
            _neChanged( hwnd, TRUE );
          return (MRESULT)FALSE;

        case ID_CB_STOP:
        case ID_CB_IPHDR:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED ||
               SHORT2FROMMP( mp1 ) == BN_DBLCLICKED )
            _neChanged( hwnd, TRUE );
          break;

        case ID_CB_PROTO:
          if ( SHORT2FROMMP( mp1 ) == CBN_EFCHANGE )
            _neProtoChanged( hwnd, SHORT1FROMMP( mp1 ), HWNDFROMMP( mp2 ) );
          return (MRESULT)FALSE;

        case ID_LB_SRCADDR:
        case ID_LB_DSTADDR:
          if ( SHORT2FROMMP( mp1 ) == LN_ENTER )
            _addrLBEnter( hwnd, SHORT1FROMMP( mp1 ), HWNDFROMMP( mp2 ) );
          return (MRESULT)FALSE;

        case ID_EF_ADDR:
          if ( SHORT2FROMMP( mp1 ) == EN_KILLFOCUS )
            _addrLBEFDone( hwnd, TRUE );
          else if ( SHORT2FROMMP( mp1 ) == EN_MEMERROR )
            // Fake message from wpfAddrLBEF(), text entry is canceled.
            _addrLBEFDone( hwnd, FALSE );
          return (MRESULT)FALSE;

        case ID_EF_SRCPORT:
        case ID_EF_DSTPORT:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            _neChanged( hwnd, TRUE );
          break;
      }
      break;

    case WM_TIMER:
      if ( SHORT1FROMMP(mp1) != 0 )
        break;
      _neUpdateCounters( hwnd );
      return (MRESULT)FALSE;

    case WM_CLOSE:
      if ( !_neApplyNode( hwnd, TRUE ) )
        return (MRESULT)FALSE;
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


VOID showFilter(HWND hwndOwner, PVOID pSCNodeUser)
{
  HWND		hwndDlg;
  FILTERCDATA	stCData;

  stCData.cbData = sizeof( FILTERCDATA );
  stCData.pSCNodeUser = pSCNodeUser;

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndOwner, wpfDialog, hDSModule,
                        ID_DLG_FILTER, &stCData );
  if ( hwndDlg == NULLHANDLE )
  {
    debug( "WinLoadDlg() fail" );
    return;
  }
  pctrlDlgCenterOwner( hwndDlg );

  WinProcessDlg( hwndDlg );
  WinDestroyWindow( hwndDlg );
}
