#define INCL_WINDIALOGS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <string.h>
#include "sl.h"
#include "srclist.h"
#include "items.h"
#include "ctrl.h"
#include "utils.h"
#include "debug.h"

static VOID _wmInit(HWND hwnd)
{
  HWND			hwndControl;
  ULONG			ulIdx;
  CHAR			szBuf[128];
  PDATASOURCE		pDataSrc;
  SHORT			sIndex;
  ULONG			ulSelDSIdx = ((ULONG)(-1));

  hwndControl = WinWindowFromID( hwnd, IDD_LB_MODULES );
  for( ulIdx = 0; ulIdx < srclstGetCount(); ulIdx++ )
  {
    pDataSrc = srclstGetByIndex( ulIdx );

    // Get data source title without '~'.
    strRemoveMnemonic( sizeof(szBuf), &szBuf, &pDataSrc->szTitle );
    // Add data source title and pointer to data source to the list
    sIndex = WinInsertLboxItem( hwndControl, LIT_END, &szBuf );
    if ( sIndex != LIT_MEMERROR || sIndex != LIT_ERROR )
      WinSendMsg( hwndControl, LM_SETITEMHANDLE, MPFROMSHORT( sIndex ),
                  MPFROMP( pDataSrc ) );

    if ( itemsGetDataSrc() == pDataSrc )
      ulSelDSIdx = ulIdx;
  }

  // Select item of current data source.
  if ( ulSelDSIdx != ((ULONG)(-1)) )
    WinSendMsg( hwndControl, LM_SELECTITEM, MPFROMSHORT( ulSelDSIdx ),
                MPFROMSHORT( 1 ) );
}

static MRESULT _wmMeasureitem(HWND hwnd, ULONG ulCrtlId)
{
  SIZEL		sizeText;
  HPS		hps;
  HWND		hwndControl = WinWindowFromID( hwnd, IDD_LB_MODULES );

  // Get text height from the reserved window words. 
  sizeText.cy = WinQueryWindowULong( hwndControl, QWL_USER );
  if ( sizeText.cy == 0 )	// Was not detected.
  {
    // Detect text height and store it in the reserved window words. 
    hps = WinGetPS( hwndControl );
    utilGetTextSize( hps, 1, "E", &sizeText );
    WinReleasePS( hps );
    WinSetWindowULong( hwndControl, QWL_USER, sizeText.cy );
  }

  return MRFROM2SHORT( sizeText.cy + (sizeText.cy >> 1), 0 );
}

static MRESULT _wmDrawItem(HWND hwnd, ULONG ulCrtlId, POWNERITEM pstOI)
{
  CHAR		szBuf[256];
  POINTL	pt;
  HWND		hwndControl = WinWindowFromID( hwnd, ulCrtlId );
  ULONG		ulTextCY = WinQueryWindowULong( hwndControl, QWL_USER );
  LONG		lBgColor;
  LONG		lColor;
 
  WinSendMsg( hwndControl, LM_QUERYITEMTEXT,
              MPFROM2SHORT( pstOI->idItem, sizeof(szBuf) ), MPFROMP( &szBuf ) );

  if ( pstOI->fsState )
  {
    lBgColor = SYSCLR_MENUHILITEBGND;
    lColor = SYSCLR_MENUHILITE;
  }
  else
  {
    lBgColor = GpiQueryBackColor( pstOI->hps );
    lColor = SYSCLR_MENUTEXT;
  }

  if ( !srclstIsEnabled( pstOI->idItem ) )
  {
    GpiCreateLogColorTable( pstOI->hps, 0, LCOLF_RGB, 0, 0, NULL );
    lColor = utilMixRGB( GpiQueryRGBColor( pstOI->hps, 0, lColor ),
                         GpiQueryRGBColor( pstOI->hps, 0, lBgColor ), 140 );
  }

  utilBox( pstOI->hps, &pstOI->rclItem, lBgColor );
  pt.x = pstOI->rclItem.xLeft + 2;
  pt.y = pstOI->rclItem.yBottom + (ulTextCY >> 2);
  GpiSetColor( pstOI->hps, lColor );
  GpiCharStringAt( pstOI->hps, &pt, strlen( &szBuf ), &szBuf );
  WinUpdateWindow( pstOI->hwnd );

  if ( pstOI->fsState != pstOI->fsStateOld )
  {
    pstOI->fsState = pstOI->fsStateOld;
  }
  else if ( pstOI->fsState != 0 )
  {
    pstOI->fsState = 0;
    pstOI->fsStateOld = 0;
  }

  return (MRESULT)TRUE;

/*      typedef struct _OWNERITEM {
        HWND    hwnd;
        HPS     hps;
        ULONG   fsState;
        ULONG   fsAttribute;
        ULONG   fsStateOld;
        ULONG   fsAttributeOld;
        RECTL   rclItem;
        LONG    idItem;
        ULONG   hItem;
      }*/
}

