/*
    Manage windows of data source items.
    Data refresh timer thread.
*/

#include <stdio.h>
#include <string.h>
#define INCL_DOSMISC		/* DOS Miscellaneous values  */
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS		/* Process and thread values */
#define INCL_DOSERRORS		/* DOS error values          */
#include "items.h"
#include "srclist.h"
#include "ds.h"
#include "utils.h"
#include "hint.h"
#include "debug.h"     // Must be the last.

#define WC_SL_ITEM               "SLItem"
#define ITEM_MAX_WIDTH           500
#define ITEM_MAX_HEIGHT          100

// WM_SL_SELECTITEM - Internal items message.
// An update of the items if it is equal to mp1 or mp2.
#define WM_SL_SELECTITEM         ( WM_USER + 1 )


extern HWND  hwndMain; // sl.c

// Current (visible) data source
static PDATASOURCE     pCurrentDataSrc = NULL;

#define UPDATE_THREAD_STACK	65535
static TID   tidUpdate  = 0;
static HEV   hevStop    = NULLHANDLE;
static HPS   hpsMem     = NULLHANDLE;
static BOOL  fVisible   = TRUE;
// Share mutex sem. hmtxUpdate to lock/unlock update thread from datasoures
// (via data source's helpers functions in srclist.c).
HMTX         hmtxUpdate = NULLHANDLE;

// Data for item's window creation.
typedef struct _ITEMWNDDATA {
  USHORT               cbSize;
  PDATASOURCE          pDataSrc;
  ULONG                ulItem;
} ITEMWNDDATA, *PITEMWNDDATA;

static ULONG _getSel(PDATASOURCE pDataSrc)
{
  ULONG		ulIdx;

  if ( pDataSrc->fnGetSel != NULL )
    return pCurrentDataSrc->fnGetSel();

  if ( pDataSrc->fnIsSelected != NULL )
  {
    for( ulIdx = 0; ulIdx < pDataSrc->fnGetCount(); ulIdx++ )
      if ( pDataSrc->fnIsSelected( ulIdx ) )
        return ulIdx;
  }

  return DS_SEL_NONE;
}

static BOOL _isSelected(PDATASOURCE pDataSrc, ULONG ulIndex)
{
  if ( !pCurrentDataSrc->fInitialized )
    return FALSE;

  if ( pDataSrc->fnIsSelected != NULL )
    return pDataSrc->fnIsSelected( ulIndex );

  return pDataSrc->fnGetSel == NULL ? FALSE : (pDataSrc->fnGetSel() == ulIndex);
}

// _itemsUpdate() calls data source's function for update, then sends commands
// to update the windows. This function is called from thread updateThread().

static VOID _itemsUpdate(PDATASOURCE pDataSrc)
{
  ULONG      ulRC;

  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &pDataSrc->ulLastUpdate,
                   sizeof(ULONG) );

  if ( pDataSrc->fnUpdate == NULL )
    return;

  // Module with flag DS_FL_NO_BG_UPDATES should not be updated until it is
  // invisible. But we must record the fact that the missing updates to force
  // update when the module becomes visible.
  if ( ( (pDataSrc->pDSInfo->ulFlags & DS_FL_NO_BG_UPDATES) != 0 )
       && ( ( pCurrentDataSrc != pDataSrc ) || !fVisible ) )
  {
    pDataSrc->fOverdueUpdate = TRUE;
    return;
  }

  ulRC = pDataSrc->fnUpdate( pDataSrc->ulLastUpdate );

  if ( ulRC != DS_UPD_NONE && pDataSrc == pCurrentDataSrc )
  {
    WinPostMsg( hwndMain, WM_SL_UPDATE_LIST,
                MPFROMLONG( ulRC == DS_UPD_DATA ), 0 );
    WinPostMsg( hwndMain, WM_SL_UPDATE_DETAILS, NULL, NULL );
  }
}

// updateThread(). This thread monitors the intervals of time and update
// data sources.

