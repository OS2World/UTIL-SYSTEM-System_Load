#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "hint.h"
#include "debug.h"     // Must be the last.

#define HINT_BOTTOM_OFFS		20
#define HINT_TOP_OFFS			10

#define HINT_TEXT_LPAD			2
#define HINT_TEXT_RPAD			2
#define HINT_TEXT_BPAD			0
#define HINT_TEXT_TPAD			0

#define HINT_USE_STD_COLORS
// Values HINT_CLR_*  will be used only if HINT_USE_STD_COLORS is not specified.
#define HINT_CLR_TEXT			0x00000050
#define HINT_CLR_BACKGROUND		0x00FFFFE0
#define HINT_CLR_BORDER			0x00505050

#define HINT_SHOW_DELAY			700
#define HINT_CHECK_DELAY		200
#define HINT_ID_TIMER			1

typedef struct _HINTDATA {
  HWND       hwndTag;
  PSZ        pszText;
  ULONG      ulTimerId;
  POINTL     ptPointer;
  ULONG      ulCXSens;
  ULONG      ulCYSens;
  HAB        hab;
} HINTDATA, *PHINTDATA;


static VOID _hintDrawText(HPS hps, PRECTL prctlText, PSZ pszText, BOOL fReqSize)
{
  LONG         lRC = 1;
  LONG         lSaveYTop = prctlText->yTop;
  LONG         cbText = strlen( pszText );
  LONG         lMinLeft = prctlText->xRight;
  LONG         lMaxRight = prctlText->xLeft;
  RECTL        rctlDraw;
  FONTMETRICS  stFontMetrics;
  ULONG        flCmd;
  ULONG        ulLineCY = 0;

  GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &stFontMetrics );
  ulLineCY = stFontMetrics.lMaxBaselineExt + stFontMetrics.lExternalLeading;

  if ( fReqSize )
  {
//    prctlText->yBottom = prctlText->yTop - ulLineCY - 1;
    flCmd = DT_LEFT | DT_TOP | DT_WORDBREAK | DT_QUERYEXTENT;
  }
  else
#ifdef HINT_USE_STD_COLORS
    flCmd = DT_LEFT | DT_TOP | DT_WORDBREAK | DT_TEXTATTRS;
#else
    flCmd = DT_LEFT | DT_TOP | DT_WORDBREAK;
#endif

  while( ( lRC != 0 ) && ( cbText > 0 ) )
  {
    memcpy( &rctlDraw, prctlText, sizeof(RECTL) );
    lRC = WinDrawText( hps, cbText, pszText, &rctlDraw,
                       HINT_CLR_TEXT, HINT_CLR_BACKGROUND, flCmd );
   if ( lRC == 0 ) // Without FT2LIB WinDrawText() returns 0 on each line end.
    {
      PCHAR  pcLF;

      if ( *pszText == '\0' )
        break;

      pcLF = strchr( pszText, 0x0A );
      lRC = pcLF == NULL ? 0 : ( (pcLF - pszText) + 1 );
    }
    else
    {
      if ( rctlDraw.xLeft < lMinLeft )
        lMinLeft = rctlDraw.xLeft;
      if ( rctlDraw.xRight > lMaxRight )
        lMaxRight = rctlDraw.xRight;
    }

    pszText += lRC;
    cbText -= lRC;
    /* With FT2LIB WinDrawText() stops on 0xOD and 0x0A characters, we need to
       skip 0x0A after 0x0D. */
    while( *pszText == 0x0A ) { pszText++; cbText--; }

    prctlText->yTop -= ulLineCY;
  }

  prctlText->xLeft    = lMinLeft;
  prctlText->xRight   = lMaxRight;
  prctlText->yBottom  = prctlText->yTop;
  prctlText->yTop     = lSaveYTop;
}

static BOOL _hintSetText(PHINTDATA pData, PSZ pszText)
{
  PCHAR      pcEnd;
  ULONG      cbText;

  // Destroy old text
  if ( pData->pszText != NULL )
  {
    free( pData->pszText );
    pData->pszText = NULL;
  }

  if ( pszText == NULL )
    return FALSE;

  // Removes leading and trailing spaces, \t, \r, \n.
  while( isspace( *pszText ) ) pszText++;
  pcEnd = strchr( pszText, '\0' );
  while( pcEnd > pszText && isspace( *(pcEnd - 1) ) ) pcEnd--;
  cbText = pcEnd - pszText;
  if ( cbText == 0 )
    return FALSE;

  // Allocate memory with new text and store new text pointer at
  // hint-window's data

  pData->pszText = malloc( cbText + 1 );
  if ( pData->pszText == NULL )
    return FALSE;
  memcpy( pData->pszText, pszText, cbText );
  pData->pszText[cbText] = '\0';

  return TRUE;
}

