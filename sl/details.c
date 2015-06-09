#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"
#include "items.h"
#include "debug.h"

#define WC_SL_DETAILS		"SLDetails"

extern HINI		hIni;		// sl.c
extern HWND		hwndMain;	// sl.c

static PFNWP		fnDetailsFrameOrgProc;


MRESULT EXPENTRY DetailsFrameWndProc(HWND hwnd, ULONG msg, MPARAM mp1,
                                     MPARAM mp2)
{
  switch( msg )
  {
    case WM_MOUSEMOVE:
      // Always vertical arrows up and down over frame.
      WinSetPointer( HWND_DESKTOP,
                     WinQuerySysPointer( HWND_DESKTOP, SPTR_SIZENS, FALSE ) );
      return (MRESULT)FALSE;

    case WM_ADJUSTFRAMEPOS:
      // Ajust window to the bottom of parent window. Hide left, right and
      // bottom edges of the frame outside the parent window.
      {
        PSWP		pswp = (PSWP)mp1;
        ULONG		ulCXSizeBorder = WinQuerySysValue( HWND_DESKTOP, SV_CXSIZEBORDER );
        ULONG		ulCYSizeBorder = WinQuerySysValue( HWND_DESKTOP, SV_CYSIZEBORDER );
        SWP		swpParent;

        WinQueryWindowPos( WinQueryWindow( hwnd, QW_PARENT ), &swpParent );
        pswp->x = -ulCXSizeBorder;
        pswp->y = -ulCYSizeBorder;
        pswp->cx = swpParent.cx + 2 * ulCXSizeBorder;
      }
      return (MRESULT)FALSE;

    case WM_ACTIVATE:
      // We notify the main window that details window activated.
      // Main window on this message must activate a list window. Ie detailt
      // must be never activated.
      if ( SHORT1FROMMP(mp1) != 0 )
        WinPostMsg( WinQueryWindow( WinQueryWindow( hwnd, QW_PARENT ),
                    QW_PARENT ),
                    WM_SL_DETAILSACTIVATE, 0, 0 );
      return (MRESULT)FALSE;
  }

  return fnDetailsFrameOrgProc( hwnd, msg, mp1, mp2 );
}