VOID APIENTRY updateThread(ULONG ulData)
{
  ULONG                ulIdx, ulTime, ulUpdateDelay, ulRC;
  PDATASOURCE          pDataSrc;
  LONG                 lTimeRemains;

  ulRC = DosSetPriority( PRTYS_THREAD, PRTYC_REGULAR, 0, 0 );
  if ( ulRC != NO_ERROR )
    debug( "Warning! DosSetPriority(), rc = %u" );
  else
    ulRC = DosSetPriority( PRTYS_THREAD, 0, PRTYD_MAXIMUM, 0 );

  ulRC = DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  if ( ulRC != NO_ERROR )
    debug( "Warning! DosRequestMutexSem(), rc = %u" );

  for( ulIdx = 0; ulIdx < srclstGetCount(); ulIdx++ )
  {
    pDataSrc = srclstGetByIndex( ulIdx );
    if ( pDataSrc->fInitialized && pDataSrc->fnGetUpdateDelay != NULL )
      _itemsUpdate( pDataSrc );
  }

  DosReleaseMutexSem( hmtxUpdate );

  while( DosWaitEventSem( hevStop, 1 ) == ERROR_TIMEOUT )
  {
    DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulTime, sizeof(ULONG) );
    ulRC = DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
    if ( ulRC != NO_ERROR )
      debug( "Warning! DosRequestMutexSem(), rc = %u" );

    for( ulIdx = 0; ulIdx < srclstGetCount(); ulIdx++ )
    {
      pDataSrc = srclstGetByIndex( ulIdx );
      if ( !pDataSrc->fInitialized || pDataSrc->fnGetUpdateDelay == NULL )
        continue;

      if ( pDataSrc->fForceUpdate )
      {
        pDataSrc->fForceUpdate = FALSE;
        lTimeRemains = 0;
      }
      else
      {
        ulUpdateDelay = pDataSrc->fnGetUpdateDelay();
        if ( ulUpdateDelay == 0 )
          continue;

        lTimeRemains = (LONG)( (pDataSrc->ulLastUpdate + ulUpdateDelay) - ulTime );
        if ( lTimeRemains > 0 )
          continue;
      }

      _itemsUpdate( pDataSrc );
//      pDataSrc->ulLastUpdate = ulTime;
      pDataSrc->ulLastUpdate += lTimeRemains;	// Correct by interval's error.
    }  // for( ulIdx ...

    DosReleaseMutexSem( hmtxUpdate );
  }  // while( DosWaitEventSem( hevStop, 1 ) == ERROR_TIMEOUT )

  debug( "Exit" );
}

// ItemWndProc(). Item's window procedure.

