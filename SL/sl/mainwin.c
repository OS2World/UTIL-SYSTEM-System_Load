#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include "srclist.h"
#include "items.h"
#include "sl.h"
#include "debug.h"

#define WC_SL_MAIN		"SLMain"

#define IDWND_DETAILS		100
#define IDWND_LIST		101

#define MAX_ACCELTABLE_ENTRIES	32
#define MAX_ACCELTABLE_SIZE	( sizeof(ACCELTABLE) + MAX_ACCELTABLE_ENTRIES * sizeof(ACCEL) )

extern HINI		hIni;			// sl.c

HWND dtlInstall(HWND hwndParent, ULONG ulId);	// details.c
HWND lstInstall(HWND hwndParent, ULONG ulId);	// list.c
VOID dlgProperties(HWND hwndOwner);		// properties.c
VOID dlgSettings(HWND hwndOwner);		// settings.c

// _makeDataSourcesMenu(HWND hwndMenu)
// Removes all exists items for data sources and creates new items for exists
// sources.

static VOID _setMenuDataSources(HWND hwndMenu)
{
  LONG			lIdx;
  SHORT			sItemId;
  MENUITEM		stMI;
  PDATASOURCE		pDataSrc;
  PSZ			pszTitle;
  CHAR			szBuf[128];

  // Remove items for data sources

  for( lIdx = SHORT1FROMMP(WinSendMsg( hwndMenu, MM_QUERYITEMCOUNT, 0, 0 )) - 1;
       lIdx >= 0; lIdx-- )
  {
    sItemId = SHORT1FROMMR( WinSendMsg( hwndMenu, MM_ITEMIDFROMPOSITION,
                                        MPFROMSHORT( lIdx ), 0 ) );
    // Skip separator (MIT_ERROR) and items not for data sources.
    if ( sItemId != MIT_ERROR &&
         ( sItemId >= IDM_DATASRC_FIRST_ID && sItemId <= IDM_DATASRC_LAST_ID ) )
      WinSendMsg( hwndMenu, MM_REMOVEITEM, MPFROM2SHORT( sItemId, 0 ), 0 );
  }

  // Create new items for data sources

  stMI.afStyle = MIS_TEXT;
  stMI.hwndSubMenu = NULLHANDLE;
  stMI.hItem = NULLHANDLE;

  for( stMI.iPosition = 0; stMI.iPosition < srclstGetCount(); stMI.iPosition++ )
  {
    pDataSrc = srclstGetByIndex( stMI.iPosition );

    stMI.afAttribute = pDataSrc->fInitialized ? 0 : MIA_DISABLED;
    stMI.id = IDM_DATASRC_FIRST_ID + stMI.iPosition;
    if ( stMI.id >= IDM_DATASRC_LAST_ID )
      break;

    // Build menu item title, module info used.
    if ( stMI.iPosition < 9 )
    {
      // First 9 items will have tip Ctrl+1 .. Ctrl+9
      _bprintf( &szBuf, sizeof(szBuf), "%s\tCtrl+%c", 
                pDataSrc->szTitle, (CHAR)('1' + stMI.iPosition) );
      pszTitle = &szBuf;
    }

    // Insert new main menu item to select data source
    sItemId = SHORT1FROMMR( WinSendMsg( hwndMenu, MM_INSERTITEM,
                            MPFROMP(&stMI), MPFROMP( pszTitle ) ) );
    if ( sItemId == MIT_MEMERROR || sItemId == MIT_ERROR )
    {
      debug( "Cannot insert new item, rc = %d", sItemId );
      continue;
    }

    // Store menu item ID
    pDataSrc->ulMenuItemId = stMI.id;

    // Check current data source's menu item
    if ( pDataSrc == itemsGetDataSrc() )
      WinSendMsg( hwndMenu, MM_SETITEMATTR, MPFROM2SHORT( stMI.id, TRUE ),
                  MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED ) );
  }
}

// _loadAccelDataSources(HAB hab, HMODULE hMod, ULONG ulAccelId)
// Loads an accelerator table from the resource and insert entries
// Ctrl+1 .. Ctrl+N to select data sources.
// Returns new accelerator table.