static BOOL _hintSetWindow(HWND hwnd)
{
  PHINTDATA  pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );
  RECTL      rclWnd = { 1000, 0, 1000, 300 };
  LONG       lSrcCX = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
  HPS        hps;
  LONG       lX, lY;
  ULONG      ulCX, ulCY;
  CHAR       szText[1024];

  if ( pData->pszText == NULL )
  {
    // Text was not specified with message - request it from window
    szText[0] = '\0';
    WinSendMsg( pData->hwndTag, WM_HINT_GETTEXT,
                MPFROMLONG(sizeof(szText)), MPFROMP(szText) );

    if ( !_hintSetText( pData, szText ) )
      return FALSE;
  }

  // Calc window size

  // Request text size
  rclWnd.xRight += ( lSrcCX / 2 ) - 1;
  hps = WinGetPS( hwnd );
  _hintDrawText( hps, &rclWnd, pData->pszText, TRUE );
  WinReleasePS( hps );
  // Window size is text size + paddings + border
  ulCX = (rclWnd.xRight - rclWnd.xLeft) + HINT_TEXT_LPAD + HINT_TEXT_RPAD + 2;
  ulCY = (rclWnd.yTop - rclWnd.yBottom) + HINT_TEXT_BPAD + HINT_TEXT_TPAD + 2;

  // Calc window position

  lX = pData->ptPointer.x - ulCX / 2;
  if ( lX <= 0 )
    lX = 1;
  else if ( (lX + ulCX) >= lSrcCX )
    lX = lSrcCX - ulCX;

  lY = (pData->ptPointer.y - ulCY) - HINT_BOTTOM_OFFS;
  if ( lY <= 0 )
    lY = pData->ptPointer.y + HINT_TOP_OFFS;

  return WinSetWindowPos( hwnd, HWND_TOP, lX, lY, ulCX, ulCY,
                          SWP_MOVE | SWP_SIZE/* | SWP_ZORDER*/ );
}

static VOID _hintOff(HWND hwnd)
{
  PHINTDATA  pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );

  pData->hwndTag = NULLHANDLE;
  WinShowWindow( hwnd, FALSE );

  if ( pData->ulTimerId != 0 )
  {
    WinStopTimer( pData->hab, hwnd, HINT_ID_TIMER );
    pData->ulTimerId = 0;
  }
}


MRESULT EXPENTRY HintWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_CREATE:
      {
        PHINTDATA      pData = calloc( 1, sizeof(HINTDATA) );

        if ( pData == NULL )
          return (MRESULT)TRUE;  // Discontinue window creation

        pData->hab = WinQueryAnchorBlock( hwnd );
        pData->ulCXSens = WinQuerySysValue( HWND_DESKTOP, SV_CXDBLCLK ) / 2;
        pData->ulCYSens = WinQuerySysValue( HWND_DESKTOP, SV_CYDBLCLK ) / 2;
        WinSetWindowPtr( hwnd, 0, (PVOID)pData );

        WinSetPresParam( hwnd, PP_FONTNAMESIZE, 11, "9.WarpSans" );
      }
      break;

    case WM_DESTROY:
      {
        PHINTDATA      pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );

        if ( pData->pszText != NULL )
          free( pData->pszText );
        free( pData );
      }
      break;

    case WM_HITTEST:
      return (MPARAM)HT_TRANSPARENT;