MRESULT EXPENTRY ItemWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  PDATASOURCE          pDataSrc = (PDATASOURCE)WinQueryWindowPtr( hwnd, 0 );
  ULONG                ulItem = WinQueryWindowULong( hwnd, 4 );

  switch( msg )
  {
    case WM_CREATE:
      {
        PITEMWNDDATA   pWndData = (PITEMWNDDATA)mp1;
        PSZ            pszFont = NULL;

        pDataSrc = pWndData->pDataSrc;

        // Store data source of item and item's index
        WinSetWindowPtr( hwnd, 0, pDataSrc );
        WinSetWindowULong( hwnd, 4, pWndData->ulItem );

        // Set font in item window's presentation parameter for:
        // 1. prevent receiving messages WM_PRESPARAMCHANGED when font
        //    changed by user in the parent window,
        // 2. use data source's font if memory PS not available.
        if ( pDataSrc->pszFont != NULL )
          pszFont = pDataSrc->pszFont;
        if ( pszFont == NULL )
          pszFont = utilGetDefaultFont( NULL );
        WinSetPresParam( hwnd, PP_FONTNAMESIZE, strlen(pszFont) + 1, pszFont );

        // Set some colors to prevent receiving messages WM_PRESPARAMCHANGED
        // when color changed by user in the parent window,
        WinSetPresParam( hwnd, PP_BACKGROUNDCOLOR, sizeof(LONG),
                         &pDataSrc->lItemBackCol );
        WinSetPresParam( hwnd, PP_FOREGROUNDCOLOR, sizeof(LONG),
                         &pDataSrc->lItemForeCol );
      }
      return (MRESULT)FALSE;

    case WM_DESTROY:
      if ( WinQueryCapture( HWND_DESKTOP ) == hwnd )
      {
         // Release the pointing device capture, tune off hint.
         WinSetCapture( HWND_DESKTOP, NULLHANDLE );
         WinSendMsg( LONGFROMMR(WinSendMsg( WinQueryWindow( hwnd, QW_PARENT ),
                                            WM_SL_QUERY,
                                            MPFROMLONG(SLQUERY_HINT), 0 )),
                     WM_HINT_UNSET, MPFROMHWND(hwnd), 0 );
      }
      break;

    case WM_PAINT:
      {
        HPS            hps, hpsPaint;
        RECTL          rectWnd;
        POINTL         aPoints[3];
        BOOL           fSelected;
        LONG           lBkgColor;
        SIZEL          sizeWnd;

        hps = WinBeginPaint( hwnd, 0, NULL );
        // Choose PS for painting. If memory PS not available - window's PS
        // will be used to paint directly.
        hpsPaint = hpsMem == NULLHANDLE ? hps : hpsMem;

        WinQueryWindowRect( hwnd, &rectWnd );
        sizeWnd.cx = rectWnd.xRight - rectWnd.xLeft;
        sizeWnd.cy = rectWnd.yTop - rectWnd.yBottom;

        GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
        GpiCreateLogColorTable( hpsMem, 0, LCOLF_RGB, 0, 0, NULL );
        GpiSetBackMix( hpsPaint, BM_OVERPAINT );
        GpiSetMix( hpsPaint, BM_OVERPAINT );

        if ( DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT ) != NO_ERROR )
        {
          debug( "DosRequestMutexSem() fail" );
          WinEndPaint( hps );
          return (MRESULT)FALSE;
        }

        // Fill background and set current color and background color.
        // The colors depend upon is it selected item or not.

        fSelected = _isSelected( pDataSrc, ulItem );

        if ( fSelected )
        {
          lBkgColor = (pDataSrc->pDSInfo->ulFlags & DS_FL_SELITEM_BG_LIST) != 0 ?
                      pDataSrc->lListBackCol : pDataSrc->lItemHlBackCol;
        }
        else
        {
          lBkgColor = (pDataSrc->pDSInfo->ulFlags & DS_FL_ITEM_BG_LIST) != 0 ?
                      pDataSrc->lListBackCol : pDataSrc->lItemBackCol;
        }

        WinFillRect( hpsPaint, &rectWnd, lBkgColor );
        GpiSetBackColor( hpsPaint, GpiQueryRGBColor( hpsPaint, 0, lBkgColor ) );
        GpiSetColor( hpsPaint, GpiQueryRGBColor( hpsPaint, 0, 
                                        fSelected ? pDataSrc->lItemHlForeCol :
                                                    pDataSrc->lItemForeCol ) );

        // Lets data source to paint Presentation Space
        if ( pDataSrc->fnPaintItem != NULL )
          pDataSrc->fnPaintItem( ulItem, hpsPaint, &sizeWnd );

        DosReleaseMutexSem( hmtxUpdate );

        // Copy result image to the window's presentation space
        if ( hpsMem != NULLHANDLE )
        {
          aPoints[0].x = 0;
          aPoints[0].y = 0;
          aPoints[1].x = rectWnd.xRight - rectWnd.xLeft;
          aPoints[1].y = rectWnd.yTop - rectWnd.yBottom;
          aPoints[2].x = 0;
          aPoints[2].y = 0;
          GpiBitBlt( hps, hpsMem, 3, aPoints, ROP_SRCCOPY, 0 );
        }

        WinEndPaint( hps );
      }
      return (MRESULT)FALSE;

    case WM_CLOSE:
      WinDestroyWindow( hwnd );
      return (MRESULT)FALSE;

    case WM_CONTEXTMENU:
    {
      // Send WM_SL_CONTEXTMENU message to the parent window to show
      // context menu for data source item.
      HWND    hwndParent = LONGFROMMR( WinQueryWindow( hwnd, QW_PARENT ) );
      POINTL  ptMap;

      if ( SHORT2FROMMP( mp2 ) == 0 )
      {
        // Message contains coordinates. We need to recalculate it for parent
        // window.
        ptMap.x = ((PPOINTS)&mp1)->x;
        ptMap.y = ((PPOINTS)&mp1)->y;
      }
      else
      {
        ptMap.x = 0;
        ptMap.y = 0;
      }

      WinMapWindowPoints( hwnd, hwndParent, &ptMap, 1 );
      ((PPOINTS)&mp1)->x = ptMap.x;
      ((PPOINTS)&mp1)->y = ptMap.y;

      return WinSendMsg( hwndParent, WM_SL_CONTEXTMENU, mp1,
                         MPFROMLONG(ulItem) );
    }

    case WM_SL_UPDATE_DATA:
    {
      HWND   hwndHint = (HWND)WinSendMsg( WinQueryWindow( hwnd, QW_PARENT ),
                                    WM_SL_QUERY, MPFROMLONG(SLQUERY_HINT), 0 );

      WinInvalidateRect( hwnd, NULL, FALSE );
      WinSendMsg( hwndHint, WM_HINT_UPDATE, MPFROMHWND(hwnd), 0 );

      return (MRESULT)FALSE;
    }

    case WM_SL_SELECTITEM:
      if ( LONGFROMMP(mp1) == ulItem || LONGFROMMP(mp2) == ulItem )
        WinInvalidateRect( hwnd, NULL, FALSE );
      return (MRESULT)FALSE;

    case WM_BUTTON1DOWN:
      itemsSelect( WinQueryWindow( hwnd, QW_PARENT ), TRUE, ulItem );
      break;

    case WM_BUTTON1DBLCLK:
      {
        BOOL fSelected;

        DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
        fSelected = _isSelected( pDataSrc, ulItem );
        DosReleaseMutexSem( hmtxUpdate );

        if ( fSelected )
          itemsEnter( WinQueryWindow( hwnd, QW_PARENT ) );
      }
      return (MRESULT)FALSE;

    case WM_PRESPARAMCHANGED:
      // Check visibility state of a window to prevent 
      // reactions when WinSetPresParam() called from WM_CREATE.
      if ( !WinIsWindowVisible( hwnd ) )
        break;

      switch( LONGFROMMP( mp1 ) )
      {
        case PP_FONTNAMESIZE:
          {
            HWND       hwndParent = WinQueryWindow( hwnd, QW_PARENT );
            CHAR       szBuf[128];

            WinQueryPresParam( hwnd, PP_FONTNAMESIZE, 0, NULL,
                               sizeof(szBuf), &szBuf, QPF_NOINHERIT );
            WinSetPresParam( hwndParent, PP_FONTNAMESIZE,
                             strlen( &szBuf ) + 1, &szBuf );
          }
          return (MRESULT)FALSE;

        case PP_BACKGROUNDCOLOR:
        case PP_FOREGROUNDCOLOR:
          {
            LONG       lColor;
            BOOL       fSelected;
            ULONG      ulColorType;

            DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
            fSelected = _isSelected( pDataSrc, ulItem );
            DosReleaseMutexSem( hmtxUpdate );

            ulColorType = LONGFROMMP( mp1 ) == PP_BACKGROUNDCOLOR ?
                              ( fSelected ? SLCT_ITEMHLBACK : SLCT_ITEMBACK ) :
                              ( fSelected ? SLCT_ITEMHLFORE : SLCT_ITEMFORE );

            WinQueryPresParam( hwnd, LONGFROMMP( mp1 ), 0, NULL,
                               sizeof(LONG), &lColor, QPF_NOINHERIT );

            // Store new color for data source
            srclstSetColor( pDataSrc, ulColorType, lColor );
            // Update list window (items windows interior only)
            WinSendMsg( WinQueryWindow(hwnd, QW_PARENT), WM_SL_UPDATE_DATA, 0, 0 );
          }
          return (MRESULT)FALSE;
      }
      break;

    case WM_MOUSEMOVE:
      if ( pDataSrc->fnGetHintText != NULL )
      {
        HWND  hwndParent = WinQueryWindow( hwnd, QW_PARENT );
        HWND  hwndHint = LONGFROMMR( WinSendMsg( hwndParent,
                                     WM_SL_QUERY, MPFROMLONG(SLQUERY_HINT), 0 ) );

        WinSendMsg( hwndHint, WM_HINT_SET, MPFROMHWND( hwnd ), 0 );
      }
      break;

    case WM_HINT_GETTEXT:
      if ( pDataSrc->fnGetHintText != NULL )
      {
        DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
        pDataSrc->fnGetHintText( ulItem, PVOIDFROMMP(mp2), LONGFROMMP(mp1) );
        DosReleaseMutexSem( hmtxUpdate );
      }
      return (MRESULT)FALSE;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


