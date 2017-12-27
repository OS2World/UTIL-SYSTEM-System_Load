#define INCL_DOSMISC
#define INCL_WINDIALOGS
#define INCL_WINSYS		/* Or use INCL_WIN, INCL_PM, */
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>		/* isspace() */
#include <ds.h>
#include <sl.h>
#include "os4irq.h"
#include <debug.h>

extern HMODULE		hDSModule;		// Module handle, os4irq.c
extern ULONG		ulNumSelected;		// os4irq.c
extern IRQPROPERTIES	stIRQProperties;	// os4irq.c
extern PIRQ		*paIRQList;		// os4irq.c

// Helpers, os4irq.c
extern PHiniWriteStr		piniWriteStr;
extern PHctrlDlgCenterOwner	pctrlDlgCenterOwner;

static ULONG		ulNumMenu = (ULONG)(-1);

extern DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex);	// os4irq.c

static MRESULT EXPENTRY NameDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      {
        CHAR		szRcWinTitle[64];
        LONG		cbRcWinTitle;
        CHAR		szNum[8];
        PCHAR		pcNum = &szNum;
        CHAR		szWinTitle[64];
        ULONG		cbWinTitle;

        pctrlDlgCenterOwner( hwnd );

        // Set dialog window title.
 
        cbRcWinTitle = WinQueryWindowText( hwnd, sizeof(szRcWinTitle),
                                           &szRcWinTitle );

        ultoa( ulNumMenu, &szNum, 10 );
        DosInsertMessage( &pcNum, 1, &szRcWinTitle, cbRcWinTitle,
                          &szWinTitle, sizeof(szWinTitle) - 1, &cbWinTitle );
        szWinTitle[cbWinTitle] = '\0';

        WinSetWindowText( hwnd, szWinTitle );

        // Set entry field text to current name

        WinSendDlgItemMsg( hwnd, IDD_EF_NAME, EM_SETTEXTLIMIT,
                           MPFROMSHORT( 128 ), 0 );
        if ( stIRQProperties.apszNames[ulNumMenu] != NULL )
          WinSetDlgItemText( hwnd, IDD_EF_NAME,
                             stIRQProperties.apszNames[ulNumMenu] );
      }
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_SET:
        {
          CHAR		szBuf[128];
          PCHAR		pcBegin = &szBuf;
          PCHAR		pcEnd;

          // Change IRQ name
          pcEnd = &szBuf[ WinQueryDlgItemText( hwnd, IDD_EF_NAME,
                                               sizeof(szBuf), &szBuf ) ];
          while( isspace( *pcBegin ) ) pcBegin++;
          while( pcEnd > pcBegin && isspace( *(pcEnd - 1) ) ) pcEnd--;
          *pcEnd = '\0';

          if ( stIRQProperties.apszNames[ulNumMenu] != NULL )
            debugFree( stIRQProperties.apszNames[ulNumMenu] );

          stIRQProperties.apszNames[ulNumMenu] =
                              pcBegin == pcEnd ? NULL : debugStrDup( pcBegin );

          // Store a new name
          sprintf( &szBuf, "IRQ%u", ulNumMenu );
          piniWriteStr( hDSModule, &szBuf,
                        stIRQProperties.apszNames[ulNumMenu] );
          break;
        }
      }
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


DSEXPORT VOID APIENTRY dsFillMenu(HWND hwndMenu, ULONG ulIndex)
{
  MENUITEM		stMINew = { 0 };
  CHAR			szNum[8];
  PCHAR			pcNum = &szNum;
  PSZ			pszMsg = "IRQ %1 ~name...";
  CHAR			szBuf[32];
  ULONG			cbBuf;

  if ( ulIndex == DS_SEL_NONE )
    return;

  // Store IRQ No. for which built menu.
  ulNumMenu = paIRQList[ulIndex]->ulNum;

  stMINew.iPosition = MIT_END;
  stMINew.afStyle = MIS_SEPARATOR;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );

  ultoa( ulNumMenu, &szNum, 10 );
  DosInsertMessage( &pcNum, 1, pszMsg, strlen( pszMsg ), &szBuf,
                    sizeof(szBuf) - 1, &cbBuf );
  szBuf[cbBuf] = '\0';

  stMINew.afStyle = MIS_TEXT;
  stMINew.id = IDM_DSCMD_FIRST_ID;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
}

DSEXPORT ULONG APIENTRY dsCommand(HWND hwndOwner, USHORT usCommand)
{
  HWND		hwndDlg;

  if ( usCommand != IDM_DSCMD_FIRST_ID || ulNumMenu == (ULONG)(-1) )
    return DS_UPD_NONE;

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndOwner, NameDialogProc, hDSModule,
                        IDD_IRQNAME, NULL );
  if ( hwndDlg == NULLHANDLE )
    return DS_UPD_NONE;

  WinProcessDlg( hwndDlg );
  WinDestroyWindow( hwndDlg );

  ulNumMenu = (ULONG)(-1);

  return DS_UPD_LIST;
}

DSEXPORT ULONG APIENTRY dsEnter(HWND hwndOwner)
{
  ulNumMenu = ulNumSelected;
  return dsCommand( hwndOwner, IDM_DSCMD_FIRST_ID );
}
