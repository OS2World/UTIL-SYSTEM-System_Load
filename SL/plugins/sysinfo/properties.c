#include <ds.h>
#include <sl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sysinfo.h"
#include "ctrl.h"
#include <debug.h>

extern HMODULE         hDSModule;                   // Module handle, sysinfo.c
extern SYSVAR          aSysVar[];                   // sysinfo.c
extern CONVVAL         aValToStr[CONV_COUNT];       // sysinfo.c
extern USERVAR         aUserVar[SYSVAR_COUNT];      // sysinfo.c
extern ULONG           cUserVar;                    // sysinfo.c
extern ULONG           ulShowItems;                 // sysinfo.c
extern ULONG           ulInterval;                  // sysinfo.c

// Helpers, sysinfo.c
extern PHiniWriteStr             piniWriteStr;
extern PHiniWriteULong           piniWriteULong;
extern PHhelpShow                phelpShow;

// Disable button Add when selected available variable already listed in the
// user's list. Otherwise enable it.
static VOID _setEnableAddBtn(HWND hwnd)
{
  HWND       hwndVarAvail = WinWindowFromID( hwnd, IDD_CB_VARAVAIL );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarAvail, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      cUserVar = (SHORT)WinSendMsg( hwndVarList, LM_QUERYITEMCOUNT,
                                           0, 0 );
  SHORT      sIdx;
  PUSERVAR   pUserVar;
  BOOL       fListed = FALSE;

  if ( sSelIdx == LIT_NONE )
    fListed = TRUE;
  else
  {
    for( sIdx = 0; sIdx < cUserVar; sIdx++ )
    {
      pUserVar = (PUSERVAR)WinSendMsg( hwndVarList, LM_QUERYITEMHANDLE,
                                       MPFROMSHORT( sIdx ), 0 );
      if ( pUserVar->ulSysVarId == sSelIdx )
      {
        fListed = TRUE;
        break;
      }
    }
  }

  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_ADD ), !fListed );
}

// Remove all items from user's variables lists.
static VOID _clearVarList(HWND hwnd)
{
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      cUserVar = (SHORT)WinSendMsg( hwndVarList, LM_QUERYITEMCOUNT,
                                           0, 0 );
  SHORT      sIdx;
  PUSERVAR   pUserVar;

  for( sIdx = 0; sIdx < cUserVar; sIdx++ )
  {
    pUserVar = (PUSERVAR)WinSendMsg( hwndVarList, LM_QUERYITEMHANDLE,
                                     MPFROMSHORT( sIdx ), 0 );
    if ( pUserVar->pszComment != NULL )
      debugFree( pUserVar->pszComment );
    debugFree( pUserVar );
  }

  WinSendMsg( hwndVarList, LM_DELETEALL, 0, 0 );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_UP ), FALSE );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_DOWN ), FALSE );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_REMOVE ), FALSE );
  WinSendMsg( WinWindowFromID( hwnd, IDD_CB_PRESENTATION ), LM_DELETEALL, 0, 0 );
  WinSetWindowText( WinWindowFromID( hwnd, IDD_CB_PRESENTATION ), "" );
  WinSetWindowText( WinWindowFromID( hwnd, IDD_EF_COMMENTSET ), "" );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_EF_COMMENTSET ), FALSE );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_COMMENTSET ), FALSE );
}


static VOID _wmInit(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  LONG       lColor = SYSCLR_DIALOGBACKGROUND;
  ULONG      ulIdx;
  HWND       hwndVarAvail = WinWindowFromID( hwnd, IDD_CB_VARAVAIL );

  // Set background color for variable comment not-editable entry field.
  WinSetPresParam( WinWindowFromID( hwnd, IDD_EF_COMMENT ),
                   PP_BACKGROUNDCOLORINDEX, sizeof(LONG), &lColor );

  // List of available variables.
  for( ulIdx = 0; ulIdx < SYSVAR_COUNT; ulIdx++ )
    WinInsertLboxItem( hwndVarAvail, LIT_END, aSysVar[ulIdx].pszName );

  // Update intervals.
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETLIMITS,
                     MPFROMLONG( 10 ), MPFROMLONG( 1 ) );

  debugStat();
}