static HACCEL _loadAccelDataSources(HAB hab, HMODULE hMod, ULONG ulAccelId)
{
  HACCEL		hAccel;
  PACCELTABLE		pAccel;
  ULONG			cbAccel;
  LONG			lIdx;
  ULONG			cDataSrc = min( srclstGetCount(), 9 );

  // Load an accelerator table from the resource.
  hAccel = WinLoadAccelTable( hab, hMod, ulAccelId );
  if ( hAccel == NULLHANDLE )
  {
    debug( "Cannot load accelerator table #%u", ulAccelId );
    return NULLHANDLE;
  }

  // Get accelerator-table structure

  cbAccel = WinCopyAccelTable( hAccel, NULL, 0 );
  cbAccel += cDataSrc * sizeof(ACCEL);
  pAccel = debugMAlloc( cbAccel );
  if ( pAccel == NULL )
  {
    debug( "Not enough memory" );
    WinDestroyAccelTable( hAccel );
    return NULLHANDLE;
  }

  if ( WinCopyAccelTable( hAccel, pAccel, cbAccel ) == 0 )
  {
    debug( "WinCopyAccelTable() fail" );
    WinDestroyAccelTable( hAccel );
    debugFree( pAccel );
    return NULLHANDLE;
  }

  if ( !WinDestroyAccelTable( hAccel ) )
    debug( "WinDestroyAccelTable() fail" );

  // Remove accelerator entries for Ctrl+1 .. Ctrl..N

  for( lIdx = pAccel->cAccel - 1; lIdx >= 0; lIdx-- )
  {
    if ( ( pAccel->aaccel[lIdx].fs == AF_CONTROL | AF_CHAR ) &&
         ( ('1' - pAccel->aaccel[lIdx].key) < cDataSrc ) )
    {
      debug( "Remove Ctrl+%c", pAccel->aaccel[lIdx].key );
      if ( lIdx < pAccel->cAccel )
        memcpy( &pAccel->aaccel[lIdx], &pAccel->aaccel[pAccel->cAccel],
                sizeof(ACCEL) );
      pAccel->cAccel--;
    }
  }

  // Create new accelerator enrties

  for( lIdx = 0; lIdx < cDataSrc; lIdx++ )
  {
    pAccel->aaccel[pAccel->cAccel].fs = AF_CONTROL | AF_CHAR;
    pAccel->aaccel[pAccel->cAccel].key = ('1' + lIdx);
    pAccel->aaccel[pAccel->cAccel].cmd = IDM_DATASRC_FIRST_ID + lIdx;
    pAccel->cAccel++;
  }

  hAccel = WinCreateAccelTable( hab, pAccel );
  if ( hAccel == NULLHANDLE )
    debug( "WinCreateAccelTable() fail" );
  debugFree( pAccel );

  return hAccel;
}

// _makeDataSourcesMenus(HWND hwnd)
// Make items in context menu and main menu for data sources. Load
// accelerator-table, add entries for data sources and set it to window.
// hwnd - main window.

static VOID _makeDataSourcesMenus(HWND hwnd)
{
  HWND			hwndMenu;
  MENUITEM		stMI;
  HACCEL		hAccel;
  HWND			hwndFrame = WinQueryWindow( hwnd, QW_PARENT );
  HAB			hab = WinQueryAnchorBlock( hwnd );

  // Create data sources menu items at the context menu

  hwndMenu = LONGFROMMR( WinSendMsg( WinWindowFromID( hwnd, IDWND_LIST ),
                                     WM_SL_QUERY,
                                     MPFROMLONG( SLQUERY_CTXMENU ), 0 ) );
  if ( hwndMenu == NULLHANDLE )
    debug( "No context menu" );
       // Query context menu item "Modules" (submenu)
  else if ( !WinSendMsg( hwndMenu, MM_QUERYITEM, MPFROM2SHORT( IDM_DATASRC, TRUE ),
                    MPFROMP( &stMI ) ) )
    debug( "Context menu have no submenu for modules list" );
  else
    _setMenuDataSources( stMI.hwndSubMenu );

  // Create data sources menu items at the main menu

  hwndMenu = WinWindowFromID( hwndFrame, FID_MENU );
  if ( hwndMenu == NULLHANDLE )
    debug( "No main menu" );
  // Query main menu item "Modules" (submenu)
  else if ( !WinSendMsg( hwndMenu, MM_QUERYITEM, MPFROM2SHORT( IDM_DATASRC, TRUE ),
                         MPFROMP( &stMI ) ) )
    debug( "Main menu have no submenu for modules list" );
  else
    _setMenuDataSources( stMI.hwndSubMenu );

  // Destroy previous accelerator-table

  hAccel = WinQueryWindowULong( hwnd, 0 );
  if ( hAccel != NULLHANDLE && !WinDestroyAccelTable( hAccel ) )
    debug( "WinDestroyAccelTable() fail" );

  // Create and set accelerator-table

  hAccel = _loadAccelDataSources( hab, 0, ID_ACCELTABLE );
  if ( hAccel != NULLHANDLE )
  {
    WinSetAccelTable( hab, hAccel, hwndFrame );
    // Store handle of the new accelerator-table (we must destroy it later).
    WinSetWindowULong( hwnd, 0, hAccel );
  }
}