BOOL itemsInit(HAB hab)
{
  HDC        hdcMem;
  ULONG      ulRC;
  HPS        hps;
  SIZEL      size = { 0, 0 };

  // Register window class for items
  if ( !WinRegisterClass( hab, WC_SL_ITEM, ItemWndProc, CS_SAVEBITS,
                           2 * sizeof(ULONG) // data source ptr, item index
                         ) )
    return FALSE;

  // Create item's memory presentation space

  hps = WinGetPS( hwndMain );
  hdcMem = DevOpenDC( hab, OD_MEMORY, "*", 0, NULL, hps );
  WinReleasePS( hps );
  if ( hdcMem == NULLHANDLE )
    return FALSE;

  hpsMem = GpiCreatePS( hab, hdcMem, &size,
                        PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC );
  if ( hpsMem == NULLHANDLE )
  {
    debugCP( "Warning! Memory PS was not created" );
    DevCloseDC( hdcMem );
    hdcMem = NULLHANDLE;
  }
  else
    // Switch HPS into RGB mode
    GpiCreateLogColorTable( hpsMem, 0, LCOLF_RGB, 0, 0, NULL );

  do
  {
    // Create update mutex semaphore
    ulRC = DosCreateMutexSem( NULL, &hmtxUpdate, 0, FALSE );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosCreateMutexSem(), rc = %u", ulRC );
      break;
    }

    // Create an event Stop semaphore
    ulRC = DosCreateEventSem( NULL, &hevStop, 0, FALSE );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosCreateEventSem(), rc = %u", ulRC );
      break;
    }

    // Start the thread for data updates
    ulRC = DosCreateThread( &tidUpdate, updateThread, 0, CREATE_READY,
                            UPDATE_THREAD_STACK );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosCreateThread(), rc = %u", ulRC );
      break;
    }

    return TRUE;
  }
  while( FALSE );

  if ( hevStop != NULLHANDLE )
    DosCloseEventSem( hevStop );
  if ( hmtxUpdate != NULLHANDLE )
    DosCloseMutexSem( hmtxUpdate );
  if ( hdcMem != NULLHANDLE )
    DevCloseDC( hdcMem );

  return FALSE;
}

