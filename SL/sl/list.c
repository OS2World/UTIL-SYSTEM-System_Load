#define INCL_DOSERRORS   /* DOS error values */
#define INCL_DOSRESOURCES     /* Resource types */
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <string.h>
#include "utils.h"
#include "items.h"
#include "hint.h"
#include "sl.h"
#include "debug.h"

#define WC_SL_LIST		"SLList"
// WM_SL_ORDERITEMS - Internal list window message.
#define WM_SL_ORDERITEMS	( WM_USER + 1 )

extern HINI		hIni;		// sl.c
extern HWND		hwndMain;	// sl.c

typedef struct _LISTWINDATA {
  LONG		lScrollPos;
  HWND		hwndHint;
  HWND		hwndCtxMenu;
  PSHORT	psCtxMenuStaticItems;
} LISTWINDATA, *PLISTWINDATA;


static VOID _freeListWinData(PLISTWINDATA pstWinData)
{
  WinDestroyWindow( pstWinData->hwndCtxMenu );

  if ( pstWinData->psCtxMenuStaticItems != NULL )
    debugFree( pstWinData->psCtxMenuStaticItems );

  hintDestroy( pstWinData->hwndHint );

  debugFree( pstWinData );
}

static MRESULT _wmCreate(HWND hwnd)
{
  PLISTWINDATA	pstWinData;
  PVOID		pCtxMenu;
  ULONG		ulRC;
  ULONG		cItems;
  ULONG		ulIdx;

  // Load context menu template from resource
  ulRC = DosGetResource2( 0, (USHORT)RT_MENU, ID_POPUP_MENU, &pCtxMenu );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosGetResource2(), rc = %u", ulRC );
    return (MRESULT)TRUE;		// Discontinue window creation
  }

  pstWinData = debugCAlloc( 1, sizeof(LISTWINDATA) );

  // Create context menu
  pstWinData->hwndCtxMenu = WinCreateMenu( HWND_DESKTOP, pCtxMenu );
  DosFreeResource( pCtxMenu );

  // Store context menu top-level items IDs

  cItems = (SHORT)WinSendMsg( pstWinData->hwndCtxMenu, MM_QUERYITEMCOUNT, 0, 0 );
  pstWinData->psCtxMenuStaticItems = debugMAlloc( (cItems + 1) * sizeof(ULONG) );
  if ( pstWinData->psCtxMenuStaticItems == NULL )
    debug( "Warning! Not enough memory" );
  else
  {
    for( ulIdx = 0; ulIdx < cItems; ulIdx++ )
    {
      pstWinData->psCtxMenuStaticItems[ulIdx] =
        (SHORT)WinSendMsg( pstWinData->hwndCtxMenu, MM_ITEMIDFROMPOSITION,
                           MPFROMSHORT( ulIdx ), 0 );
    }
    pstWinData->psCtxMenuStaticItems[ulIdx] = MIT_ERROR; // end of list mark
  }

  // Create hint window and store handle
  pstWinData->hwndHint = hintCreate( hwnd );

  // Register items window class, start update thread.
  if ( !itemsInit( WinQueryAnchorBlock( hwnd ) ) )
  {
    debug( "itemsInit() fail" );
    _freeListWinData( pstWinData );
    return (MRESULT)TRUE;		// Discontinue window creation
  }

  WinSetWindowPtr( hwnd, 0, pstWinData );
  return (MRESULT)FALSE; // Success code.
}

static VOID _wmDestroy(HWND hwnd)
{
  PLISTWINDATA	pstWinData = (PLISTWINDATA)WinQueryWindowPtr( hwnd, 0 );

  // Stop update thread
  itemsDone();

  // Destroy hint window, context menu, e.t.c.
  _freeListWinData( pstWinData );
}