static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      ctrlDlgCenterOwner( hwnd );
      _wmInit( hwnd );
      break;

    case WM_DESTROY:
      srclstStoreOrder();
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_DSUP:
        case IDD_PB_DSDOWN:
          {
            LONG		lSelIdx;
            PDATASOURCE		pDataSrc;
            HWND		hwndControl;
            CHAR		szBuf[128];
            BOOL		fForward = SHORT1FROMMP( mp1 ) == IDD_PB_DSDOWN;

            hwndControl = WinWindowFromID( hwnd, IDD_LB_MODULES );
            lSelIdx = WinQueryLboxSelectedItem( hwndControl );

            if ( srclstMove( lSelIdx, fForward ) )
            {
              // Save selected item text and pointer to data source.
              WinQueryLboxItemText( hwndControl, lSelIdx, &szBuf, sizeof(szBuf) );
              pDataSrc = (PDATASOURCE)WinSendMsg( hwndControl, LM_QUERYITEMHANDLE,
                                                  MPFROMSHORT( lSelIdx ), 0 );
              // Delete selected item.
              WinSendMsg( hwndControl, LM_DELETEITEM, MPFROMSHORT( lSelIdx ), 0 );
              // Change index.
              lSelIdx += fForward ? 1 : -1;
              // Insert item on new place with saved text and field number.
              WinInsertLboxItem( hwndControl, lSelIdx, &szBuf );
              WinSendMsg( hwndControl, LM_SETITEMHANDLE, MPFROMSHORT( lSelIdx ),
                          MPFROMP( pDataSrc ) );
              // Select new item.
              WinSendMsg( hwndControl, LM_SELECTITEM, MPFROMSHORT( lSelIdx ),
                          MPFROMSHORT( 1 ) );
            }
          }
          break;
      }
      return (MRESULT)FALSE;

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_LB_MODULES:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            PDATASOURCE	pDataSrc;
            LONG	lSelIdx = WinQueryLboxSelectedItem( HWNDFROMMP( mp2 ) );
            HWND	hwndOwner = WinQueryWindow( hwnd, QW_OWNER );

            pDataSrc = (PDATASOURCE)WinSendMsg( HWNDFROMMP( mp2 ),
                                                LM_QUERYITEMHANDLE,
                                                MPFROMSHORT( lSelIdx ), 0 );

            // Enable/disable buttons Up/Down.
            WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_DSUP ), lSelIdx > 0 );
            WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_DSDOWN ),
                             lSelIdx < WinQueryLboxCount( HWNDFROMMP(mp2) ) - 1 );
            // Check button "Enable"
            WinCheckButton( hwnd, IDD_CB_DSENABLE, !pDataSrc->fDisable );

            // Select data source
            WinSendMsg( hwndOwner, WM_COMMAND,
                        MPFROMLONG( pDataSrc->ulMenuItemId ), 0 );
          }
          break;

        case IDD_CB_DSENABLE:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED ||
               SHORT2FROMMP( mp1 ) == BN_DBLCLICKED )
          {
            LONG		lSelIdx;
            PDATASOURCE		pDataSrc;
            HWND		hwndControl;
            HWND		hwndOwner = WinQueryWindow( hwnd, QW_OWNER );
            USHORT		sChecked = WinQueryButtonCheckstate( hwnd,
                                             SHORT1FROMMP( mp1 ) );

            hwndControl = WinWindowFromID( hwnd, IDD_LB_MODULES );
            lSelIdx = WinQueryLboxSelectedItem( hwndControl );
            pDataSrc = (PDATASOURCE)WinSendMsg( hwndControl, LM_QUERYITEMHANDLE,
                                                MPFROMSHORT( lSelIdx ), 0 );
            srclstEnable( lSelIdx, sChecked );
            // Select data source
            WinSendMsg( hwndOwner, WM_COMMAND,
                        MPFROMLONG( pDataSrc->ulMenuItemId ), 0 );
            WinInvalidateRect( hwndControl, NULL, FALSE );
          }
          break;
      }
      return (MRESULT)FALSE;

    case WM_MEASUREITEM:
      return _wmMeasureitem( hwnd, SHORT1FROMMP(mp1) );

    case WM_DRAWITEM:
      return _wmDrawItem( hwnd, SHORT1FROMMP(mp1),
                          (POWNERITEM)PVOIDFROMMP(mp2) );
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


VOID dlgSettings(HWND hwndOwner)
{
  HWND		hwndDlg;

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndOwner, DialogProc, 0, IDD_SETTINGS,
                        NULL );
  if ( hwndDlg == NULLHANDLE )
  {
    debug( "WinLoadDlg() fail" );
    return;
  }

  WinProcessDlg( hwndDlg );
  WinDestroyWindow( hwndDlg );
}