VOID itemsDone()
{
  ULONG      ulRC;

  if ( hevStop != NULLHANDLE )
  {
    if ( tidUpdate != 0 )
    {
      debug( "Wait thread %u...", tidUpdate );
      ulRC = DosPostEventSem( hevStop );
      if ( ulRC != NO_ERROR )
        debug( "DosPostEventSem(), rc = %u", ulRC );

      ulRC = DosWaitThread( &tidUpdate, DCWW_WAIT );
      if ( ulRC != NO_ERROR )
        debug( "DosWaitThread(), rc = %u", ulRC );
    }
    DosCloseEventSem( hevStop );
  }

  if ( hmtxUpdate != NULLHANDLE )
    DosCloseMutexSem( hmtxUpdate );

  if ( hpsMem != NULLHANDLE )
  {
    HDC      hdcMem = GpiQueryDevice( hpsMem );
    HBITMAP  hbmMem = GpiSetBitmap( hpsMem, NULLHANDLE );

    if ( hbmMem != NULLHANDLE )
      GpiDeleteBitmap( hbmMem );
    GpiDestroyPS( hpsMem );
    DevCloseDC( hdcMem );
  }
}

VOID itemsSetDataSrc(HWND hwndList, PDATASOURCE pDataSrc)
{
  HWND                 hwndItem;
  ITEMWNDDATA          sWndData; // Passed to the new window with WM_CREATE
  BITMAPINFOHEADER2    bmi = { 0 };
  SWP                  swpItem;
  HBITMAP              hbmMem;
  HPS                  hpsList, hpsItems;
  SIZEL                sizeItem = { 300, 40 };
  LONG                 alFormats[4];

  // Tune off hint window
  WinSendMsg( LONGFROMMR( WinSendMsg( hwndList, WM_SL_QUERY,
                                      MPFROMLONG(SLQUERY_HINT), 0 ) ),
              WM_HINT_UNSET, 0, 0 );

  // Remove all items windows
  WinBroadcastMsg( hwndList, WM_CLOSE, 0, 0, BMSG_SEND );

  if ( pDataSrc == NULL )
    return;

  // Set data source's font in list window
  // Set pCurrentDataSrc=NULL to prevent updates via list window
  pCurrentDataSrc = NULL;
  if ( pDataSrc->pszFont != NULL )
    WinSetPresParam( hwndList, PP_FONTNAMESIZE,
                     strlen( pDataSrc->pszFont ) + 1, pDataSrc->pszFont );

  hpsList = WinGetPS( hwndList );

  utilGetTextSize( hpsList, 2, "Em", &pDataSrc->sizeEm );

  if ( hpsMem != NULLHANDLE )
  {
    // Copy font from list window's PS to the memory PS.
    utilSetFontFromPS( hpsMem, hpsList, 1 );
    hpsItems = hpsMem;
  }
  else
    hpsItems = hpsList;

  // Set new current data source
  pCurrentDataSrc = pDataSrc;

  if ( pCurrentDataSrc->fInitialized )
  {
    DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );

    if ( pDataSrc->fOverdueUpdate )
    {
      // Force update if have been skipped for a module that has flag
      // DS_FL_NO_BG_UPDATES.
      pDataSrc->fOverdueUpdate = FALSE;
      pDataSrc->fForceUpdate = TRUE;
    }

    if ( pDataSrc->fnSetWndStart != NULL )
      pDataSrc->fnSetWndStart( hpsItems, &sizeItem );

    sWndData.cbSize  = sizeof(ITEMWNDDATA);
    sWndData.pDataSrc = pDataSrc;

    for( sWndData.ulItem = 0; sWndData.ulItem < pDataSrc->fnGetCount();
         sWndData.ulItem++ )
    {
      hwndItem = WinCreateWindow( hwndList, WC_SL_ITEM, NULL,
                                  WS_VISIBLE | WS_SAVEBITS | WS_TABSTOP,
                                  0, 0, sizeItem.cx, sizeItem.cy,
                                  hwndList,
                                  HWND_BOTTOM,
                                  0,
                                  &sWndData,
                                  NULL );

      if ( pDataSrc->fnSetWnd != NULL )
        pDataSrc->fnSetWnd( sWndData.ulItem, hwndItem, hpsItems );

      WinQueryWindowPos( hwndItem, &swpItem );
      if ( swpItem.cx > bmi.cx )
        bmi.cx = swpItem.cx;
      if ( swpItem.cy > bmi.cy )
        bmi.cy = swpItem.cy;
    }

    DosReleaseMutexSem( hmtxUpdate );

    // Create a (new) bitmap for memory presentation space.
    if ( hpsMem != NULLHANDLE )
    {
      // Request most closely formats of bitmaps matches the device.
      GpiQueryDeviceBitmapFormats( hpsList, 4, &alFormats );
      // Create a new bitmap
      bmi.cbFix         = sizeof(BITMAPINFOHEADER2);
      bmi.cPlanes       = (USHORT)alFormats[0]; //1;
      bmi.cBitCount     = (USHORT)alFormats[1]; //24;
      hbmMem = GpiCreateBitmap( hpsMem, &bmi, 0, NULL, NULL );
      if ( hbmMem != NULLHANDLE )
      {
        // Set new bitmap for the memory presentation space.
        hbmMem = GpiSetBitmap( hpsMem, hbmMem );
        // Delete previous bitmap
        if ( hbmMem != NULLHANDLE )
          GpiDeleteBitmap( hbmMem );
      }
    }

    WinReleasePS( hpsList );
  }  // if ( pCurrentDataSrc->fInitialized )

  WinSendMsg( hwndMain, WM_SL_UPDATE_DETAILS, NULL, NULL );
}

