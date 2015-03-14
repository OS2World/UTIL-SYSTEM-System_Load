/*
  The sources contained here are designed  to  serve  as  a  template  for
  your System Load data source (plugin). These sources files  contain  the
  overhead routines necessary to create a plugin as well as stubs for  the
  basic routines that all plugins should have. If you compile and link the
  sample plugin now, it will create a dynamic link module that will placed
  in ..\..\bin.

  sample2:
    - Selectable items.
    - Call helper function.
    - Specifying size for item's window.
    - Drawing in the window of details.
    - Context menu.
    - Enter / double click for item.
*/

#include <string.h>
#include <stdio.h>
#include <ds.h>
#include <sl.h>		// constant IDM_DSCMD_FIRST_ID

// Data source information.
static DSINFO stDSInfo =
{
  sizeof(DSINFO),		// Size, in bytes, of this data structure.
  "Sample ~2",			// Menu item title.
  0,				// Number of "sort-by" strings.
  NULL,				// "Sort-by" strings.
  0,				// Flags DS_FL_*
  100,				// Items horisontal space, %Em
  100				// Items vertical space, %Em
};

static PSZ	apszItems[] =
{
  "Warp",
  "Merlin",
  "Aurora",
  "OS/2",
  "os4krnl",
  "eComStation"
};

// ulSelectedItem - stores the index of selected item.
static ULONG			ulSelectedItem = 0;
// putilGetTextSize - pointer to helper routine.
static PHutilGetTextSize	putilGetTextSize;
// ulSelectedItem - index of item for which the context menu was builted.
static ULONG			ulCtxMenuItem;



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
  // Query pointer to helper routine.
  putilGetTextSize = (PHutilGetTextSize)pSLInfo->slQueryHelper( "utilGetTextSize" );

  // Return data source information for main program
  return &stDSInfo;
}


// BOOL dsInit()
//
// This function is called after dsInstall() and every time when user turns
// data source (in "Settings").

DSEXPORT BOOL APIENTRY dsInit()
{
  return TRUE;
}


// VOID dsDone()
//
// This function is called when user turns off the data source or exits the
// program.

DSEXPORT VOID APIENTRY dsDone()
{
}


// ULONG dsGetCount()
//
// The program calls this function to query number of items.

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return sizeof(apszItems) / sizeof(PSZ);
}


// BOOL dsSetSel(ULONG ulIndex)
//
// This function is called when the user selects one of the items.
// ulIndex: Selected item.
// Returns TRUE if the given item is selectable.

DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex)
{
  ulSelectedItem = ulIndex;

  return TRUE;
}


// ULONG dsGetSel()
//
// The program calls this function to query currently selected item.
// You can use only one of the functions of dsGetSel() and dsIsSelected().
// However, it is recommended to use both functions for optimization.

DSEXPORT ULONG APIENTRY dsGetSel()
{
  return ulSelectedItem;
}


// BOOL dsIsSelected(ULONG ulIndex)
//
// The function should return TRUE if ulIndex is the index of selected item.

BOOL APIENTRY dsIsSelected(ULONG ulIndex)
{
  return ulIndex == ulSelectedItem;
}


// VOID dsSetWndStart(HPS hps, PSIZEL pSize)
//
// The program calls this function before creating windows for items, to query
// size for all item's windows.

DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  ULONG		ulIdx;
  SIZEL		sizeText;

  // Scan all items and find largest size for the item.

  pSize->cx = 0;
  pSize->cy = 0;
  for( ulIdx = 0; ulIdx < dsGetCount(); ulIdx++ )
  {
    // Use a helper function to determine size of item's text.
    putilGetTextSize( hps, strlen( apszItems[ulIdx] ), apszItems[ulIdx],
                      &sizeText );

    if ( pSize->cx < sizeText.cx )
      pSize->cx = sizeText.cx;
    if ( pSize->cy < sizeText.cy )
      pSize->cy = sizeText.cy;
  }

  // Add a small margins around the text.
  pSize->cx += 30; 
  pSize->cy += 20;
}


// VOID dsSetWnd(ULONG ulIndex, HWND hwnd, HPS hps)
//
// This function is called immediately after creating an item's window.
// Here, we change the size of one of the items for example.

