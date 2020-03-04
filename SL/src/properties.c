#define INCL_WINDIALOGS
#define INCL_WIN
#include <os2.h>
#include <string.h>
#include "sl.h"
#include "srclist.h"
#include "items.h"
#include "ctrl.h"
#include "debug.h"     // Must be the last.


static VOID _propertiesResize(HWND hwnd)
{
  HWND       hwndNB = WinWindowFromID( hwnd, IDD_NOTEBOOK );
  RECTL      rect;

  if ( ( hwndNB != NULLHANDLE ) &&
       WinQueryWindowRect( hwnd, &rect ) &&
       WinCalcFrameRect( hwnd, &rect, TRUE ) )
  {
    WinSetWindowPos( hwndNB, 0, rect.xLeft, rect.yBottom,
                     rect.xRight - rect.xLeft, rect.yTop - rect.yBottom,
                     SWP_SIZE | SWP_MOVE );
  }
}

static MRESULT EXPENTRY PropertiesDlgProc(HWND hwnd, ULONG msg,
                                          MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_WINDOWPOSCHANGED:
      if ( (((PSWP)PVOIDFROMMP(mp1))->fl & SWP_SIZE) == 0 )
        break;

    case WM_INITDLG:
      _propertiesResize( hwnd );
      break;

    case WM_ERASEBACKGROUND:
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

VOID dlgProperties(HWND hwndOwner)
{
  HWND		hwndDlg, hwndNB, hwndPage;
  ULONG		ulIdx, ulPageIdx;
  USHORT        usPageStyle;
  PDATASOURCE	pDataSrc;
  LONG		lPageId;
  CHAR          szBuf[128];
  BOOKPAGEINFO  stBPI = { 0 };

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndOwner, PropertiesDlgProc, 0,
                        IDD_PROPERTIES, NULL );
  if ( hwndDlg == NULLHANDLE )
  {
    debugCP( "WinLoadDlg() fail" );
    return;
  }

  hwndNB = WinWindowFromID( hwndDlg, IDD_NOTEBOOK );
  ctrlSetDefaultFont( hwndNB );
  if ( hwndNB == NULLHANDLE )
  {
    debug( "Cannot find notebook, id: %u", IDD_NOTEBOOK );
    WinDestroyWindow( hwndDlg );
    return;
  }

  stBPI.cb = sizeof(BOOKPAGEINFO);
  stBPI.fl = BFA_MINORTABTEXT;
  stBPI.pszMinorTab = szBuf;

  for( ulIdx = 0; ulIdx < srclstGetCount(); ulIdx++ )
  {
    pDataSrc = srclstGetByIndex( ulIdx );
    if ( pDataSrc->fnLoadDlg == NULL || !pDataSrc->fInitialized )
      continue;

    // Insert data source's pages to the notebook.
    for( ulPageIdx = 0; ulPageIdx < pDataSrc->pDSInfo->ulPropPages;
         ulPageIdx++ )
    {
      // Default page title ("status line") is a plugin name
      strlcpy( szBuf, pDataSrc->szTitle, sizeof(szBuf) );

      // Query dialog page from data source
      hwndPage = pDataSrc->fnLoadDlg( hwndNB, ulPageIdx, sizeof(szBuf), szBuf );
      if ( hwndPage == NULLHANDLE )
        continue;
      ctrlSetDefaultFont( hwndPage );

      usPageStyle = ulPageIdx == 0
                      ? (BKA_MAJOR | BKA_AUTOPAGESIZE | BKA_STATUSTEXTON)
                      : (BKA_MINOR | BKA_AUTOPAGESIZE | BKA_STATUSTEXTON);

      lPageId = (LONG)WinSendMsg( hwndNB, BKM_INSERTPAGE, NULL,
                                  MPFROM2SHORT(usPageStyle,BKA_LAST) );

      WinSendMsg( hwndNB, BKM_SETPAGEWINDOWHWND, MPFROMLONG(lPageId),
                  MPFROMLONG(hwndPage) );
      WinSetOwner( hwndPage, hwndNB );

      // Set the tab text
      WinSendMsg( hwndNB, BKM_SETTABTEXT, MPFROMLONG(lPageId),
                  MPFROMP(pDataSrc->szTitle) );

      // Set the page title
      stBPI.cbMinorTab = strlen( szBuf );
      WinSendMsg( hwndNB, BKM_SETPAGEINFO, MPFROMLONG(lPageId),
                  MPFROMP(&stBPI) );

      // Set "Page N of N" at top right corner.
      if ( pDataSrc->pDSInfo->ulPropPages > 1 )
      {
        CHAR           szPage[8];
        CHAR           szTotal[8];
        PSZ            aVal[2];

        aVal[0] = ultoa( ulPageIdx + 1, szPage, 10 );
        aVal[1] = ultoa( pDataSrc->pDSInfo->ulPropPages, szTotal, 10 );
        if ( utilLoadInsertStr( 0, TRUE, IDS_PAGE_NO, 2, aVal,
                                sizeof(szBuf), szBuf ) != 0 )
          WinSendMsg( hwndNB, BKM_SETSTATUSLINETEXT, MPFROMLONG(lPageId),
                      MPFROMP(szBuf) );
      }

      // Activate first page of curent data source
      if ( ( ulPageIdx == 0 ) && ( pDataSrc == itemsGetDataSrc() ) )
        WinSendMsg( hwndNB, BKM_TURNTOPAGE, MPFROMLONG(lPageId), NULL );
    }
  }

  // Show dialog only if pages exists
  if ( (SHORT)WinSendMsg( hwndNB, BKM_QUERYPAGECOUNT, 0,
                          MPFROMSHORT(BKA_END) ) > 0 )
  {
    ctrlDlgCenterOwner( hwndDlg );
    WinSetFocus( HWND_DESKTOP, hwndDlg );
    WinProcessDlg( hwndDlg );
  }

  WinDestroyWindow( hwndDlg );
  debugStat();
}