PDATASOURCE itemsGetDataSrc()
{
  return pCurrentDataSrc;
}

// VOID itemsSetVisible(BOOL fFlag)
//
// Informs that the application window is visible or invisible.

VOID itemsSetVisible(BOOL fFlag)
{
  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  if ( !fVisible && fFlag && pCurrentDataSrc->fOverdueUpdate )
  {
    // Force update if have been skipped for a module that has flag
    // DS_FL_NO_BG_UPDATES.
    pCurrentDataSrc->fOverdueUpdate = FALSE;
    pCurrentDataSrc->fForceUpdate = TRUE;
  }
  fVisible = fFlag;
  DosReleaseMutexSem( hmtxUpdate );
}

VOID itemsSelect(HWND hwndListClient, BOOL fAbsolute, LONG lItem)
{
  LONG       lOldSel, lNewSel;
  ULONG      cItems;
  HWND       hwndItem;
  SWP        swpList, swpItem;
  BOOL       fChanged = FALSE;

  if ( pCurrentDataSrc == NULL || !pCurrentDataSrc->fInitialized ||
       pCurrentDataSrc->fnSetSel == NULL )
    return;

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );

  cItems = pCurrentDataSrc->fnGetCount();
  if ( cItems == 0 )
  {
    DosReleaseMutexSem( hmtxUpdate );
    return;
  }

  lOldSel = _getSel( pCurrentDataSrc );

  // fAbsolute: Set new position.
  // !fAbsolute: Search new available position, skip not selectable items.

  lNewSel = fAbsolute ? lItem : ( lOldSel + lItem );

  if ( lNewSel < 0 )
  {
    lNewSel = 0;
    lItem = 1;
  }
  else if ( lNewSel >= cItems )
  {
    lNewSel = cItems - 1;
    lItem = -1;
  }
  else
    lItem = lItem < 0 ? -1 : 1;

  // Now lItem - "search direction": 1 / -1
  while( ( lNewSel != lOldSel ) && ( lNewSel >= 0 ) && ( lNewSel < cItems ) )
  {
    fChanged = pCurrentDataSrc->fnSetSel( lNewSel );
    if ( fChanged || fAbsolute )
      break;

    lNewSel += lItem;
  }

  DosReleaseMutexSem( hmtxUpdate );

  // Selected item not changed or cannot be selected.
  if ( !fChanged )
    return;

  // Update old and new selected items windows.
  WinBroadcastMsg( hwndListClient, WM_SL_SELECTITEM,
                   MPFROMLONG(lOldSel), MPFROMLONG(lNewSel), BMSG_SEND );

  // Search selected item window.
  for( hwndItem = WinQueryWindow( hwndListClient, QW_TOP );
       hwndItem != NULLHANDLE; hwndItem = WinQueryWindow( hwndItem, QW_NEXT ) )
  {
    if ( pCurrentDataSrc == (PDATASOURCE)WinQueryWindowPtr( hwndItem, 0 ) &&
         lNewSel == WinQueryWindowULong( hwndItem, 4 ) )
    {
      WinQueryWindowPos( hwndItem, &swpItem );
      WinQueryWindowPos( hwndListClient, &swpList );

      // Scroll list to selected item it if necessary.
      if ( swpItem.x < 0 || (swpItem.x + swpItem.cx) > swpList.cx )
      {
        SIZEL          sizeSpace;

        itemsGetSpace( &sizeSpace );

        WinSendMsg( hwndListClient, WM_HSCROLL, 0,
                    MPFROM2SHORT( (SHORT)(swpItem.x - sizeSpace.cx),
                    SB_SL_SLIDEROFFSET ) );
      }
      break;
    }
  }

  WinSendMsg( hwndMain, WM_SL_UPDATE_DETAILS, NULL, NULL );
}