DSEXPORT VOID APIENTRY dsSetWnd(ULONG ulIndex, HWND hwnd, HPS hps)
{
  SIZEL		sizeText;

  if ( ulIndex != 3 )
    return;

  // Crop window of 3th item.
  putilGetTextSize( hps, strlen( apszItems[ulIndex] ), apszItems[ulIndex],
                    &sizeText );
  WinSetWindowPos( hwnd, HWND_TOP, 0, 0, sizeText.cx + 30, sizeText.cy + 20,
                   SWP_SIZE );
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

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, SIZEL sizeWnd)
{
  POINTL	pt = { 15, 10 };

  GpiCharStringAt( hps, &pt, strlen( apszItems[ulIndex] ), apszItems[ulIndex] );
}


// VOID dsPaintDetails(HPS hps, SIZEL sizeWnd)
//
// This function calls when a window of details needs repainting.

DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  POINTL	pt;
  CHAR		szBuf[256];
  ULONG		cbBuf;
  SIZEL		sizeText;

  cbBuf = sprintf( &szBuf, "Index: %u, String: %s",
                   ulSelectedItem, apszItems[ulSelectedItem] );

  // Use a helper function to determine size of text in window's presentation
  // space.
  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );

  // Write text at the center of window.
  pt.x = ( psizeWnd->cx - sizeText.cx ) / 2;
  pt.y = ( psizeWnd->cy - sizeText.cy ) / 2;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );
}


// VOID dsFillMenu(HWND hwndMenu, ULONG ulIndex)
//
// This function is called before displaying the context menu. Here, the data
// source module can add own menu items.
// 
// hwndMenu: Window handle of menu.
// ulIndex:  Index of item or DS_SEL_NONE if menu created not for any of the
//           items.

DSEXPORT VOID APIENTRY dsFillMenu(HWND hwndMenu, ULONG ulIndex)
{
  MENUITEM		stMINew = { 0 };
  CHAR			szBuf[64];

  // Separator
  stMINew.iPosition = MIT_END;
  stMINew.afStyle = MIS_SEPARATOR;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );

  // First menu item - always present.

  // Command IDs from IDM_DSCMD_FIRST_ID (20000) to IDM_DSCMD_LAST_ID
  // (29999) can be used by data source. These commands will cause function
  // dsCommand() call.
  stMINew.id = IDM_DSCMD_FIRST_ID;
  stMINew.afStyle = MIS_TEXT;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ),
              MPFROMP( "Command for data source" ) );

  // Second menu item - only when menu called for the item of data source.

  if ( ulIndex != DS_SEL_NONE )
  {
    sprintf( szBuf, "Command for \"%s\"", apszItems[ulIndex] );
    stMINew.afStyle = MIS_TEXT;
    stMINew.id = IDM_DSCMD_FIRST_ID + 1;
    WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ),
                MPFROMP( &szBuf ) );

    // Store index of data source's item.
    ulCtxMenuItem = ulIndex;
  }
}


// ULONG dsCommand(HWND hwndOwner, USHORT usCommand)
//
// This function is called when user selects one of the items of context menu
// that has command id IDM_DSCMD_FIRST_ID..IDM_DSCMD_LAST_ID.
// Return values:
//   DS_UPD_NONE - Displayed data is not changed.
//   DS_UPD_DATA - Displayed data is changed. Size of items windows, order and
//                 number of items have not changed. SL will update items and
//                 details.
//   DS_UPD_LIST - Size of items windows, order or number of items have been
//                 changed. SL will recreate items windows and update details.

DSEXPORT ULONG APIENTRY dsCommand(HWND hwndOwner, USHORT usCommand)
{
  CHAR		szMsg[255];

  switch( usCommand )
  {
    case IDM_DSCMD_FIRST_ID:
      strcpy( &szMsg, "Command for data source" );
      break;

    case IDM_DSCMD_FIRST_ID + 1:
      sprintf( &szMsg, "Command for \"%s\"", apszItems[ulCtxMenuItem] );
      break;

    default: 
      return DS_UPD_NONE;
  }

  WinMessageBox( HWND_DESKTOP, hwndOwner, &szMsg, "Context menu", 1,
                 MB_MOVEABLE | MB_INFORMATION | MB_OK );
  return DS_UPD_NONE;
}


// ULONG dsEnter(HWND hwndOwner)
//
// The program calls this function when Enter is pressed or double click on
// item.
// Return values: same as for dsCommand().

DSEXPORT ULONG APIENTRY dsEnter(HWND hwndOwner)
{
  CHAR		szMsg[255];

  sprintf( &szMsg, "Enter: \"%s\"", apszItems[ulSelectedItem] );

  WinMessageBox( HWND_DESKTOP, hwndOwner, &szMsg, "Enter / double click", 1,
                 MB_MOVEABLE | MB_INFORMATION | MB_OK );
  return DS_UPD_NONE;
}
