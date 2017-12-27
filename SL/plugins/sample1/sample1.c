/*
  The sources contained here are designed  to  serve  as  a  template  for
  your System Load data source (plugin). These sources files  contain  the
  overhead routines necessary to create a plugin as well as stubs for  the
  basic routines that all plugins should have. If you compile and link the
  sample plugin now, it will create a dynamic link module that will placed
  in ..\..\bin.

  sample1:
    Implementation of the basic required interface functions for module.
*/

#include <string.h>
#include <ds.h>

// Data source information.
static DSINFO stDSInfo =
{
  sizeof(DSINFO),		// Size, in bytes, of this data structure.
  "Sample ~1",			// Menu item title.
  0,				// Number of "sort-by" strings.
  NULL,				// "Sort-by" strings.
  0,				// Flags DS_FL_* (see ds.h)
  100,				// Items horisontal space, %Em
  100,				// Items vertical space, %Em
  0				// Help main panel index.
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
  POINTL	pt = { 5, 5 };

  GpiCharStringAt( hps, &pt, strlen( apszItems[ulIndex] ), apszItems[ulIndex] );
}