VOID itemsGetSpace(PSIZEL psizeSpace)
{
  if ( pCurrentDataSrc == NULL )
  {
    psizeSpace->cx = 0;
    psizeSpace->cy = 0;
    return;
  }

  psizeSpace->cx =
    ( pCurrentDataSrc->sizeEm.cx * pCurrentDataSrc->pDSInfo->ulHorSpace ) / 100;
  psizeSpace->cy =
    ( pCurrentDataSrc->sizeEm.cy * pCurrentDataSrc->pDSInfo->ulVertSpace ) / 100;
}

VOID itemsPaintDetails(HPS hps, SIZEL sizeWnd)
{
  if ( pCurrentDataSrc == NULL || !pCurrentDataSrc->fInitialized ||
       pCurrentDataSrc->fnPaintDetails == NULL )
    return;

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  pCurrentDataSrc->fnPaintDetails( hps, &sizeWnd );
  DosReleaseMutexSem( hmtxUpdate );
}

VOID itemsFillContextMenu(HWND hwndMenu, ULONG ulItem)
{
  if ( pCurrentDataSrc == NULL || !pCurrentDataSrc->fInitialized ||
       pCurrentDataSrc->fnFillMenu == NULL )
    return;

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  pCurrentDataSrc->fnFillMenu( hwndMenu, ulItem );
  DosReleaseMutexSem( hmtxUpdate );
}

