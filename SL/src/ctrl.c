/*
    Dialog controls and windows utilites.
*/

#define INCL_WIN
#define INCL_GPI
#define INCL_WINWORKPLACE
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "ctrl.h"
#include "debug.h"     // Must be the last.

static PFNWP ColorRectWinProcOrg;


static MRESULT EXPENTRY ColorRectWinProc(HWND hwnd, ULONG msg,
                                         MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_PAINT:
      {
        LONG           lColor, lTextColor;
        ULONG          ulThick;
        RECTL          rectWin;
        CHAR           szBuf[128];
        HPS            hps = WinBeginPaint( hwnd, NULLHANDLE, NULL );

        // Switch HPS into RGB mode
        GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );

        WinQueryWindowRect( hwnd, &rectWin );

        if ( WinQueryPresParam( hwnd, PP_BACKGROUNDCOLOR, 0, NULL,
                                sizeof(LONG), &lColor, QPF_NOINHERIT ) == 0 )
          lColor = SYSCLR_DIALOGBACKGROUND;

        lTextColor = ~lColor & 0x00FFFFFF;
        ulThick = WinQueryWindowULong( hwnd, QWL_USER ) != 0 ? 2 : 1 ;
        WinDrawBorder( hps, &rectWin, ulThick, ulThick, lTextColor,
                       lColor, DB_INTERIOR );

        WinQueryWindowText( hwnd, sizeof(szBuf), szBuf );
        WinDrawText( hps, -1, szBuf, &rectWin, lTextColor, lColor,
                     DT_CENTER | DT_VCENTER );

        WinEndPaint( hps );
      }
      return (MRESULT)FALSE;

    case WM_FOCUSCHANGE:
      WinSetWindowULong( hwnd, QWL_USER, SHORT1FROMMP( mp2 ) );
      WinInvalidateRect( hwnd, NULL, FALSE );
      return (MRESULT)FALSE;

    case WM_BUTTON1DOWN:
    case WM_BUTTON2DOWN:
      WinSetFocus( HWND_DESKTOP, hwnd );
      return (MRESULT)FALSE;

    case WM_BUTTON1UP:
    case WM_BUTTON2UP:
      return (MRESULT)FALSE;

    case WM_CHAR:
      if ( ( (SHORT1FROMMP(mp1) & (KC_KEYUP | KC_VIRTUALKEY))
             != KC_VIRTUALKEY ) ||
           ( ( SHORT2FROMMP(mp2) != VK_ENTER ) &&
             ( SHORT2FROMMP(mp2) != VK_NEWLINE ) ) )
        break;
      // ENTER pressed - fall to open the palette.

    case WM_BUTTON1CLICK:
      {
        // Open "Mixed Color Palette" object
        HOBJECT        hObject = WinQueryObject( "<WP_HIRESCLRPAL>" );

        if ( hObject != NULLHANDLE )
          WinOpenObject( hObject, 0, TRUE );
      }
      return (MRESULT)FALSE;

    case WM_BUTTON2CLICK:
      {
        // Open "Solid Color Palette" object
        HOBJECT        hObject = WinQueryObject( "<WP_LORESCLRPAL>" );

        if ( hObject != NULLHANDLE )
          WinOpenObject( hObject, 0, TRUE );
      }
      return (MRESULT)FALSE;

    case WM_PRESPARAMCHANGED:
      switch ( LONGFROMMP( mp1 ) )
      {
        case PP_BACKGROUNDCOLOR:
          WinInvalidateRect( hwnd, NULL, FALSE );

          // [xCenter] notify owner; since the static control doesn't send
          // any notifications, we use EN_CHANGED
          WinSendMsg( WinQueryWindow( hwnd, QW_OWNER ), WM_CONTROL,
                      MPFROM2SHORT( WinQueryWindowUShort( hwnd, QWS_ID ),
                                    EN_CHANGE ),
                      MPFROMLONG( hwnd ) );
          break;
      }
      return (MRESULT)FALSE;
  }

  return ColorRectWinProcOrg( hwnd, msg, mp1, mp2 );
}


// BOOL ctrlStaticToColorRect(HWND hwndStatic, LONG lColor)
//
// "Convert" static text control to control to select color which can get
// colors from Color Palette.

BOOL ctrlStaticToColorRect(HWND hwndStatic, LONG lColor)
{
  PFNWP      pfnwp;

  if ( pfnwp = WinSubclassWindow( hwndStatic, ColorRectWinProc ) )
  {
    ColorRectWinProcOrg = pfnwp;
    WinSetPresParam( hwndStatic, PP_BACKGROUNDCOLOR, sizeof(LONG), &lColor );
    return TRUE;
  }

  return FALSE;
}


// VOID ctrlDlgCenterOwner(HWND hwnd)
//
// Centers dialog window hwnd by owner.

VOID ctrlDlgCenterOwner(HWND hwnd)
{
  RECTL      rectParent;
  POINTL     pt;
  SWP        swp;
  HWND       hwndOwner = WinQueryWindow( hwnd, QW_OWNER );
  ULONG      ulCXScr = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
  ULONG      ulCYScr = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

  WinQueryWindowRect( hwndOwner, &rectParent );
  WinQueryWindowPos( hwnd, &swp );
  pt.x = ( (rectParent.xRight - rectParent.xLeft) - swp.cx ) / 2;
  pt.y = ( (rectParent.yTop - rectParent.yBottom) - swp.cy ) / 2;
  WinMapWindowPoints( hwndOwner, HWND_DESKTOP, &pt, 1 );

  if ( pt.x < 0 )
    pt.x = 0;
  else if ( (pt.x + swp.cx) >= ulCXScr )
    pt.x = ulCXScr - swp.cx;

  if ( pt.y < 0 )
    pt.y = 0;
  else if ( (pt.y + swp.cy) >= ulCYScr )
    pt.y = ulCYScr - swp.cy;

  WinMapWindowPoints( HWND_DESKTOP, WinQueryWindow( hwnd, QW_PARENT ), &pt, 1 );
  WinSetWindowPos( hwnd, HWND_TOP, pt.x, pt.y, 0, 0, SWP_MOVE );
}


// ULONG ctrlQueryText(HWND hwnd, ULONG cbBuf, PCHAR pcBuf)
//
// Copies window text into a buffer. Removes leading and trailing spaces from
// the text.
// Returns length of text not including the null terminator. 

ULONG ctrlQueryText(HWND hwnd, ULONG cbBuf, PCHAR pcBuf)
{
  PCHAR      pCh;

  cbBuf = WinQueryWindowText( hwnd, cbBuf, pcBuf );

  // Removes leading and trailing spaces.
  while( cbBuf > 0 && isspace( pcBuf[cbBuf-1] ) ) cbBuf--;
  pcBuf[cbBuf] = '\0';
  pCh = pcBuf;
  while( isspace( *pCh ) ) pCh++;
  cbBuf -= ( pCh - pcBuf );
  memmove( pcBuf, pCh, cbBuf + 1 );

  return cbBuf;
}


VOID ctrlSetDefaultFont(HWND hwnd)
{
  ULONG      cbFont;
  PSZ        pszFont = utilGetDefaultFont( &cbFont );

  WinSetPresParam( hwnd, PP_FONTNAMESIZE, cbFont, pszFont );
}