static VOID _wmSLOrederItems(HWND hwnd)
{
  // Calculate the position of all items windows. Clear the space therebetween.

  PLISTWINDATA		pstWinData = (PLISTWINDATA)WinQueryWindowPtr( hwnd, 0 );
  HWND			hwndItem;
  SWP			swpList;
  PSWP			paswpItem = NULL, pswpItem;
  ULONG			cItems = 0;
  ULONG			ulLeftOffs;
  LONG			lTop, lTopOffs;
  ULONG			ulColWidth = 0;
  POINTL		ptlItem;
  HRGN			hrgnValid, hrgnInvalid, hrgnUpdate;
  HPS			hps;
  RECTL			rectlListFrame;
  PRECTL		parectlItem, prectlItem;
  LONG			lScrollPos = 0;
  ULONG			ulIdx;
  LONG			lMax = 0;
  HWND			hwndListFrame = WinQueryWindow( hwnd, QW_PARENT );
  HWND			hwndScroll = WinWindowFromID( hwndListFrame,
                                                      FID_HORZSCROLL );
  SIZEL			sizeSpace;
  HENUM			hEnum;

  itemsGetSpace( &sizeSpace );
  ulLeftOffs = sizeSpace.cx;

  // Calculate top bound of items list
  WinQueryWindowPos( hwnd, &swpList );
  lTop = swpList.cy - sizeSpace.cy;
  lTopOffs = lTop;

  // Scan items, calculate new positions

  hEnum = WinBeginEnumWindows( hwnd );
  while( ( hwndItem = WinGetNextWindow( hEnum ) ) != NULLHANDLE )
  {
    if ( ( cItems == 0 ) || ( (cItems & 0x00FF) == 0x00FF ) )
    {
      PSWP	paswpItemNew =
         debugReAlloc( paswpItem, (cItems & ~0x00FF) + 0x0100 * sizeof(SWP) );

      if ( paswpItemNew == NULL )
      {
        debug( "Not enough memory" );
        break;
      }
      paswpItem = paswpItemNew;
      pswpItem = &paswpItem[cItems];
    }

    WinQueryWindowPos( hwndItem, pswpItem );

    // Calculate new itam's position

    if ( lTopOffs != lTop &&
         ( lTopOffs - ( pswpItem->cy + sizeSpace.cy ) <= 0 ) )
    {
      // Lower boundary is reached - jump to the new column
      ulLeftOffs += ulColWidth;
      lTopOffs = lTop;
      ulColWidth = 0;
    }

    // Calculate column width
    if ( ( pswpItem->cx + sizeSpace.cx ) > ulColWidth )
      ulColWidth = pswpItem->cx + sizeSpace.cx;

    pswpItem->x = ulLeftOffs;
    pswpItem->y = lTopOffs - pswpItem->cy;
    // Vertical offset for next item
    lTopOffs -= ( pswpItem->cy + sizeSpace.cy );

    // Next item...
    cItems++;
    pswpItem++;
  }
  WinEndEnumWindows( hEnum );

  // Set scroll bar

  if ( cItems != 0 )
  {
    LONG		lMaxScroll;

    lMax = paswpItem[cItems - 1].x + ulColWidth;
    lMaxScroll = lMax - swpList.cx;
    if ( lMax <= swpList.cx )
      lScrollPos = 0;
    else
    {
      lScrollPos = pstWinData->lScrollPos;
      if ( lScrollPos > lMaxScroll )
        lScrollPos = lMaxScroll;
    }

    WinSendMsg( hwndScroll, SBM_SETSCROLLBAR,
      MPFROMSHORT( lScrollPos ), MPFROM2SHORT( 1, lMaxScroll ) );	
  }
  // Set scroll bar slider size or disable scroll bar
  WinSendMsg( hwndScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(swpList.cx, lMax), 0 );
  pstWinData->lScrollPos = lScrollPos;

  // Change coordinates of items by scroll bar position. Build array of window
  // rectangles for all items.

  parectlItem = debugMAlloc( cItems * sizeof(RECTL) );
  if ( parectlItem == NULL )
    debug( "Not enough memory" );
  else
  {
    for( ulIdx = 0, pswpItem = paswpItem, prectlItem = parectlItem;
         ulIdx < cItems; ulIdx++, pswpItem++, prectlItem++ )
    {
      pswpItem->x -= lScrollPos;
      // Calc rectangle of item in frame-window's coordinates
      ptlItem.x = pswpItem->x;
      ptlItem.y = pswpItem->y;
      WinMapWindowPoints( hwnd, hwndListFrame, &ptlItem, 1 );
      prectlItem->xLeft = ptlItem.x;
      prectlItem->yBottom = ptlItem.y;
      prectlItem->xRight = ptlItem.x + pswpItem->cx;
      prectlItem->yTop = ptlItem.y + pswpItem->cy;
    }
  }

  // Move reordered items windows
  if ( paswpItem != NULL )
  {
    WinSetMultWindowPos( WinQueryAnchorBlock( hwnd ), paswpItem, cItems );
    debugFree( paswpItem );
  }

  // Update (erase) not used window area.

  if ( parectlItem == NULL )
  {
    WinInvalidateRegion( hwndListFrame, NULLHANDLE, TRUE );
    return;
  }

  hps = WinGetPS( hwndListFrame );

  // hrgnInvalid - invalid region, whole window
  WinQueryWindowRect( hwndListFrame, &rectlListFrame );
  hrgnInvalid = GpiCreateRegion( hps, 1, &rectlListFrame );

  // hrgnValid - rectangles of items
  hrgnValid = GpiCreateRegion( hps, cItems, parectlItem );
  debugFree( parectlItem );

  // hrgnUpdate - region to update is ( hrgnInvalid AND NOT hrgnValid )
  hrgnUpdate = GpiCreateRegion( hps, 0, NULL );
  GpiCombineRegion( hps, hrgnUpdate, hrgnInvalid, hrgnValid, CRGN_DIFF );
  GpiDestroyRegion( hps, hrgnValid );
  GpiDestroyRegion( hps, hrgnInvalid );

  // Update (erase) result region
  WinInvalidateRegion( hwndListFrame, hrgnUpdate, TRUE );
  GpiDestroyRegion( hps, hrgnUpdate );
  WinReleasePS( hps );
}