static VOID _wmSLUndo(HWND hwnd)
{
  const ULONG          aShowItemsRBId[] = { IDD_RB_SHOW_FULL, IDD_RB_SHOW_NAME,
                                            IDD_RB_SHOW_COMMENT };
  ULONG      ulIdx;
  PUSERVAR   pUserVarCur, pUserVar;
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      sIdx;

  _clearVarList( hwnd );

  for( ulIdx = 0; ulIdx < cUserVar; ulIdx++ )
  {
    pUserVarCur = &aUserVar[ulIdx];

    sIdx = WinInsertLboxItem( hwndVarList, LIT_END,
                              aSysVar[pUserVarCur->ulSysVarId].pszName );
    if ( sIdx == LIT_MEMERROR || sIdx == LIT_ERROR )
      continue;

    pUserVar = debugMAlloc( sizeof(USERVAR) );
    if ( pUserVar == NULL )
    {
      debug( "Not enough memory" );
      return;
    }

    pUserVar->ulSysVarId = pUserVarCur->ulSysVarId;
    pUserVar->pszComment = debugStrDup( pUserVarCur->pszComment );
    pUserVar->ulConvFunc = pUserVarCur->ulConvFunc;

    WinSendMsg( hwndVarList, LM_SETITEMHANDLE, MPFROMSHORT( sIdx ),
                MPFROMP( pUserVar ) );
  }

  WinSendDlgItemMsg( hwnd, IDD_CB_VARAVAIL, LM_SELECTITEM, MPFROMSHORT( 0 ),
                     MPFROMLONG( TRUE ) );

  // Disable button Set.
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_COMMENTSET ), FALSE );

  _setEnableAddBtn( hwnd );

  // Items style.
  WinSendDlgItemMsg( hwnd, aShowItemsRBId[ulShowItems], BM_SETCHECK,
                     MPFROMSHORT( 1 ), 0 );

  // Update interval
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETCURRENTVALUE,
                     MPFROMLONG( ulInterval ), 0 );
}

static VOID _wmDestroy(HWND hwnd)
{
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      csUserVar = (SHORT)WinSendMsg( hwndVarList, LM_QUERYITEMCOUNT,
                                           0, 0 );
  SHORT      sIdx;
  PUSERVAR   pUserVar;
  CHAR       acBuf[24];
  PCHAR      pcBuf;

  // Destroy current user's list of variables
  for( sIdx = 0; sIdx < cUserVar; sIdx++ )
    if ( aUserVar[sIdx].pszComment != NULL )
      debugFree( aUserVar[sIdx].pszComment );

  // ...and build a new one.
  for( sIdx = 0; sIdx < csUserVar; sIdx++ )
  {
    pUserVar = (PUSERVAR)WinSendMsg( hwndVarList, LM_QUERYITEMHANDLE,
                                     MPFROMSHORT( sIdx ), 0 );

    aUserVar[sIdx] = *pUserVar;
    pUserVar->pszComment = NULL; // Avoid destroy string in _clearVarList().
  }
  cUserVar = csUserVar;

  _clearVarList( hwnd );

  // Set items style.
  ulShowItems =
    WinSendDlgItemMsg( hwnd, IDD_RB_SHOW_NAME, BM_QUERYCHECK, 0, 0 ) != 0
      ? SI_ITEMS_NAME
      : WinSendDlgItemMsg( hwnd, IDD_RB_SHOW_COMMENT, BM_QUERYCHECK, 0, 0 ) != 0
          ? SI_ITEMS_COMMENT : SI_ITEMS_FULL;

  // Set update interval.
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_QUERYVALUE,
                     MPFROMP( &ulInterval ), MPFROM2SHORT( 0, SPBQ_DONOTUPDATE ) );

  // Store properties to the ini-file

  piniWriteULong( hDSModule, "VarCount", cUserVar );
  for( sIdx = 0; sIdx < cUserVar; sIdx++ )
  {
    pUserVar = &aUserVar[sIdx];
    pcBuf = &acBuf[ sprintf( &acBuf, "Var%u", sIdx ) ];

    strcpy( pcBuf, "Id" );
    piniWriteULong( hDSModule, &acBuf, pUserVar->ulSysVarId );
    strcpy( pcBuf, "Comment" );
    piniWriteStr( hDSModule, &acBuf, pUserVar->pszComment );
    strcpy( pcBuf, "Conv" );
    piniWriteULong( hDSModule, &acBuf, pUserVar->ulConvFunc );
  }
  piniWriteULong( hDSModule, "ItemsStyle", ulShowItems );
  piniWriteULong( hDSModule, "UpdateInterval", ulInterval );
}


