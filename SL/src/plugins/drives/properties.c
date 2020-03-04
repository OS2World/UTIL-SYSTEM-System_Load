#define INCL_WIN
#include <os2.h>
#include <ds.h>
#include <sl.h>
#include "drives.h"
#include <debug.h>     // Must be the last.

extern HMODULE		hDSModule;	// Module handle, drives.c
extern BOOL		fShowFloppy;	// drive.c
extern PHiniWriteULong	piniWriteULong;	// Helper routine, drive.c

static BOOL		fOldShowFloppy;


static VOID _undo(HWND hwnd)
{
  fShowFloppy = fOldShowFloppy;
  WinCheckButton( hwnd, IDD_CB_SHOWFLOPPY, fShowFloppy );
}

static VOID _default(HWND hwnd)
{
  fShowFloppy = FALSE;
  // Uncheck button "Show floppy drives"
  WinCheckButton( hwnd, IDD_CB_SHOWFLOPPY, FALSE );
}

static VOID _wmInitDlg(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  debugStat();
  fOldShowFloppy = fShowFloppy;
}

static VOID _wmDestroy(HWND hwnd)
{
  // Store properties to the ini-file
  piniWriteULong( hDSModule, "ShowFloppy", (ULONG)fShowFloppy );
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
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_CB_SHOWFLOPPY:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED ||
               SHORT2FROMMP( mp1 ) == BN_DBLCLICKED )
          {
            fShowFloppy = WinQueryButtonCheckstate( hwnd,
                                                    SHORT1FROMMP( mp1 ) ) != 0;
          }
          break;

      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

DSEXPORT HWND APIENTRY dsLoadDlg(HWND hwndOwner, ULONG ulPage,
                                 ULONG cbTitle, PCHAR acTitle)
{
  return WinLoadDlg( hwndOwner, hwndOwner, DialogProc, hDSModule,
                     IDD_DSPROPERTIES, NULL );
}
