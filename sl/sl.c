#define INCL_DOSERRORS    /* DOS error values */
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_WIN
#include <os2.h>
#include "sl.h"
#include "srclist.h"
#include "utils.h"
#include "hmem.h"
#include "debug.h"
//#include "ft2lib.h"
#include <string.h>

HINI			hIni;
HWND			hwndMain;
HAB			hab;

HWND mwInstall(HWND hwndParent, ULONG ulId); // mainwin.c

static HMODULE		hFt2lib = NULLHANDLE;
static VOID APIENTRY (*pFt2EnableFontEngine)(BOOL fEnable);


static VOID _ft2libOn()
{
  CHAR		szErr[256];
  ULONG		ulRC;

  ulRC = DosLoadModule( &szErr, sizeof(szErr), "ft2lib.dll", &hFt2lib );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosLoadModule(), rc = %u, err: %s", ulRC, &szErr );
    return;
  }

  ulRC = DosQueryProcAddr( hFt2lib, 0, "Ft2EnableFontEngine",
                           (PFN *)&pFt2EnableFontEngine );
  if ( ulRC != NO_ERROR )
  {
    debug( "Entry Ft2EnableFontEngine not found." );
    DosFreeModule( hFt2lib );
    hFt2lib = NULLHANDLE;
    return;
  }

  pFt2EnableFontEngine( TRUE );
}

static VOID _ft2libOff()
{
  if ( hFt2lib == NULLHANDLE )
    return;

  pFt2EnableFontEngine( FALSE );
  DosFreeModule( hFt2lib );
}


int main(int argc, char **argv)
{
  ULONG		ulRC;
  HMQ   	hmq;
  QMSG		qmsg;
  CHAR		szBuf[CCHMAXPATHCOMP];

  debugInit();

  if ( hmInit() != 0 )
  {
    debug( "High memory allocator initialization failed" );
    return 1;
  }

  // Initialize PM
  hab = WinInitialize( 0 );
  if ( !hab )
  {
    debug( "WinInitialize() fail" );
    return 1;
  }

  // Create default size message queue 
  hmq = WinCreateMsgQueue( hab, 0 );
  if ( !hmq )
  {
    debug( "WinCreateMsgQueue() fail" );
    WinTerminate( hab );
    return 1;
  }

  // Open ini-file
  ulRC = utilQueryProgPath( sizeof(szBuf), &szBuf );
  strcpy( &szBuf[ulRC], INI_FILE );
  debug( "INI-file: %s", &szBuf );
  hIni = PrfOpenProfile( hab, &szBuf );
  if ( hIni == NULLHANDLE )
  {
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP,
                   "Cannot read/write configuration.", NULL, 1,
                   MB_ERROR | MB_MOVEABLE | MB_CANCEL );
    WinDestroyMsgQueue( hmq );
    WinTerminate( hab );
    return 1;
  }

  // If switch -noft2 is not specified - turn on anti-aliasing.
  if ( argc < 2 || ( stricmp( argv[1], "-noft2" ) != 0 ) )
    _ft2libOn();

  // Initialize each data source (call ds*Init() functions).
  srclstInit( hab );

  hwndMain = mwInstall( HWND_DESKTOP, 0 );

  if ( hwndMain != NULLHANDLE )
  {
    // Process messages
    while( WinGetMsg( hab, &qmsg, 0L, 0, 0 ) )
      WinDispatchMsg( hab, &qmsg );

    WinDestroyWindow( WinQueryWindow( hwndMain, QW_PARENT ) );
  }

  if ( hIni != NULLHANDLE )
    PrfCloseProfile( hIni );

  // Finalization of each data source (call ds*Done() functions).
  srclstDone();

  WinDestroyMsgQueue( hmq );
  WinTerminate( hab );

  // It's ok if _ft2libOn() was not called.
  _ft2libOff();

  hmDone();
  debugDone();

  return 0;
}