// Button Add pressed (For the selected available variable).

static VOID _cmdAddVar(HWND hwnd)
{
  HWND       hwndVarAvail = WinWindowFromID( hwnd, IDD_CB_VARAVAIL );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarAvail, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      sIdx;
  PUSERVAR   pUserVar;

  sIdx = WinInsertLboxItem( hwndVarList, LIT_END, aSysVar[sSelIdx].pszName );
  if ( sIdx == LIT_MEMERROR || sIdx == LIT_ERROR )
    return;

  pUserVar = debugMAlloc( sizeof(USERVAR) );
  if ( pUserVar == NULL )
  {
    debug( "Not enough memory" );
    return;
  }

  pUserVar->ulSysVarId = sSelIdx;
  pUserVar->pszComment = debugStrDup( aSysVar[sSelIdx].pszComment );
  // First conv. function for the system variable from aSysVar[] is default.
  pUserVar->ulConvFunc = aSysVar[sSelIdx].aConv[0];

  WinSendMsg( hwndVarList, LM_SETITEMHANDLE, MPFROMSHORT( sIdx ),
              MPFROMP( pUserVar ) );
  WinSendMsg( hwndVarList, LM_SELECTITEM, MPFROMSHORT( sIdx ),
              MPFROMLONG( TRUE ) );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_ADD ), FALSE );
}

// Button Up/Down pressed.

static VOID _cmdMoveVar(HWND hwnd, BOOL fUp)
{
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarList, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );
  PUSERVAR   pUserVar = (PUSERVAR)WinSendMsg( hwndVarList,
                               LM_QUERYITEMHANDLE, MPFROMSHORT( sSelIdx ), 0 );

  // Delete selected item.
  WinSendMsg( hwndVarList, LM_DELETEITEM, MPFROMSHORT( sSelIdx ), 0 );
  // Change index.
  sSelIdx += fUp ? -1 : 1;
  // Insert item on new place.
  WinInsertLboxItem( hwndVarList, sSelIdx,
                     aSysVar[ pUserVar->ulSysVarId ].pszName );
  WinSendMsg( hwndVarList, LM_SETITEMHANDLE, MPFROMSHORT( sSelIdx ),
              MPFROMP( pUserVar ) );
  // Select new item.
  WinSendMsg( hwndVarList, LM_SELECTITEM, MPFROMSHORT( sSelIdx ),
              MPFROMLONG( TRUE ) );
}

// Button Remove pressed.

static VOID _cmdRemoveVar(HWND hwnd)
{
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarList, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );
  PUSERVAR   pUserVar = (PUSERVAR)WinSendMsg( hwndVarList,
                               LM_QUERYITEMHANDLE, MPFROMSHORT( sSelIdx ), 0 );

  // Delete selected item.
  WinSendMsg( hwndVarList, LM_DELETEITEM, MPFROMSHORT( sSelIdx ), 0 );
  // Destroy variable user data.
  debugFree( pUserVar->pszComment );
  debugFree( pUserVar );
  // Disable button Remove.
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_REMOVE ), FALSE );

  _setEnableAddBtn( hwnd );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_UP ), FALSE );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_DOWN ), FALSE );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_REMOVE ), FALSE );
  WinSendMsg( WinWindowFromID( hwnd, IDD_CB_PRESENTATION ), LM_DELETEALL, 0, 0 );
  WinSetWindowText( WinWindowFromID( hwnd, IDD_CB_PRESENTATION ), "" );
  WinSetWindowText( WinWindowFromID( hwnd, IDD_EF_COMMENTSET ), "" );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_EF_COMMENTSET ), FALSE );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_COMMENTSET ), FALSE );
}

// Button Set (for the comment) pressed.

