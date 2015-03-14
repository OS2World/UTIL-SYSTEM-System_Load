/*
  The sources contained here are designed  to  serve  as  a  template  for
  your System Load data source (plugin). These sources files  contain  the
  overhead routines necessary to create a plugin as well as stubs for  the
  basic routines that all plugins should have. If you compile and link the
  sample plugin now, it will create a dynamic link module that will placed
  in ..\..\bin.

  sample3:
    - Using helpers.
    - Sort.
    - Periodic data updates.
    - Properties dialog.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ds.h>
#include <sl.h>
#include "sample3.h"

#define UPDATE_INTERVAL_SLOW	1000	// msec
#define UPDATE_INTERVAL_FAST	100	// msec


// Data of item
typedef struct _smpl3item {
  CHAR		szName[16];
  ULONG		ulSpeed;
  ULONG		ulCounter;
} SMPL3ITEM, *PSMPL3ITEM;


// Sort fields. If there is just one sort field, its name will not appear in
// the menu, but sort will run through menu items Ascending and Descending.
static PSZ		apszSortBy[] =
{
  "Name",
  "Counter"
};

// Data source information.
static DSINFO stDSInfo =
{
  sizeof(DSINFO),			// Size of this data structure.
  "Sample ~3",				// Menu item title.
  sizeof(apszSortBy) / sizeof(PSZ),	// Number of "sort-by" strings.
  &apszSortBy,				// "Sort-by" strings.
  0,					// Flags DS_FL_*
  100,					// Items horisontal space, %Em
  100					// Items vertical space, %Em
};

static HMODULE		hDSModule;	// Module handle.
static PSMPL3ITEM	*ppItems;	// List of items.
static ULONG		cItems;		// Number of items.
static ULONG		ulInterval;	// Counters update interval.
static ULONG		ulSortBy;	// Sort field, index of apszSortBy[].
static BOOL		fSortDesc;	// Reverse sort order flag.

// Pointers to helper routines.

static PHiniWriteULong		piniWriteULong;
static PHiniReadULong		piniReadULong;
static PHupdateLock		pupdateLock;
static PHupdateUnlock		pupdateUnlock;


static VOID _addItem(PSZ pszName, ULONG ulSpeed)
{
  PSMPL3ITEM	pItem = malloc( sizeof(SMPL3ITEM) );

  if ( pItem == NULL )
    return;

  strlcpy( &pItem->szName, pszName, sizeof(pItem->szName) );
  pItem->ulSpeed = ulSpeed;
  pItem->ulCounter = 0;

  // Expand list for next new 16 items.
  if ( ( cItems == 0 ) || ( (cItems & 0x0000000F) == 0x0000000F ) )
  {
    PSMPL3ITEM	*ppNewItems;
    ULONG	ulNewSize = ( ( ( cItems + 1 ) & 0xFFFFFFF0 ) + 0x00000010 )
                            * sizeof(PSMPL3ITEM);

    ppNewItems = realloc( ppItems, ulNewSize );
    if ( ppNewItems == NULL )
    {
      free( pItem );
      return;
    }

    ppItems = ppNewItems;
  }

  // Store pointer to the new item.
  ppItems[cItems] = pItem;
  cItems++;
}

static int _sortComp(const void *p1, const void *p2)
{
  PSMPL3ITEM	pItem1 = *(PSMPL3ITEM *)p1;
  PSMPL3ITEM	pItem2 = *(PSMPL3ITEM *)p2;
  int		iRes;

  switch( ulSortBy )
  {
    case 0:		// 0: "Name"
      iRes = strcmp( &pItem1->szName, &pItem2->szName );
      break;

    default:		// 1: "Counter"
      iRes = pItem1->ulCounter - pItem2->ulCounter;
      break;
  }

  return fSortDesc ? -iRes : iRes;
}



// Interface routines of data source
// ---------------------------------


// PDSINFO dsInstall(HMODULE hMod, PSLINFO pSLInfo)
//
// This function is called once, immediately after module is loaded.
// hMod: Handle for the dynamic link module of data source.
// pSLInfo: Program information, see ..\..\sl\ds.h
// Returns pointer to the DSINFO structure.

DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  // Store module handle of data source.
  hDSModule = hMod;

  // Query pointers to helper routines.
  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  pupdateLock = (PHupdateLock)pSLInfo->slQueryHelper( "updateLock" );
  pupdateUnlock = (PHupdateUnlock)pSLInfo->slQueryHelper( "updateUnlock" );

  // Return data source information for main program
  return &stDSInfo;
}


// BOOL dsInit()
//
// This function is called after dsInstall() and every time when user turns
// data source (in "Settings").
// Returns TRUE if data source initialized successfully.

DSEXPORT BOOL APIENTRY dsInit()
{
  ppItems = NULL;
  cItems = 0;

  // Create items.

  _addItem( "Device D", 0 );
  _addItem( "Device E", 3 );
  _addItem( "Device F", 5 );
  _addItem( "Device A", 1 );
  _addItem( "Device B", 2 );
  _addItem( "Device C", 4 );

  // Data source properties.

  // Load update interval from ini-file.
  ulInterval = piniReadULong( hDSModule, "Fast", 0 ) == 1 ?
                 UPDATE_INTERVAL_FAST : UPDATE_INTERVAL_SLOW;

  // Load sort order from ini-file.
  ulSortBy = piniReadULong( hDSModule, "SortBy", 0 );
  fSortDesc = (BOOL)piniReadULong( hDSModule, "SortDesc", (LONG)FALSE );


  // Sort items.
  qsort( ppItems, cItems, sizeof(PSMPL3ITEM), _sortComp );

  return TRUE;
}


// VOID dsDone()
//
// This function is called when user turns off the data source or exits the
// program.

DSEXPORT VOID APIENTRY dsDone()
{
  ULONG		ulIdx;

  if ( ppItems != NULL )
  {
    // Free all items.
    for( ulIdx = 0; ulIdx < cItems; ulIdx++ )
      free( ppItems[ulIdx] );

    // Free list.
    free( ppItems );
  }
}


// ULONG dsGetCount()
//
// The program calls this function to query number of items.

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return cItems;
}


// ULONG dsGetUpdateDelay()
//
// The program calls this function to query time intervals for updates.
// This function must take a minimum of time. If your module uses threads
// you should use lock with helpers updateLock()/updateUnlock() to access the
// data which changes by thread and uses in this function (to avoid calls
// dsGetUpdateDelay() and dsUpdate()). Functions dsGetUpdateDelay() and
// dsUpdate() will not be called during execution of other module's interface
// functions, except dsLoadDlg().
// dsGetUpdateDelay() calls rather frequent, it allows you to change intervals
// dynamically.

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return ulInterval;
}


// ULONG dsUpdate(ULONG ulTime)
//
// Function will be called with an interval that is set dsGetUpdateDelay().
// This function must take a minimum of time. If your module uses threads
// you should use lock with helpers updateLock()/updateUnlock() to access the
// data which changes by thread and uses in this function (to avoid calls
// dsGetUpdateDelay() and dsUpdate()). Functions dsGetUpdateDelay() and
// dsUpdate() will not be called during execution of other module's interface
// functions, except dsLoadDlg().
// Return values:
//   DS_UPD_NONE - Displayed data is not changed.
//   DS_UPD_DATA - Displayed data is changed. Size of items windows, order and
//                 number of items have not changed. SL will update items and
//                 details.
//   DS_UPD_LIST - Size of items windows, order or number of items have been
//                 changed. SL will recreate items windows and update details.

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  ULONG		ulIdx;

  // Increment all counters.
  for( ulIdx = 0; ulIdx < cItems; ulIdx++ )
  {
    ppItems[ulIdx]->ulCounter += ppItems[ulIdx]->ulSpeed;
  }

  // Sort order is not changed after updates in this example. But in another
  // situation, we would need sort the list and return code DS_UPD_LIST if
  // order of items changed.
  return DS_UPD_DATA;
}


// VOID dsPaintItem(ULONG ulIndex, HPS hps, SIZEL sizeWnd)
//
// This function calls when a window of item needs repainting. Module MAY not
// paint over whole area. The background is painted already in the right color
// (for selected or not selected item).
// 
// ulIndex: Index of the item to be paint.
// hps: Presentation space handle.
// sizeWnd: Size of item's window.

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  PSMPL3ITEM	pItem = ppItems[ulIndex];
  CHAR		szBuf[128];
  LONG		cbBuf;
  POINTL	pt = { 5, 5 };

  cbBuf = sprintf( &szBuf, "%s: %u",
                   &pItem->szName, pItem->ulCounter );
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );
}


// ULONG dsSortBy(ULONG ulNewSort)
//
// This function is used to set new sort order and to query currently set
// sort order.
// ulNewSort: bits 0-31 - index of apszSortBy[].
//            bit 32 - reverse the sort order flag.
//            or DSSORT_QUERY - do not sort, request currently set sort order.
// Returns current/new sort order: sort field index (bits 0-31) and reverse
// sort order flag (bit 32).

DSEXPORT ULONG APIENTRY dsSortBy(ULONG ulNewSort)
{
  ULONG		ulResult;

  if ( ulNewSort != DSSORT_QUERY )
  {
    // Get field index of the array apszSortBy[].
    ulSortBy = ulNewSort & DSSORT_VALUE_MASK;

    // Get reverse the sort order flag.
    fSortDesc = (ulNewSort & DSSORT_FL_DESCENDING) != 0;

    // Sort records.
    qsort( ppItems, cItems, sizeof(PSMPL3ITEM), _sortComp );

    // Store new sort order to the ini-file
    piniWriteULong( hDSModule, "SortBy", ulSortBy );
    piniWriteULong( hDSModule, "SortDesc", fSortDesc );
  }

  ulResult = ulSortBy;
  if ( fSortDesc )
    ulResult |= DSSORT_FL_DESCENDING;

  return ulResult;
}


//	Properties dialog
//	-----------------

// ULONG dsLoadDlg(HWND hwndOwner, PHWND pahWnd)
//
// Module should fill array pointed by pahWnd with handles of dialogs which
// will be added to the properties notebook. Up to eight dialogs can be used.
// Returns number of dialogs.
// This is thread-unsafe function, so you need to use helpers updateLock() and
// updateUnlock() to avoid calling dsUpdate() during data changes.

static ULONG	ulSaveInterval;

static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      // Save current value for undo.
      ulSaveInterval = ulInterval;
      // Go to WM_SL_UNDO to set controls state:

    case WM_SL_UNDO:
      // Button "Undo" clicked.

      ulInterval = ulSaveInterval;
      WinSendDlgItemMsg( hwnd,
                         ulSaveInterval == UPDATE_INTERVAL_FAST ?
                           IDD_RB_FAST : IDD_RB_SLOW,
                         BM_SETCHECK, MPFROMSHORT( 1 ), 0 );
      return (MRESULT)FALSE;

    case WM_SL_DEFAULT:
      // Button "Default" clicked.

      WinSendDlgItemMsg( hwnd, IDD_RB_SLOW, BM_SETCHECK, MPFROMSHORT( 1 ), 0 );
      ulInterval = UPDATE_INTERVAL_SLOW;
      return (MRESULT)FALSE;

    case WM_DESTROY:
      // Dialog is closed. Store new value to the ini-file.

      piniWriteULong( hDSModule, "Fast",
                      ulInterval == UPDATE_INTERVAL_FAST ? 1 : 0 );
      break;

    case WM_COMMAND:
      return (MRESULT)FALSE;

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_RB_FAST:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED  )
          {
            // Radio button "Fast" clicked.
            pupdateLock();
            ulInterval = UPDATE_INTERVAL_FAST;
            pupdateUnlock();
          }
          break;

        case IDD_RB_SLOW:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED  )
          {
            // Radio button "Slow" clicked.
            pupdateLock();
            ulInterval = UPDATE_INTERVAL_SLOW;
            pupdateUnlock();
          }
          break;
      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

DSEXPORT ULONG APIENTRY dsLoadDlg(HWND hwndOwner, PHWND pahWnd)
{
  pahWnd[0] = WinLoadDlg( hwndOwner, hwndOwner, DialogProc,
                          hDSModule, IDD_DSPROPERTIES, NULL );

  return 1;
}
