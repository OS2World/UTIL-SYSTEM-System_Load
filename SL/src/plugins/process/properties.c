#define INCL_WIN
#include <os2.h>
#include <ds.h>
#include <sl.h>
#include "process.h"
#include <debug.h>     // Must be the last.

extern HMODULE		hDSModule;	// Module handle, process.c
extern BOOL		fShowFloppy;	// process.c
extern PHiniWriteULong	piniWriteULong;	// Helper routine, process.c
extern BOOL		fConfirmClose;
extern BOOL		fConfirmKill;



static VOID _undo(HWND hwnd)
{
  WinCheckButton( hwnd, IDD_CB_CLOSEPROG, fConfirmClose );
  WinCheckButton( hwnd, IDD_CB_KILLPROC, fConfirmKill );
}

static VOID _default(HWND hwnd)
{
  WinCheckButton( hwnd, IDD_CB_CLOSEPROG, TRUE );
  WinCheckButton( hwnd, IDD_CB_KILLPROC, TRUE );
}

static VOID _wmInitDlg(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  debugStat();
}

static VOID _wmDestroy(HWND hwnd)
{
  fConfirmClose = WinQueryButtonCheckstate( hwnd, IDD_CB_CLOSEPROG ) != 0;
  fConfirmKill = WinQueryButtonCheckstate( hwnd, IDD_CB_KILLPROC ) != 0;
  piniWriteULong( hDSModule, "confirmClose", (ULONG)fConfirmClose );
  piniWriteULong( hDSModule, "confirmKill", (ULONG)fConfirmKill );
}


static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _wmInitDlg( hwnd, mp1, mp2 );
      _undo( hwnd );
      break;

    case WM_DESTROY:
      _wmDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_UNDO:
          _undo( hwnd );
          break;

        case IDD_PB_DEFAULT:
          _default( hwnd );
          break;
      }
      return (MRESULT)FALSE;

    case WM_CONTROL:
/*      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_CB_SHOWFLOPPY:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED ||
               SHORT2FROMMP( mp1 ) == BN_DBLCLICKED )
            fShowFloppy = WinQueryButtonCheckstate( hwnd,
                                                    SHORT1FROMMP( mp1 ) ) != 0;
          break;

      }*/
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

DSEXPORT HWND APIENTRY dsLoadDlg(HWND hwndOwner, ULONG ulPage,
                                 ULONG cbTitle, PCHAR acTitle)
{
  return WinLoadDlg( hwndOwner, hwndOwner, DialogProc,
                     hDSModule, IDD_DSPROPERTIES, NULL );
}