// Check corresponding sort direction ("Ascending" and "Descending")
// menu items.

static VOID _checkSortDirectionItems(HWND hwndMenu, ULONG ulSort)
{
  WinSendMsg( hwndMenu, MM_SETITEMATTR,
    MPFROM2SHORT( IDM_SORT_ASCN, TRUE ),
    MPFROM2SHORT( MIA_CHECKED,
                  (ulSort & DSSORT_FL_DESCENDING) ? 0 : MIA_CHECKED ) );
  WinSendMsg( hwndMenu, MM_SETITEMATTR,
    MPFROM2SHORT( IDM_SORT_DESCN, TRUE ),
    MPFROM2SHORT( MIA_CHECKED,
                  (ulSort & DSSORT_FL_DESCENDING) ? MIA_CHECKED : 0 ) );
}

// _wmSLSetDataSrc(HWND hwnd, MPARAM mp1, MPARAM mp2)
// - Set data source given by (PDATASOURCE)mp1 if it is not NULL.
// - Rebuild items windows for data source given by mp1 or current data
//   source.
// If data source changed:
// - Set main window title for new data source.
// - Check correspond menu item, uncheck old datasource's menu item (main
//   and context menus).
// - Rebuild "Sort" menus for new data source.

static MRESULT _wmSLSetDataSrc(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  PLISTWINDATA	pstWinData = (PLISTWINDATA)WinQueryWindowPtr( hwnd, 0 );
  PDATASOURCE	pDataSrc = (PDATASOURCE)PVOIDFROMMP( mp1 );
  PDATASOURCE	pOldDataSrc = itemsGetDataSrc();
  CHAR		szBuf[256];
  PCHAR		pcChar;
  MENUITEM	stMISort;
  MENUITEM	stMICtxSort;
  MENUITEM	stMINew;
  HWND		hwndFrame, hwndMenu;
  HWND		hwndCtxMenu = pstWinData->hwndCtxMenu;
  ULONG		ulSort;
  ULONG		ulIdx;

  if ( pDataSrc == NULL )
  {
    if ( pOldDataSrc == NULL )
      return (MRESULT)FALSE;

    pDataSrc = pOldDataSrc;
  }

  itemsSetDataSrc( hwnd, pDataSrc );
  WinSendMsg( hwnd, WM_SL_ORDERITEMS, 0, 0 );

  if ( pDataSrc == pOldDataSrc || !pDataSrc->fInitialized )
    return (MRESULT)FALSE;

  hwndFrame = WinQueryWindow( hwndMain, QW_PARENT );
  hwndMenu = WinWindowFromID( hwndFrame, FID_MENU );

  // Set window title (static part + ": " + menu item title)
  // Load static part.
  ulIdx = WinLoadString( WinQueryAnchorBlock( hwnd ), 0, IDS_TITLE,
                         sizeof(szBuf) - 3, &szBuf );
  szBuf[ulIdx++] = ':';
  szBuf[ulIdx++] = ' ';
  // Get data source title without '~'.
  strRemoveMnemonic( sizeof(szBuf) - 1 - ulIdx, &szBuf[ulIdx],
                     &pDataSrc->szTitle );
  // Set new window title.
  WinSetWindowText( hwndFrame, &szBuf );

  // Uncheck old data source's menu item.
  if ( pOldDataSrc != NULL )
  {
    WinSendMsg( hwndMenu, MM_SETITEMATTR,
      MPFROM2SHORT( pOldDataSrc->ulMenuItemId, TRUE ),
      MPFROM2SHORT( MIA_CHECKED, FALSE ) );

    WinSendMsg( hwndCtxMenu, MM_SETITEMATTR,
      MPFROM2SHORT( pOldDataSrc->ulMenuItemId, TRUE ),
      MPFROM2SHORT( MIA_CHECKED, FALSE ) );
  }

  // Check new data source's menu item.
  WinSendMsg( hwndMenu, MM_SETITEMATTR,
    MPFROM2SHORT( pDataSrc->ulMenuItemId, TRUE ),
    MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED ) );

  WinSendMsg( hwndCtxMenu, MM_SETITEMATTR,
    MPFROM2SHORT( pDataSrc->ulMenuItemId, TRUE ),
    MPFROM2SHORT( MIA_CHECKED, MIA_CHECKED ) );

  // Enable/disable menu item "Help".
  WinSendMsg( hwndCtxMenu, MM_SETITEMATTR,
              MPFROM2SHORT( IDM_HELP, TRUE ),
              MPFROM2SHORT( MIA_DISABLED,
                itemsHelp( FALSE ) ? 0 : MIA_DISABLED ) );

  // Query menu item "Sort" (submenu).
  WinSendMsg( hwndMenu, MM_QUERYITEM, MPFROM2SHORT( IDM_VIEW_SORT, TRUE ),
              MPFROMP( &stMISort ) );
  WinSendMsg( hwndCtxMenu, MM_QUERYITEM, MPFROM2SHORT( IDM_VIEW_SORT, TRUE ),
              MPFROMP( &stMICtxSort ) );

  // Delete old items form submenu "Sort" (left last 2 items:
  // "Ascending" and "Descending").
  for( ulIdx = IDM_SORT_FIRST_ID;
       (SHORT)WinSendMsg( stMISort.hwndSubMenu, MM_DELETEITEM,
                          MPFROM2SHORT( ulIdx, FALSE ), 0 ) > 2;
       ulIdx++ );
  for( ulIdx = IDM_SORT_FIRST_ID;
       (SHORT)WinSendMsg( stMICtxSort.hwndSubMenu, MM_DELETEITEM,
                          MPFROM2SHORT( ulIdx, FALSE ), 0 ) > 2;
       ulIdx++ );

  if ( pDataSrc->pDSInfo->cSortBy == 0 || pDataSrc->fnSortBy == NULL )
  {
    // Disable item (submenu) "Sort" if we have no sort-by enrties for
    // this data source.
    WinSendMsg( hwndMenu, MM_SETITEMATTR,
                MPFROM2SHORT( IDM_VIEW_SORT, TRUE ),
                MPFROM2SHORT( MIA_DISABLED, MIA_DISABLED ) );

    WinSendMsg( hwndCtxMenu, MM_SETITEMATTR,
                MPFROM2SHORT( IDM_VIEW_SORT, TRUE ),
                MPFROM2SHORT( MIA_DISABLED, MIA_DISABLED ) );
  }
  else
  {
    // Enable item (submenu) "Sort".
    WinSendMsg( hwndMenu, MM_SETITEMATTR,
                MPFROM2SHORT( IDM_VIEW_SORT, TRUE ),
                MPFROM2SHORT( MIA_DISABLED, 0 ) );
    WinSendMsg( hwndCtxMenu, MM_SETITEMATTR,
                MPFROM2SHORT( IDM_VIEW_SORT, TRUE ),
                MPFROM2SHORT( MIA_DISABLED, 0 ) );

    // Query current sort type from data source.
    ulSort = pDataSrc->fnSortBy( DSSORT_QUERY );

    // Fill submenu "Sort". Do not add single sort-by menu item - use
    // only "Ascending" and "Descending" for sorting.
    if ( pDataSrc->pDSInfo->cSortBy > 1 )
    {
      stMINew.afStyle = MIS_TEXT;
      stMINew.hwndSubMenu = NULLHANDLE;
      stMINew.hItem = NULLHANDLE;
      stMINew.id = IDM_SORT_FIRST_ID;

      for( stMINew.iPosition = 0;
           stMINew.iPosition < pDataSrc->pDSInfo->cSortBy;
           stMINew.iPosition++, stMINew.id++ )
      {
        stMINew.afAttribute =
          (ulSort & DSSORT_VALUE_MASK) == stMINew.iPosition ? MIA_CHECKED : 0;

        if ( (pDataSrc->pDSInfo->ulFlags & DS_FL_RES_SORT_BY) != 0 )
        {
          strLoad( pDataSrc->hModule,
                   (ULONG)pDataSrc->pDSInfo->_sortBy.aulResId[stMINew.iPosition],
                   sizeof(szBuf), &szBuf );
          pcChar = &szBuf;
        }
        else
          pcChar = pDataSrc->pDSInfo->_sortBy.apszStr[stMINew.iPosition];

        WinSendMsg( stMISort.hwndSubMenu, MM_INSERTITEM, MPFROMP( &stMINew ),
                    pcChar );
        WinSendMsg( stMICtxSort.hwndSubMenu, MM_INSERTITEM, MPFROMP( &stMINew ),
                    pcChar );
      }

      // Add separator.
      stMINew.afStyle = MIS_SEPARATOR;
      stMINew.afAttribute = 0;
      WinSendMsg( stMISort.hwndSubMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );
      WinSendMsg( stMICtxSort.hwndSubMenu, MM_INSERTITEM,
                  MPFROMP( &stMINew ), 0 );
    }
  }

  _checkSortDirectionItems( hwndMenu, ulSort );
  _checkSortDirectionItems( hwndCtxMenu, ulSort );
  return (MRESULT)TRUE;
}