static MRESULT _wmCreate(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  HWND			hwndFrame = WinQueryWindow( hwnd, QW_PARENT );
  HWND			hwndList, hwndDetails;
  LONG			alSizePos[5];
  ULONG			cbWinSizePos = sizeof(alSizePos);
  ULONG			ulCXScr = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
  ULONG			ulCYScr = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );
  CHAR			szModule[13];
  PDATASOURCE		pDataSrc, pSelDataSrc = NULL;
  ULONG			ulIdx;

  // Insert list window
  hwndList = lstInstall( hwnd, IDWND_LIST );
  if ( hwndList == NULLHANDLE )
    return (MRESULT)TRUE;

  // Insert details window
  hwndDetails = dtlInstall( hwnd, IDWND_DETAILS );
  if ( hwndDetails == NULLHANDLE )
    return (MRESULT)TRUE;

  // Make data source's items in context menu, main menu and accelerator-table.
  _makeDataSourcesMenus( hwnd );

  // Set main window position and size

  if ( hIni != NULLHANDLE &&
       PrfQueryProfileData( hIni, INI_APP, INI_KEY_WINDOW,
                            &alSizePos, &cbWinSizePos ) &&
       cbWinSizePos == sizeof(alSizePos) )
  {
    // Use stored position and size. Change of coordinates in according to
    // the dimensions of the screen if necessary.

    if ( alSizePos[2] >= ulCXScr )
      alSizePos[2] = ulCXScr;

    if ( alSizePos[3] >= ulCYScr )
      alSizePos[3] = ulCYScr;

    if ( alSizePos[0] < 0 )
      alSizePos[0] = 0;
    else if ( (alSizePos[0] + alSizePos[2]) >= ulCXScr )
      alSizePos[0] = ulCXScr - alSizePos[2] - 1;

    if ( alSizePos[1] < 0 )
      alSizePos[1] = 0;
    else if ( (alSizePos[1] + alSizePos[3]) >= ulCYScr )
      alSizePos[1] = ulCYScr - alSizePos[3] - 1;
  }
  else
  {
    // Use default position and size.

//    alSizePos[0] = (ulCXScr >> 2) + (ulCXScr >> 5);	// Left
//    alSizePos[2] = (ulCXScr >> 1) - (ulCXScr >> 4);	// Width
    alSizePos[2] = ulCXScr >> 1;			// Width
    alSizePos[0] = ulCXScr >> 2;			// Left
    alSizePos[1] = ulCYScr >> 2;			// Bottom
    alSizePos[3] = ulCYScr >> 1;			// Height
    alSizePos[4] = (ulCYScr >> 3) + (ulCYScr >> 4);	// Details win. height
  }

  // Set main window
  WinSetWindowPos( hwndFrame, HWND_TOP, alSizePos[0], alSizePos[1],
                   alSizePos[2], alSizePos[3], SWP_MOVE | SWP_SIZE );
  // Set height of details window
  WinSetWindowPos( WinQueryWindow( hwndDetails, QW_PARENT ), HWND_TOP,
                   0, 0, 10, alSizePos[4], SWP_SIZE );


  // Load last selected data source's module name from ini-file.
  utilQueryProfileStr( hIni, INI_APP, INI_KEY_MODULE, "", &szModule,
                       sizeof(szModule) );

  // Search for last selected data source or first initialized data source.
  for( ulIdx = 0; ulIdx < srclstGetCount(); ulIdx++ )
  {
    pDataSrc = srclstGetByIndex( ulIdx );

    if ( stricmp( &pDataSrc->szModule, &szModule ) == 0 )
    {
      pSelDataSrc = pDataSrc;
      break;
    }

    if ( pSelDataSrc == NULL && !pDataSrc->fDisable &&
         pDataSrc->fInitialized )
      pSelDataSrc = pDataSrc;
  }

  // Select founded data source.
  if ( pSelDataSrc != NULL )
    WinPostMsg( hwndList, WM_COMMAND, MPFROMLONG( pSelDataSrc->ulMenuItemId ),
                0 );

  return (MRESULT)FALSE; // Success code.
}

