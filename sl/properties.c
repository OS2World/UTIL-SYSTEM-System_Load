#define INCL_WINDIALOGS
#define INCL_WIN
#include <os2.h>
#include <string.h>
#include "sl.h"
#include "srclist.h"
#include "items.h"
#include "ctrl.h"
#include "debug.h"


static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_COMMAND:
      switch( COMMANDMSG( &msg )->cmd )
      {
        case IDD_PB_UNDO:
        case IDD_PB_DEFAULT:
          {
            LONG	lPageId;
            HWND	hwndPage = NULLHANDLE;

            lPageId = (LONG)WinSendDlgItemMsg( hwnd, IDD_NOTEBOOK,
                              BKM_QUERYPAGEID, 0,
                              MPFROM2SHORT( BKA_TOP, BKA_MAJOR ) );

            if ( lPageId != 0 && lPageId != BOOKERR_INVALID_PARAMETERS )
              hwndPage = (HWND)WinSendDlgItemMsg( hwnd, IDD_NOTEBOOK,
                                                  BKM_QUERYPAGEWINDOWHWND,
                                                  MPFROMLONG( lPageId ), 0 );

            if ( hwndPage != NULLHANDLE &&
                 hwndPage != BOOKERR_INVALID_PARAMETERS )
              WinSendMsg( hwndPage,
                          COMMANDMSG( &msg )->cmd == IDD_PB_UNDO ?
                            WM_SL_UNDO : WM_SL_DEFAULT,
                          0, 0 );
          }
          return (MRESULT)TRUE;
      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


VOID dlgProperties(HWND hwndOwner)
{
  HWND		hwndDlg, hwndNB;
  HWND		ahDSPages[8];
  ULONG		cDSPages;
  ULONG		ulIdx, ulPageIdx;
  PDATASOURCE	pDataSrc;
  LONG		lPageId;

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndOwner, DialogProc, 0, IDD_PROPERTIES,
                        NULL );
  if ( hwndDlg == NULLHANDLE )
  {
    debug( "WinLoadDlg() fail" );
    return;
  }
  ctrlSubclassNotebook( hwndDlg );

  hwndNB = WinWindowFromID( hwndDlg, IDD_NOTEBOOK );
  if ( hwndNB == NULLHANDLE )
  {
    debug( "Cannot find notebook, id: %u", IDD_NOTEBOOK );
    WinDestroyWindow( hwndDlg );
    return;
  }

  WinSendMsg( hwndNB, BKM_SETNOTEBOOKCOLORS,
              MPFROMLONG ( SYSCLR_FIELDBACKGROUND ),
              MPFROMSHORT( BKA_BACKGROUNDPAGECOLORINDEX ));

  for( ulIdx = 0; ulIdx < srclstGetCount(); ulIdx++ )
  {
    pDataSrc = srclstGetByIndex( ulIdx );
    if ( pDataSrc->fnLoadDlg == NULL || !pDataSrc->fInitialized )
      continue;

    // Query dialog pages list from data source.
    cDSPages = pDataSrc->fnLoadDlg( hwndNB, &ahDSPages );
    if ( cDSPages == 0 )	// No pages returned.
      continue;

    // Insert data source's pages to the notebook.
    for( ulPageIdx = 0; ulPageIdx < cDSPages; ulPageIdx++ )
    {
      lPageId = (LONG)WinSendMsg( hwndNB, BKM_INSERTPAGE, NULL,
                        MPFROM2SHORT( BKA_AUTOPAGESIZE | BKA_STATUSTEXTON |
                                      (ulPageIdx == 0 ? BKA_MAJOR : BKA_MINOR),
                                      BKA_LAST ) );

      WinSendMsg( hwndNB, BKM_SETTABTEXT, MPFROMLONG( lPageId ),
                  MPFROMP( pDataSrc->pDSInfo->pszMenuTitle ) );
      WinSendMsg( hwndNB, BKM_SETPAGEWINDOWHWND, MPFROMLONG( lPageId ),
                  MPFROMLONG( ahDSPages[ulPageIdx] ) );
      WinSetOwner( ahDSPages[ulPageIdx], hwndNB );

      // Set "Page N from N" at top right corner.
      if ( cDSPages > 1 )
      {
        CHAR		szBuf[64]= { 0 };
        CHAR		szPage[8];
        CHAR		szTotal[8];
        PSZ		aVal[2];

        aVal[0] = ultoa( ulPageIdx + 1, &szPage, 10 );
        aVal[1] = ultoa( cDSPages, &szTotal, 10 );
        utilLoadInsertStr( 0, TRUE, IDS_PAGE_NO, 2, &aVal,
                           sizeof(szBuf), &szBuf );

        WinSendMsg( hwndNB, BKM_SETSTATUSLINETEXT, MPFROMLONG( lPageId ),
                    MPFROMP( &szBuf ));
      }

      // Activate first page of cuurent data source.
      if ( ( ulPageIdx == 0 ) && ( pDataSrc == itemsGetDataSrc() ) )
          WinSendMsg( hwndNB, BKM_TURNTOPAGE, MPFROMLONG( lPageId ), NULL );
    }
  }

  // Show dialog only if pages exists
  if ( (SHORT)WinSendMsg( hwndNB, BKM_QUERYPAGECOUNT, 0,
                          MPFROMSHORT( BKA_END ) ) > 0 )
  {
    WinSetFocus( HWND_DESKTOP, hwndNB );
    WinProcessDlg( hwndDlg );
  }

  WinDestroyWindow( hwndDlg );
}