// _wmSLContextMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
//   (POINTS)mp1 - menu coordinates,
//   (LONG)mp2 - data source item for which will be built menu or DS_SEL_NONE.
static VOID _wmSLContextMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  PPOINTS		pPt = (PPOINTS)&mp1;
  ULONG			ulDataSrcItem = LONGFROMMP( mp2 );
  PLISTWINDATA		pstWinData = (PLISTWINDATA)WinQueryWindowPtr( hwnd, 0 );
  HWND			hwndCtxMenu = pstWinData->hwndCtxMenu;
  SHORT			sIdx, sItemId;
  BOOL			fFound;
  ULONG			ulIdx;
  PSHORT		psCtxMenuStaticItems = pstWinData->psCtxMenuStaticItems;

  // Remove all not static items from context menu which remains from
  // previous function itemsFillContextMenu() call.

  if ( psCtxMenuStaticItems != NULL )
  {
    for( sIdx = (SHORT)WinSendMsg( hwndCtxMenu, MM_QUERYITEMCOUNT, 0, 0 ) - 1;
         sIdx >= 0 ; sIdx-- )
    {
      // Query Id of an existing item.
      sItemId = (SHORT)WinSendMsg( hwndCtxMenu, MM_ITEMIDFROMPOSITION,
                                   MPFROMSHORT( sIdx ), 0 );
      if ( sItemId == MIT_ERROR )
        break;

      // Seek Id in the static IDs list.
      fFound = FALSE;
      for( ulIdx = 0; psCtxMenuStaticItems[ulIdx] != MIT_ERROR; ulIdx++ )
        if ( psCtxMenuStaticItems[ulIdx] == sItemId )
        {
          fFound = TRUE;
          break;
        }

      // Remove item if Id was not listed.
      if ( !fFound )
        WinSendMsg( hwndCtxMenu, MM_DELETEITEM, MPFROM2SHORT( sItemId, 1 ),
                    0 );
    }
  }

  // Let data source fill context menu.
  itemsFillContextMenu( hwndCtxMenu, ulDataSrcItem );

  // Show context menu.
  WinPopupMenu( hwnd, hwndMain, hwndCtxMenu, pPt->x, pPt->y, 0,
                PU_NONE | PU_MOUSEBUTTON1| PU_KEYBOARD | PU_HCONSTRAIN |
                PU_VCONSTRAIN );
}