static VOID _wmDestroy(HWND hwnd)
{
  HACCEL		hAccel = WinQueryWindowULong( hwnd, 0 );
  SWP			swp;
  PDATASOURCE		pDataSrc = itemsGetDataSrc();
  LONG			alSizePos[5];

  if ( hAccel != NULLHANDLE )
  {
    debug( "Destroy accelerator table..." );
    WinDestroyAccelTable( hAccel );
  }

  debug( "Store selected module name, window position and size..." );

  // Store to the ini-file selected data source's module name.
  if ( pDataSrc != NULL )
    utilWriteProfileStr( hIni, INI_APP, INI_KEY_MODULE, pDataSrc->szModule );

  // Query window position and size.
  WinQueryWindowPos( WinQueryWindow( hwnd, QW_PARENT ), &swp );
  alSizePos[0] = swp.x;
  alSizePos[1] = swp.y;
  alSizePos[2] = swp.cx;
  alSizePos[3] = swp.cy;
  // Querye details window height.
  WinQueryWindowPos( WinWindowFromID( hwnd, IDWND_DETAILS ), &swp );
  alSizePos[4] = swp.cy;
  // Store data to the ini-file.
  utilWriteProfileData( hIni, INI_APP, INI_KEY_WINDOW, &alSizePos,
                       sizeof(alSizePos) );
}

static VOID _wmSize(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  SWP			swpDetails;
  SWP			swp;
  ULONG			ulCYSizeBorder = WinQuerySysValue( HWND_DESKTOP,
                                                           SV_CYSIZEBORDER );
  HWND			hwndDetails = WinWindowFromID( hwnd, IDWND_DETAILS );

  WinQueryWindowPos( hwnd, &swp );
  WinQueryWindowPos( hwndDetails, &swpDetails );

  // Details window should not be higher than not minimized main window.
  if ( // Compare height.
       swpDetails.cy > swp.cy &&
       // Check "minimized" state of the main window.
       ( WinQueryWindowULong( WinQueryWindow( hwnd, QW_PARENT ), QWL_STYLE )
         & WS_MINIMIZED ) == 0 )
  {
    swpDetails.cy = swp.cy;
  }

  // Position & width will be calculated at WM_ADJUSTFRAMEPOS
  // by DetailsFrameWndProc(). Keep details window height.
  WinSetWindowPos( hwndDetails, HWND_TOP, 0, 0, 100,
                   swpDetails.cy, SWP_SIZE | SWP_MOVE );

  WinSetWindowPos( WinWindowFromID( hwnd, IDWND_LIST ), HWND_TOP,
                   0, swpDetails.cy - ulCYSizeBorder,
                   swp.cx + WinQuerySysValue( HWND_DESKTOP, SV_CXBORDER ),
                   (swp.cy - swpDetails.cy) + ulCYSizeBorder,
                   SWP_MOVE | SWP_SIZE | SWP_ZORDER );
}