static VOID _cmdCommentSet(HWND hwnd)
{
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarList, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );
  PUSERVAR   pUserVar = (PUSERVAR)WinSendMsg( hwndVarList, LM_QUERYITEMHANDLE,
                                              MPFROMSHORT( sSelIdx ), 0 );
  CHAR       acBuf[640];
  PCHAR      pcBuf;

  // Query new comment.
  pcBuf = &acBuf[ WinQueryWindowText( WinWindowFromID(hwnd, IDD_EF_COMMENTSET),
                                     sizeof(acBuf), acBuf ) ];

  // Destroy old comment in item's data.
  if ( pUserVar->pszComment != NULL )
    debugFree( pUserVar->pszComment );

  // Store a new comment in item's data.

  while( ( pcBuf > &acBuf ) && isspace( *(pcBuf-1) ) )
    pcBuf--;
  *pcBuf = '\0';
  pcBuf = &acBuf;
  while( isspace( *pcBuf ) )
    pcBuf++;

  pUserVar->pszComment = debugStrDup( pcBuf );

  // Disable button Set.
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_COMMENTSET ), FALSE );
}

// Available variable selected from the list.

static VOID _ctlVarAvailSelect(HWND hwnd)
{
  HWND       hwndVarAvail = WinWindowFromID( hwnd, IDD_CB_VARAVAIL );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarAvail, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );

  // Set variable comment text.
  WinSetWindowText( WinWindowFromID( hwnd, IDD_EF_COMMENT ),
                    aSysVar[sSelIdx].pszComment );

  _setEnableAddBtn( hwnd );
}

// Variable in user's list selected.

static VOID _ctlVarSelect(HWND hwnd)
{
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarList, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );
  PUSERVAR   pUserVar = (PUSERVAR)WinSendMsg( hwndVarList, LM_QUERYITEMHANDLE,
                                              MPFROMSHORT( sSelIdx ), 0 );
  BOOL       fSel = sSelIdx != LIT_NONE;
  HWND       hwndPresentation = WinWindowFromID( hwnd, IDD_CB_PRESENTATION );
  ULONG      ulIdx = 0;
  PSYSVAR    pSysVar;
  LONG       lConvFunc;

  // Enable/disable buttons Up/Down.
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_UP ), fSel && sSelIdx != 0 );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_DOWN ),
                   fSel && 
                   ( sSelIdx + 1 ) <
                   (SHORT)WinSendMsg( hwndVarList, LM_QUERYITEMCOUNT, 0, 0 ) );
  // Enable/disable button Remove.
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_REMOVE ), fSel );

  // Set variable comment text.
  WinSetWindowText( WinWindowFromID( hwnd, IDD_EF_COMMENTSET ),
                    pUserVar == NULL ? "" : pUserVar->pszComment );
  WinEnableWindow( WinWindowFromID( hwnd, IDD_EF_COMMENTSET ), fSel );

  // Disable button Set.
  WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_COMMENTSET ), FALSE );

  // Value presentation.

  WinSendMsg( hwndPresentation, LM_DELETEALL, 0, 0 );
  WinSetWindowText( hwndPresentation, "" );
  if ( !fSel || ( pUserVar == NULL ) )
    return;

  // Add to presentation list all conv.function titles, select current fn. for
  // the user's list selected variable.
  pSysVar = &aSysVar[pUserVar->ulSysVarId];
  while( TRUE )
  {
    lConvFunc = pSysVar->aConv[ulIdx];
    if ( lConvFunc == -1 )
      break;
    ulIdx++;

    sSelIdx = WinInsertLboxItem( hwndPresentation, LIT_END,
                                 aValToStr[lConvFunc].pszTitle );
    // Item handle is a function index in aValToStr[].
    WinSendMsg( hwndPresentation, LM_SETITEMHANDLE, MPFROMSHORT( sSelIdx ),
                MPFROMLONG( lConvFunc ) );

    if ( lConvFunc == pUserVar->ulConvFunc )
      WinSendMsg( hwndPresentation, LM_SELECTITEM, MPFROMSHORT( sSelIdx ),
                  MPFROMLONG(TRUE) );
  }
}

// Conv.function in Presentation list selected.