MRESULT EXPENTRY ListWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_CREATE:
      return _wmCreate( hwnd );

    case WM_DESTROY:
      _wmDestroy( hwnd );
      break;

    case WM_ERASEBACKGROUND:
      {
        PDATASOURCE		pDataSrc = itemsGetDataSrc();
        HPS			hps = (HPS)mp1;

        // Switch HPS into RGB mode.
        GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
        // Fill given rectangle with data source's background color.
        WinFillRect( hps, (PRECTL)mp2, 
                     pDataSrc == NULL ? SYSCLR_WINDOW : pDataSrc->lListBackCol );
      }
      return (MRESULT)FALSE;

    case WM_SIZE:
      WinPostMsg( hwnd, WM_SL_ORDERITEMS, 0, 0 );
      return (MRESULT)FALSE;

    case WM_HELP:
      itemsHelp( TRUE );
      return (MRESULT)FALSE;

    case WM_CHAR:
      if ( (SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_KEYUP)) != KC_VIRTUALKEY )
        break;

      switch( SHORT2FROMMP(mp2) )
      {
        case VK_HOME:
          itemsSelect( hwnd, TRUE, 0 );
          break;

        case VK_END:
          itemsSelect( hwnd, TRUE, 0x7FFFFFFF );
          break;

        case VK_UP:
          itemsSelect( hwnd, FALSE, -1 );
          break;

        case VK_DOWN:
          itemsSelect( hwnd, FALSE, 1 );
          break;

        case VK_ENTER:
        case VK_NEWLINE:
          itemsEnter( hwnd );
          break;

        default:
          return WinDefWindowProc( hwnd, msg, mp1, mp2 );
      }
          return WinDefWindowProc( hwnd, msg, mp1, mp2 );
      return (MRESULT)FALSE;

    case WM_COMMAND:
      {
        USHORT		usCommand = SHORT1FROMMP(mp1);
        PDATASOURCE	pDataSrc;

        if ( usCommand == IDM_HELP )
        {
          itemsHelp( TRUE );
          return (MRESULT)FALSE;
        }

        if ( usCommand == IDM_SORT_ASCN || usCommand == IDM_SORT_DESCN ||
             ( usCommand >= IDM_SORT_FIRST_ID &&
               usCommand <= IDM_SORT_LAST_ID ) )
        {
          // Sort type changed by user.

          ULONG		ulIdx;
          HWND		hwndFrame = WinQueryWindow( hwndMain, QW_PARENT );
          HWND		hwndMenu = WinWindowFromID( hwndFrame, FID_MENU );
          PLISTWINDATA	pstWinData = (PLISTWINDATA)WinQueryWindowPtr( hwnd, 0 );
          // Query current sort type from selected data source
          ULONG		ulSort = itemsSortBy( DSSORT_QUERY );

          // Correct sort type by command
          if ( usCommand == IDM_SORT_ASCN )
            ulSort &= ~DSSORT_FL_DESCENDING;
          else if ( usCommand == IDM_SORT_DESCN )
            ulSort |= DSSORT_FL_DESCENDING;
          else
            ulSort = (ulSort & DSSORT_FL_DESCENDING) +
                     (usCommand - IDM_SORT_FIRST_ID);

          // Set new sort type for selected data source
          ulSort = itemsSortBy( ulSort );

          // Check/uncheck menu items
          pDataSrc = itemsGetDataSrc();
          for( ulIdx = 0; ulIdx < pDataSrc->pDSInfo->cSortBy; ulIdx++ )
          {
            WinSendMsg( hwndMenu, MM_SETITEMATTR,
              MPFROM2SHORT( (ulIdx + IDM_SORT_FIRST_ID), TRUE ),
              MPFROM2SHORT( MIA_CHECKED,
                            (ulSort & DSSORT_VALUE_MASK) == ulIdx ?
                              MIA_CHECKED : 0 ) );

            WinSendMsg( pstWinData->hwndCtxMenu, MM_SETITEMATTR,
              MPFROM2SHORT( (ulIdx + IDM_SORT_FIRST_ID), TRUE ),
              MPFROM2SHORT( MIA_CHECKED,
                            (ulSort & DSSORT_VALUE_MASK) == ulIdx ?
                              MIA_CHECKED : 0 ) );
          }

          _checkSortDirectionItems( hwndMenu, ulSort );
          _checkSortDirectionItems( pstWinData->hwndCtxMenu, ulSort );
          WinPostMsg( hwnd, WM_SL_SETDATASRC, 0, 0 );

          return (MRESULT)FALSE;
        }

        if ( ( usCommand >= IDM_DSCMD_FIRST_ID &&
               usCommand <= IDM_DSCMD_LAST_ID ) )
        {
          // This commands may be used by data source (from context menu).
          itemsCommand( hwnd, usCommand );
          return (MRESULT)FALSE;
        }

        // New data source selected by user.
        pDataSrc = srclstGetByMenuItemId( usCommand );
        WinSendMsg( hwnd, WM_SL_SETDATASRC, MPFROMP( pDataSrc ), NULL );
      }
      return (MRESULT)FALSE;

    case WM_CONTEXTMENU:
      if ( SHORT2FROMMP( mp2 ) != 0 )
      {
        SWP		swp;

        WinQueryWindowPos( hwnd, &swp );
        ((PPOINTS)&mp1)->x = swp.cx >> 1;
        ((PPOINTS)&mp1)->y = swp.cy >> 1;
      }
      mp2 = MPFROMLONG( DS_SEL_NONE ); // Menu not for data source item.
      // Go to WM_SL_CONTEXTMENU

    case WM_SL_CONTEXTMENU:
      _wmSLContextMenu( hwnd, mp1, mp2 );
      return (MRESULT)TRUE;

    case WM_PRESPARAMCHANGED:
      {
        ULONG			ulPresParam = LONGFROMMP( mp1 );
        PDATASOURCE		pDataSrc = itemsGetDataSrc();

        if ( pDataSrc == NULL )
          break;

        switch( ulPresParam )
        {
          case PP_FONTNAMESIZE:
            {
              CHAR		szBuf[128] = "";

              if ( pDataSrc != NULL )
              {
                WinQueryPresParam( hwnd, PP_FONTNAMESIZE, 0, NULL,
                                   sizeof(szBuf), &szBuf, QPF_NOINHERIT );
                // Store new font for data source
                srclstSetFont( pDataSrc, &szBuf );
                // Remove and create items with new font
                WinSendMsg( hwnd, WM_SL_SETDATASRC, 0, 0 );
              }
            }
            return (MRESULT)FALSE;

          case PP_BACKGROUNDCOLOR:
//          case PP_FOREGROUNDCOLOR:
            {
              LONG			lColor;

              WinQueryPresParam( hwnd, LONGFROMMP( mp1 ), 0, NULL,
                                 sizeof(LONG), &lColor, QPF_NOINHERIT );
              // Store new color for data source
              srclstSetColor( pDataSrc,
                              ulPresParam == PP_BACKGROUNDCOLOR ?
                                SLCT_LISTBACK : SLCT_ITEMBACK,
                              lColor );
              // Update list window (with background)
              WinInvalidateRect( WinQueryWindow(hwnd, QW_PARENT), NULL, TRUE );
            }
            return (MRESULT)FALSE;
        } // switch
      }
      return (MRESULT)FALSE;
      
    case WM_HSCROLL:
      {
        PLISTWINDATA	pstWinData = (PLISTWINDATA)WinQueryWindowPtr( hwnd, 0 );
        SWP		swpList;

        WinQueryWindowPos( hwnd, &swpList );

        switch( SHORT2FROMMP( mp2 ) )
        {
          case SB_LINELEFT:
             pstWinData->lScrollPos -= 10;
             break;
          case SB_PAGELEFT:
             pstWinData->lScrollPos -= swpList.cx;
             break;
          case SB_LINERIGHT:
             pstWinData->lScrollPos += 10;
             break;
          case SB_PAGERIGHT:
             pstWinData->lScrollPos += swpList.cx;
             break;
          case SB_SLIDERPOSITION:
          case SB_SLIDERTRACK:
             pstWinData->lScrollPos = SHORT1FROMMP( mp2 );
             break;
          case SB_SL_SLIDEROFFSET:
             pstWinData->lScrollPos += (SHORT)SHORT1FROMMP( mp2 );
             break;
        }

        if ( pstWinData->lScrollPos < 0 )
          pstWinData->lScrollPos = 0;
      } // Go to WM_SL_ORDERITEMS

    case WM_SL_ORDERITEMS:
      _wmSLOrederItems( hwnd );
      return (MRESULT)FALSE;

    case WM_SL_SETDATASRC:
      return _wmSLSetDataSrc( hwnd, mp1, mp2 );

    case WM_SL_UPDATE_DATA:
      // Update the data only in existing items.
      WinBroadcastMsg( hwnd, WM_SL_UPDATE_DATA, 0, 0, BMSG_SEND );
      return (MRESULT)FALSE;

    case WM_SL_QUERY:
      {
        // mp1: SLQUERY_HINT - return hint window handle
        //      SLQUERY_CTXMENU - return context (popup) menu window handle

        PLISTWINDATA	pstWinData = (PLISTWINDATA)WinQueryWindowPtr( hwnd, 0 );

        return (MRESULT)( LONGFROMMP( mp1 ) == SLQUERY_HINT ?
                            pstWinData->hwndHint : pstWinData->hwndCtxMenu );
      }
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


HWND lstInstall(HWND hwndParent, ULONG ulId)
{
  HWND		hwnd, hwndFrame;
  ULONG		ulFrameFlags = FCF_HORZSCROLL | FCF_BORDER;

  if ( !WinRegisterClass( WinQueryAnchorBlock( hwndParent ), WC_SL_LIST,
                          ListWndProc, 0, sizeof(PLISTWINDATA) ) )
  {
    debug( "WinRegisterClass() fail" );
    return NULLHANDLE;
  }

  // Data source items list window
  hwndFrame = WinCreateStdWindow( hwndParent, WS_VISIBLE, &ulFrameFlags,
                                  WC_SL_LIST, NULL, 0, 0, ulId, &hwnd );
  if ( hwndFrame == NULLHANDLE )
  {
    debug( "WinCreateStdWindow() fail" );
    return NULLHANDLE;
  }

  WinSetFocus( HWND_DESKTOP, hwndFrame );
  return hwnd;
}
