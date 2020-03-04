#define INCL_DOSMISC
#define INCL_WINDIALOGS
#define INCL_WINSYS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>     /* isspace() */
#include <ds.h>
#include <sl.h>
#include "irqlist.h"
#include "os4irq.h"
#include <debug.h>     // Must be the last.

extern HMODULE         hDSModule;          // Module handle, os4irq.c
extern ULONG           ulNumSelected;      // os4irq.c
extern IRQPROPERTIES   stIRQProperties;    // os4irq.c
extern PIRQ            *paIRQList;         // os4irq.c
extern PIRQDESCR       pIRQDescr;          // os4irq.c

// Helpers, os4irq.c
extern PHiniWriteStr             piniWriteStr;
extern PHiniWriteULong           piniWriteULong;
extern PHctrlDlgCenterOwner      pctrlDlgCenterOwner;
extern PHctrlSetDefaultFont      pctrlSetDefaultFont;


static ULONG		ulNumMenu = (ULONG)(-1);

extern DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex);	// os4irq.c
extern VOID os4irqCRLFtoCommaSpace(PSZ pszName, ULONG cbBuf, PCHAR pcBuf);

static MRESULT EXPENTRY NameDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      {
        CHAR		acBuf[256];
        ULONG		cbBuf;
        CHAR		szNum[8];
        PCHAR		pcNum = szNum;
        CHAR		szRcWinTitle[128];
        ULONG		cbRcWinTitle;

        pctrlDlgCenterOwner( hwnd );
        pctrlSetDefaultFont( hwnd );

        // Set dialog window title.
 
        cbRcWinTitle = WinQueryWindowText( hwnd, sizeof(szRcWinTitle),
                                           szRcWinTitle );

        ultoa( ulNumMenu, szNum, 10 );
        DosInsertMessage( &pcNum, 1, szRcWinTitle, cbRcWinTitle,
                          acBuf, sizeof(acBuf) - 1, &cbBuf );
        acBuf[cbBuf] = '\0';

        WinSetWindowText( hwnd, acBuf );

        // Controls setup

        WinSendDlgItemMsg( hwnd,
                           (stIRQProperties.aPres[ulNumMenu].ulFlags &
                            PRESFL_SHOWAUTODETECT) != 0
                             ? IDD_RB_AUTODETECT : IDD_RB_USERDEFINED,
                           BM_SETCHECK, MPFROMSHORT(1), 0 );

        os4irqCRLFtoCommaSpace( irqGetDescription( pIRQDescr, ulNumMenu ),
                                sizeof(acBuf), acBuf );
        WinSetDlgItemText( hwnd, IDD_EF_AUTODETECT, acBuf );

        if ( stIRQProperties.aPres[ulNumMenu].pszName != NULL )
          WinSetDlgItemText( hwnd, IDD_EF_USERDEFINED,
                             stIRQProperties.aPres[ulNumMenu].pszName );
      }
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case DID_OK:
        {
          CHAR		szBuf[MAX_IRQNAME_LEN + 1];
          PCHAR		pcBegin = szBuf;
          PCHAR		pcEnd;

          // Change IRQ name
          pcEnd = &szBuf[ WinQueryDlgItemText( hwnd, IDD_EF_USERDEFINED,
                                               sizeof(szBuf), szBuf ) ];
          while( isspace( *pcBegin ) ) pcBegin++;
          while( pcEnd > pcBegin && isspace( *(pcEnd - 1) ) ) pcEnd--;
          *pcEnd = '\0';

          if ( stIRQProperties.aPres[ulNumMenu].pszName != NULL )
            free( stIRQProperties.aPres[ulNumMenu].pszName );

          stIRQProperties.aPres[ulNumMenu].pszName =
                              pcBegin == pcEnd ? NULL : strdup( pcBegin );

          if ( WinSendDlgItemMsg( hwnd, IDD_RB_AUTODETECT, BM_QUERYCHECK,
                                  0, 0 ) != 0 )
            stIRQProperties.aPres[ulNumMenu].ulFlags |= PRESFL_SHOWAUTODETECT;
          else
            stIRQProperties.aPres[ulNumMenu].ulFlags &= ~PRESFL_SHOWAUTODETECT;

          // Store a new name and flags

          sprintf( szBuf, "IRQ%u", ulNumMenu );
          piniWriteStr( hDSModule, szBuf,
                        stIRQProperties.aPres[ulNumMenu].pszName );

          sprintf( szBuf, "IRQ%u_FLAGS", ulNumMenu );
          piniWriteULong( hDSModule, szBuf,
                          stIRQProperties.aPres[ulNumMenu].ulFlags );
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
  ulNumMenu = paIRQList[ulIndex]->ulIRQLevel;

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