MRESULT EXPENTRY DetailsWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_CREATE:
      {
        ULONG		cbFont = 0;
        CHAR		szFont[48];
        LONG		lForeCol = DEF_DETAILSFORECOL;
        LONG		lBackCol = DEF_DETAILSBACKCOL;

        // Set details window font and colors
        if ( hIni != NULLHANDLE )
        {
          cbFont = utilQueryProfileStr( hIni, INI_APP, INI_KEY_DETAILSFONT,
                                        NULL, &szFont, sizeof(szFont) - 1 );
          lForeCol = utilQueryProfileLong( hIni, INI_APP, INI_KEY_DETAILSFORECOL,
                                           DEF_DETAILSFORECOL );
          lBackCol = utilQueryProfileLong( hIni, INI_APP, INI_KEY_DETAILSBACKCOL,
                                           DEF_DETAILSBACKCOL );
        }

        if ( cbFont == 0 )
          cbFont = sprintf( &szFont, 
                            utilTestFont( "Workplace Sans" ) ?
                            "9.Workplace Sans Bold" :
                            utilTestFont( "WarpSans" ) ?
                            "9.WarpSans" : "8.Helvetica Bold" );

        WinSetPresParam( hwnd, PP_FONTNAMESIZE, cbFont + 1, &szFont );
        WinSetPresParam( hwnd, PP_FOREGROUNDCOLOR, sizeof(LONG), &lForeCol );
        WinSetPresParam( hwnd, PP_BACKGROUNDCOLOR, sizeof(LONG), &lBackCol );
      }
      break;

    case WM_DESTROY:
      {
        HPS			hpsMem = WinQueryWindowULong( hwnd, 0 );
        HDC			hdcMem;

        if ( hpsMem == NULLHANDLE )
        {
          GpiDeleteBitmap( GpiSetBitmap( hpsMem, NULLHANDLE ) );
          hdcMem = GpiQueryDevice( hpsMem );
          GpiDestroyPS( hpsMem );
          DevCloseDC( hdcMem );
        }
      }
      break;

    case WM_PRESPARAMCHANGED:
      // Check visibility state of a window to prevent 
      // reactions when WinSetPresParam() called from WM_CREATE.
      if ( !WinIsWindowVisible( hwnd ) )
        break;

      switch( LONGFROMMP( mp1 ) )
      {
        case 0:
        case PP_BACKGROUNDCOLOR:
        case PP_FOREGROUNDCOLOR:
          {
            // Store new colors to the ini-file
            LONG		lCol;

            if ( WinQueryPresParam( hwnd, PP_FOREGROUNDCOLOR, 0, NULL,
                                    sizeof(LONG), &lCol, QPF_NOINHERIT ) != 0 )
              utilWriteProfileLong( hIni, INI_APP, INI_KEY_DETAILSFORECOL, lCol );

            if ( WinQueryPresParam( hwnd, PP_BACKGROUNDCOLOR, 0, NULL,
                                    sizeof(LONG), &lCol, QPF_NOINHERIT ) != 0 )
              utilWriteProfileLong( hIni, INI_APP, INI_KEY_DETAILSBACKCOL, lCol );

          }
          break;

        case PP_FONTNAMESIZE:
          {
            HPS			hpsMem = WinQueryWindowULong( hwnd, 0 );
            HPS			hps;
            CHAR		szBuf[128];

            // Copy new font to the memory presentation space.
            if ( hpsMem != NULLHANDLE )
            {
              hps = WinGetPS( hwnd );
              utilSetFontFromPS( hpsMem, hps, 1 );
              WinReleasePS( hps );
            }

            // Store new window font to the ini-file
            if ( WinQueryPresParam( hwnd, PP_FONTNAMESIZE, 0, NULL,
                                    sizeof(szBuf), &szBuf, QPF_NOINHERIT ) != 0 )
              PrfWriteProfileString( hIni, INI_APP, INI_KEY_DETAILSFONT, &szBuf );
          }
          break;
      }

      WinInvalidateRect( hwnd, NULL, FALSE );
      break;

    case WM_SIZE:
      // Inform main window that height has changed.
      if ( SHORT2FROMMP( mp1 ) != SHORT2FROMMP( mp2 ) )
        WinPostMsg( hwndMain, WM_SL_DETAILSSIZE, mp1, mp2 );

      {
        BITMAPINFOHEADER2	bmi = { 0 };
        HBITMAP			hbmMem;
        HPS			hpsMem = WinQueryWindowULong( hwnd, 0 );
        HPS			hps;
        LONG			alFormats[4];
        HAB			hab = WinQueryAnchorBlock( hwnd );

        hps = WinGetPS( hwnd );

        if ( hpsMem == NULLHANDLE )
        {
          // Create a memory presentation space

          HDC			hdcMem;
          SIZEL			size = { 0, 0 };

          hdcMem = DevOpenDC( hab, OD_MEMORY, "*", 0, NULL, hps );
          if ( hdcMem == NULLHANDLE )
          {
            WinReleasePS( hps );
            return (MRESULT)FALSE;
          }
          hpsMem = GpiCreatePS( hab, hdcMem, &size,
                     PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC );
          if ( hpsMem == NULLHANDLE )
          {
            WinReleasePS( hps );
            DevCloseDC( hdcMem );
            return (MRESULT)FALSE;
          }

          utilSetFontFromPS( hpsMem, hps, 1 );
          WinSetWindowULong( hwnd, 0, hpsMem );
        }

        // Set bitmap with current window size to the memory presentation space

        // Request most closely formats of bitmaps matches the device.
        GpiQueryDeviceBitmapFormats( hps, 4, &alFormats );
        WinReleasePS( hps );
        // Create a bitmap and set for the memory presentation space.
        bmi.cbFix         = sizeof(BITMAPINFOHEADER2);
        bmi.cx            = SHORT1FROMMP(mp2); // Bitmap size equals the size
        bmi.cy            = SHORT2FROMMP(mp2); // of the window.
        bmi.cPlanes       = (USHORT)alFormats[0]; //1;
        bmi.cBitCount     = (USHORT)alFormats[1]; //24;
        hbmMem = GpiSetBitmap( hpsMem,
                               GpiCreateBitmap( hpsMem, &bmi, 0, NULL, NULL ) );
        // Destroy old bitmap.
        if ( hbmMem != NULLHANDLE )
          GpiDeleteBitmap( hbmMem );
      }

      return (MRESULT)FALSE;

    case WM_PAINT:
      {
        HPS		hpsMem = WinQueryWindowULong( hwnd, 0 );
        HPS		hps, hpsPaint;
        RECTL		rclRect;
        SIZEL		sizeWnd;
        POINTL		pnt;
        LONG		lForeCol = DEF_DETAILSFORECOL;
        LONG		lBackCol = DEF_DETAILSBACKCOL;

        WinQueryPresParam( hwnd, PP_FOREGROUNDCOLOR, 0,
                           NULL, sizeof(LONG), &lForeCol, QPF_NOINHERIT );
        WinQueryPresParam( hwnd, PP_BACKGROUNDCOLOR, 0,
                           NULL, sizeof(LONG), &lBackCol, QPF_NOINHERIT );

        WinQueryWindowRect( hwnd, &rclRect );

        hps = WinBeginPaint( hwnd, 0L, NULL );

        rclRect.yTop--;

        // Horisontal line at the top of client area (under frame)
        GpiSetColor( hps, SYSCLR_TITLEBOTTOM );
        pnt.x = 0;
        pnt.y = rclRect.yTop;
        GpiMove( hps, &pnt );
        pnt.x = rclRect.xRight;
        GpiLine( hps, &pnt );

        sizeWnd.cx = rclRect.xRight - rclRect.xLeft;
        sizeWnd.cy = rclRect.yTop - rclRect.yBottom;

        if ( hpsMem != NULLHANDLE )
          // Paint memory presentation space.
          hpsPaint = hpsMem;
        else
          // Memory presentation space was not created for some reason - direct
          // paint in window's presentation space.
          hpsPaint = hps;

        // Switch HPS into RGB mode.
        GpiCreateLogColorTable( hpsPaint, 0, LCOLF_RGB, 0, 0, NULL );
        lBackCol = GpiQueryRGBColor( hpsPaint, 0, lBackCol );
        lForeCol = GpiQueryRGBColor( hpsPaint, 0, lForeCol );
        // Erase background
        WinFillRect( hpsPaint, &rclRect, lBackCol );
        GpiSetColor( hpsPaint, lForeCol );
        GpiSetBackColor( hpsPaint, lBackCol );
        itemsPaintDetails( hpsPaint, sizeWnd );

        if ( hpsMem != NULLHANDLE )
        {
          POINTL	aPoints[3];

          // Copy result image to the window presentation space.
          aPoints[0].x = 0;
          aPoints[0].y = 0;
          aPoints[1].x = sizeWnd.cx;
          aPoints[1].y = sizeWnd.cy;
          aPoints[2].x = 0;
          aPoints[2].y = 0;
          GpiBitBlt( hps, hpsMem, 3, aPoints, ROP_SRCCOPY, 0 );
        }

        WinEndPaint( hps );
      }
      return (MRESULT)FALSE;

    case WM_SL_UPDATE_DETAILS:
      WinInvalidateRect( hwnd, NULL, FALSE );
      return (MRESULT)FALSE;
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


HWND dtlInstall(HWND hwndParent, ULONG ulId)
{
  HWND		hwnd, hwndFrame;
  ULONG		ulFrameFlags = FCF_SIZEBORDER;

  // WC_SL_DETAILS window data:
  //   bytes 0-3 - handle of memory presentation space
  if ( !WinRegisterClass( WinQueryAnchorBlock( hwndParent ), WC_SL_DETAILS,
                    DetailsWndProc, CS_SIZEREDRAW, sizeof(ULONG) ) )
  {
    debug( "WinRegisterClass() fail" );
    return NULLHANDLE;
  }

  // Create details window
  hwndFrame = WinCreateStdWindow( hwndParent, WS_VISIBLE, &ulFrameFlags,
                                  WC_SL_DETAILS, NULL, 0, 0, ulId, &hwnd );
  if ( hwndFrame == NULLHANDLE )
  {
    debug( "WinCreateStdWindow() fail" );
    return NULLHANDLE;
  }

  // Replace frame window procedure, store original procedure pointer
  fnDetailsFrameOrgProc = WinSubclassWindow( hwndFrame, DetailsFrameWndProc );

  return fnDetailsFrameOrgProc != NULL ? hwnd : NULLHANDLE;
}