VOID itemsCommand(HWND hwndOwner, USHORT usCommand)
{
  ULONG      ulRC;

  if ( pCurrentDataSrc != NULL && pCurrentDataSrc->fInitialized &&
       pCurrentDataSrc->fnCommand != NULL )
  {
    ulRC = pCurrentDataSrc->fnCommand( hwndOwner, usCommand );

    if ( ulRC != DS_UPD_NONE )
    {
      WinPostMsg( hwndMain, WM_SL_UPDATE_LIST,
                  MPFROMLONG( ulRC == DS_UPD_DATA ), 0 );
      WinPostMsg( hwndMain, WM_SL_UPDATE_DETAILS, NULL, NULL );
    }
  }
}

ULONG itemsSortBy(ULONG ulNewSort)
{
  ULONG      ulSort;

  if ( pCurrentDataSrc == NULL || !pCurrentDataSrc->fInitialized ||
       pCurrentDataSrc->fnSortBy == NULL )
    return 0;

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  ulSort = pCurrentDataSrc->fnSortBy( ulNewSort );
  DosReleaseMutexSem( hmtxUpdate );

  return ulSort;
}

VOID itemsEnter(HWND hwnd)
{
  ULONG      ulRC;

  if ( pCurrentDataSrc == NULL || !pCurrentDataSrc->fInitialized ||
       pCurrentDataSrc->fnEnter == NULL )
    return;

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  ulRC = _getSel( pCurrentDataSrc );
  DosReleaseMutexSem( hmtxUpdate );

  if ( ulRC == DS_SEL_NONE )
    return;

  ulRC = pCurrentDataSrc->fnEnter( hwnd );
  if ( ulRC == DS_UPD_NONE )
    return;

  WinPostMsg( hwndMain, WM_SL_UPDATE_LIST, MPFROMLONG(ulRC == DS_UPD_DATA), 0 );
  WinPostMsg( hwndMain, WM_SL_UPDATE_DETAILS, NULL, NULL );
}

// BOOL itemsHelp(BOOL fShow)
//
// Checks and returns TRUE if help (main panel) for selected data source is
// available. Shows help window with main panel from file <module>.hlp using
// index from data soure's information record (DSINFO) if fShow is TRUE.

BOOL itemsHelp(BOOL fShow)
{
  if ( pCurrentDataSrc == NULL || pCurrentDataSrc->hwndHelp == NULLHANDLE &&
       pCurrentDataSrc->pDSInfo->ulHelpId == 0 )
    return FALSE;

  if ( fShow )
    WinSendMsg( pCurrentDataSrc->hwndHelp, HM_DISPLAY_HELP,
                MPFROMLONG( MAKELONG( pCurrentDataSrc->pDSInfo->ulHelpId, NULL ) ),
                MPFROMSHORT( HM_RESOURCEID ) );
  return TRUE;
}