MRESULT EXPENTRY ClientWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_CREATE:
      return _wmCreate( hwnd, mp1, mp2 );

    case WM_DESTROY:
      _wmDestroy( hwnd );
      break;

    case WM_SIZE:
      _wmSize( hwnd, mp1, mp2 );
      return (MRESULT)FALSE;

    case WM_CLOSE:
      WinPostMsg( hwnd, WM_QUIT, 0, 0 );
      return (MRESULT)FALSE;

    case WM_ERASEBACKGROUND:
      return (MRESULT)TRUE; // TRUE - request PM to paint in SYSCLR_WINDOW

    case WM_SHOW:
      itemsSetVisible( (BOOL)SHORT1FROMMP(mp1) );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP(mp1) )
      {
        case IDM_EXIT:
          WinPostMsg( hwnd, WM_QUIT, 0, 0 );
          return (MRESULT)FALSE;

        case IDM_PROPERTIES:
          dlgProperties( hwnd );
          WinPostMsg( hwnd, WM_SL_UPDATE_LIST, 0, 0 );
          return (MRESULT)FALSE;

        case IDM_SETTINGS:
          dlgSettings( hwnd );
          _makeDataSourcesMenus( hwnd );
          return (MRESULT)FALSE;
      }

      // Forward all unknown commands to the list window.
      return WinSendMsg( WinWindowFromID( hwnd, IDWND_LIST ), WM_COMMAND,
                         mp1, mp2 );

    case WM_SL_DETAILSSIZE:
      {
        // Details window size changed - change list window size,

        ULONG		ulCYSizeBorder = WinQuerySysValue( HWND_DESKTOP,
                                                           SV_CYSIZEBORDER );
        ULONG		ulDetailHeight = SHORT2FROMMP(mp2) + ulCYSizeBorder;
        RECTL		rectlClient;

        WinQueryWindowRect( hwnd, &rectlClient );
        WinSetWindowPos( WinWindowFromID( hwnd, IDWND_LIST ), HWND_TOP, 0,
                         ulDetailHeight,
                         (rectlClient.xRight - rectlClient.xLeft) + WinQuerySysValue( HWND_DESKTOP, SV_CXBORDER ),
                         (rectlClient.yTop - rectlClient.yBottom) - ulDetailHeight,
                         SWP_MOVE | SWP_SIZE | SWP_SHOW );
      }
      return (MRESULT)FALSE;

    case WM_SL_UPDATE_LIST:
      // Update list.
      // mp1: not 0 - update data only
      WinSendMsg( WinWindowFromID( hwnd, IDWND_LIST ),
                  LONGFROMMP(mp1) == 0 ? WM_SL_SETDATASRC : WM_SL_UPDATE_DATA,
                  0, 0 );
      return (MRESULT)FALSE;

    case WM_SL_UPDATE_DETAILS:
      // Update details.
      WinSendMsg( WinWindowFromID( hwnd, IDWND_DETAILS ), WM_SL_UPDATE_DETAILS,
                  NULL, NULL );
      return (MRESULT)FALSE;

    case WM_SL_DETAILSACTIVATE:
      // Details window was activated. We transfer activation to list window.
      WinSetWindowPos( WinWindowFromID( hwnd, IDWND_LIST ), HWND_TOP,
                       0, 0, 0, 0, SWP_ACTIVATE );
      return (MRESULT)FALSE;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


HWND mwInstall(HWND hwndParent, ULONG ulId)
{
  HWND		hwnd, hwndFrame;
  ULONG		ulFrameFlags = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINBUTTON |
                               FCF_MAXBUTTON | FCF_SIZEBORDER | FCF_TASKLIST |
                               FCF_MENU | FCF_ICON /* | FCF_ACCELTABLE*/;
  HAB		hab = WinQueryAnchorBlock( hwndParent );
  CHAR		szBuf[256];

  // Register class WC_SL_MAIN for the main window. Window's data area:
  //   bytes 0-3 - accelerator-table handle
  if ( !WinRegisterClass( hab, WC_SL_MAIN, ClientWndProc, 0, 2 * sizeof(ULONG) ) )
  {
    debug( "WinRegisterClass() fail" );
    return NULLHANDLE;
  }

  // Load title string and create main window.
  WinLoadString( hab, 0, IDS_TITLE, sizeof(szBuf), &szBuf );
  hwndFrame = WinCreateStdWindow( HWND_DESKTOP, WS_VISIBLE | WS_CLIPCHILDREN,
                                  &ulFrameFlags, WC_SL_MAIN, &szBuf,
                                  0, 0, ID_RESOURCE, &hwnd );
  if ( hwndFrame == NULLHANDLE )
  {
    debug( "WinCreateStdWindow() fail" );
    return NULLHANDLE;
  }

  WinShowWindow( hwndFrame, TRUE );
  return hwnd;
}