static VOID _ctlPresentationSelect(HWND hwnd)
{
  HWND       hwndVarList = WinWindowFromID( hwnd, IDD_LB_VAR );
  SHORT      sSelIdx = (SHORT)WinSendMsg( hwndVarList, LM_QUERYSELECTION,
                                          MPFROMSHORT( LIT_FIRST ), 0 );
  PUSERVAR   pUserVar = (PUSERVAR)WinSendMsg( hwndVarList, LM_QUERYITEMHANDLE,
                                              MPFROMSHORT( sSelIdx ), 0 );
  HWND       hwndPresentation = WinWindowFromID( hwnd, IDD_CB_PRESENTATION );

  if ( ( sSelIdx == LIT_NONE ) || ( pUserVar == NULL ) )
    return;

  sSelIdx = (SHORT)WinSendMsg( hwndPresentation, LM_QUERYSELECTION,
                               MPFROMSHORT( LIT_FIRST ), 0 );

  // Presentation item handle is a func. index in aValToStr[]
  // ( see _ctlVarSelect() ).
  if ( sSelIdx != LIT_NONE )
    pUserVar->ulConvFunc =
      (ULONG)WinSendMsg( hwndPresentation, LM_QUERYITEMHANDLE,
                         MPFROMSHORT( sSelIdx ), 0 );
}

// Set default properties.

static VOID _wmSLDefault(HWND hwnd)
{
  HWND       hwndVarAvail = WinWindowFromID( hwnd, IDD_CB_VARAVAIL );

  _clearVarList( hwnd );

  // QSV_MAXPRMEM
  WinSendMsg( hwndVarAvail, LM_SELECTITEM, MPFROMSHORT(16), MPFROMLONG(TRUE) );
  _cmdAddVar( hwnd );

  // QSV_MAXSHMEM
  WinSendMsg( hwndVarAvail, LM_SELECTITEM, MPFROMSHORT(17), MPFROMLONG(TRUE) );
  _cmdAddVar( hwnd );

  // QSV_MAXHPRMEM
  WinSendMsg( hwndVarAvail, LM_SELECTITEM, MPFROMSHORT(22), MPFROMLONG(TRUE) );
  _cmdAddVar( hwnd );

  // QSV_MAXHSHMEM
  WinSendMsg( hwndVarAvail, LM_SELECTITEM, MPFROMSHORT(23), MPFROMLONG(TRUE) );
  _cmdAddVar( hwnd );

  WinSendMsg( hwndVarAvail, LM_SELECTITEM, MPFROMSHORT(0), MPFROMLONG(TRUE) );
  WinSendDlgItemMsg( hwnd, IDD_LB_VAR, LM_SELECTITEM,
                     MPFROMSHORT( LIT_NONE ), MPFROMLONG( TRUE ) );
  _ctlVarSelect( hwnd );
  _setEnableAddBtn( hwnd );

  // Items style.
  WinSendDlgItemMsg( hwnd, IDD_RB_SHOW_FULL, BM_SETCHECK, MPFROMSHORT( 1 ), 0 );

  // Update interval.
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETCURRENTVALUE,
                     MPFROMLONG( 1 ), 0 );
}

static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _wmInit( hwnd, mp1, mp2 );
      // go to WM_SL_UNDO
    case WM_SL_UNDO:
      _wmSLUndo( hwnd );
      break;

    case WM_SL_DEFAULT:
      _wmSLDefault( hwnd );
      break;

    case WM_DESTROY:
      _wmDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_ADD:
          _cmdAddVar( hwnd );
          break;

        case IDD_PB_UP:
          _cmdMoveVar( hwnd, TRUE );
          break;

        case IDD_PB_DOWN:
          _cmdMoveVar( hwnd, FALSE );
          break;

        case IDD_PB_REMOVE:
          _cmdRemoveVar( hwnd );
          break;

        case IDD_PB_COMMENTSET:
          _cmdCommentSet( hwnd );
          break;
      }
      return (MRESULT)FALSE;

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_CB_VARAVAIL:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
            _ctlVarAvailSelect( hwnd );
          break;

        case IDD_LB_VAR:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
            _ctlVarSelect( hwnd );
          break;

        case IDD_CB_PRESENTATION:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
            _ctlPresentationSelect( hwnd );
          break;

        case IDD_EF_COMMENTSET:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            // Tnable button Set.
            WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_COMMENTSET ), TRUE );
          break;
      }
      return (MRESULT)FALSE;

    case WM_SL_HELP:
      // Button "Help" pressed - show help panel (see cpu.ipf).
      phelpShow( hDSModule, 20801 );
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


// Data source's interface routine dsLoadDlg()
// -------------------------------------------

DSEXPORT ULONG APIENTRY dsLoadDlg(HWND hwndOwner, PHWND pahWnd)
{
  pahWnd[0] = WinLoadDlg( hwndOwner, hwndOwner, DialogProc,
                          hDSModule, IDD_DSPROPERTIES, NULL );

  return 1;
}