/*    case WM_SHOW:
      {
        PHINTDATA	pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );

        if ( pData->ulTimerId != 0 )
        {
          WinStopTimer( pData->hab, hwnd, HINT_ID_TIMER );
          pData->ulTimerId = 0;
        }

        if ( SHORT1FROMMP( mp1 ) != 0 )
        {
          if ( !_hintSetWindow( hwnd ) )
            return (MRESULT)FALSE;

          // When window is visible we use timer to check mouse pointer.
          // If it moves - window should be hidden.
          pData->ulTimerId = WinStartTimer( pData->hab, hwnd,
                                            HINT_ID_TIMER, HINT_CHECK_DELAY );
        }
      }
      break;*/

    case WM_HINT_UPDATE:
      {
        PHINTDATA      pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );
        HWND           hwndTag = HWNDFROMMP(mp1);

        if ( ( hwndTag == NULLHANDLE || hwndTag == pData->hwndTag ) &&
             WinIsWindowVisible( hwnd ) )
        {
          _hintSetText( pData, (PSZ)PVOIDFROMMP(mp2) );
          _hintSetWindow( hwnd );
          WinInvalidateRect( hwnd, NULL, FALSE );
        }
      }
      return (MRESULT)FALSE;

    case WM_HINT_SET:
      {
        PHINTDATA      pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );
        HWND           hwndTag = HWNDFROMMP(mp1);
        POINTL         ptPointer;

        if ( hwndTag == NULLHANDLE )
        {
          // Specified target window handle is NULLHANDLE - tune off hint
          _hintOff( hwnd );
          return (MRESULT)TRUE;
        }

        WinQueryPointerPos( HWND_DESKTOP, &ptPointer );

        // Hint has been set for the requested window - ignore message.
        if ( hwndTag == pData->hwndTag )
        {
          if ( abs( ptPointer.x - pData->ptPointer.x ) > pData->ulCXSens ||
               abs( ptPointer.y - pData->ptPointer.y ) > pData->ulCYSens )
          {
            // Mouse moved while waiting to show - restart timer to show hint
            if ( !WinIsWindowVisible( hwnd ) && pData->ulTimerId != 0 )
            {
              WinStopTimer( pData->hab, hwnd, HINT_ID_TIMER );
              pData->ulTimerId = WinStartTimer( pData->hab, hwnd,
                                                HINT_ID_TIMER, HINT_SHOW_DELAY );
            }
          }

          return (MRESULT)TRUE;
        }

        pData->hwndTag = hwndTag;
        pData->ptPointer = ptPointer;
        _hintSetText( pData, (PSZ)PVOIDFROMMP(mp2) );

        // Hide window (stop timer). We'll need to show it again to get message
        // WM_SHOW and set the position of hint window.
        WinShowWindow( hwnd, FALSE );

        pData->ulTimerId = WinStartTimer( pData->hab, hwnd,
                                          HINT_ID_TIMER, HINT_SHOW_DELAY );
      }
      return (MRESULT)FALSE; // FALSE - hint is visible now

    case WM_TIMER:
      {
        USHORT         usTimer = SHORT1FROMMP(mp1);
        PHINTDATA      pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );
        POINTL         ptPointer;

        WinQueryPointerPos( HWND_DESKTOP, &ptPointer );
        if ( ( abs( ptPointer.x - pData->ptPointer.x ) > pData->ulCXSens ) ||
             ( abs( ptPointer.y - pData->ptPointer.y ) > pData->ulCYSens ) ||
             ( WinWindowFromPoint( HWND_DESKTOP, &ptPointer, TRUE ) !=
                 pData->hwndTag ) )
        {
          // The mouse pointer is moved - cancel display hint or delay-timer to
          // show hint.
          _hintOff( hwnd );
          return (MRESULT)FALSE;
        }

        if ( !WinIsWindowVisible( hwnd ) )
        {
          // Timed out before the advent hints.

          WinStopTimer( pData->hab, hwnd, usTimer );
          pData->ulTimerId = 0;

          if ( pData->hwndTag != NULLHANDLE )
          {
            if ( !_hintSetWindow( hwnd ) )
              return (MRESULT)FALSE;

            WinShowWindow( hwnd, TRUE );
            // When window is visible we use timer to check mouse pointer.
            // If it moves - window should be hidden.
            pData->ulTimerId = WinStartTimer( pData->hab, hwnd,
                                              HINT_ID_TIMER, HINT_CHECK_DELAY );
          }
        }
      }
      return (MRESULT)FALSE;

    case WM_HINT_UNSET:
      {
        HWND           hwndTag = HWNDFROMMP( mp1 );
        PHINTDATA      pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );

        if ( hwndTag == NULLHANDLE || hwndTag == pData->hwndTag )
          _hintOff( hwnd );
      }
      return (MRESULT)FALSE;

    case WM_PAINT:
      {
        PHINTDATA      pData = (PHINTDATA)WinQueryWindowPtr( hwnd, 0 );
        HPS            hps;
        RECTL          rclRect;

        WinQueryWindowRect( hwnd, &rclRect );

        hps = WinBeginPaint( hwnd, 0, NULL );
#ifndef HINT_USE_STD_COLORS
        GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );
#endif

        WinDrawBorder( hps, &rclRect, 1, 1,
                       HINT_CLR_BORDER, HINT_CLR_BACKGROUND,
                       DB_INTERIOR
#ifdef HINT_USE_STD_COLORS
                       | DB_AREAATTRS
#endif
                     );

        if ( pData->pszText != NULL )
        {
          // Correct window rectangle with borders and paddings
          rclRect.xLeft += 1 + HINT_TEXT_LPAD;
          rclRect.xRight -= 1 + HINT_TEXT_RPAD;
          rclRect.yBottom += 1 + HINT_TEXT_BPAD;
          rclRect.yTop -= 1 + HINT_TEXT_TPAD;
          // Draw text lines
          _hintDrawText( hps, &rclRect, pData->pszText, FALSE );
        }

        WinEndPaint( hps );
      }
      return (MRESULT)FALSE;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


HWND hintCreate(HWND hwndOwner)
{
  HWND       hwndHint;

  if ( !WinRegisterClass( WinQueryAnchorBlock( hwndOwner ), WC_HINT,
                          HintWndProc, CS_HITTEST | CS_SYNCPAINT,
                          sizeof(PHINTDATA) ) )
    return NULLHANDLE;

  hwndHint = WinCreateWindow( HWND_DESKTOP, WC_HINT, NULL, 0, 0, 0, 0, 0,
                              hwndOwner, HWND_TOP, 0, NULL, NULL );
  return hwndHint;
}

VOID hintDestroy(HWND hwnd)
{
  WinDestroyWindow( hwnd );
}
