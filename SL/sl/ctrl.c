#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <string.h>
#include <ctype.h>
#include <ctrl.h>

static PFNWP ColorRectWinProcOrg;
static PFNWP DialogNBProcOrg;

/* Adjusting the position and size of a notebook window.
   Original function nb_adjust() from pm123. */
static VOID _nbAdjust(HWND hwnd)
{
  int    buttons_count = 0;
  HWND   notebook = NULLHANDLE;
  HENUM  henum;
  HWND   hnext;
  char   classname[128];
  RECTL  rect;
  POINTL pos[2];

  struct BUTTON {
    HWND hwnd;
    SWP  swp;
  } buttons[32];

  henum = WinBeginEnumWindows( hwnd );

  while(( hnext = WinGetNextWindow( henum )) != NULLHANDLE ) {
    if( WinQueryClassName( hnext, sizeof( classname ), classname ) > 0 ) {
      if( strcmp( classname, "#40" ) == 0 ) {
        notebook = hnext;
      } else if( strcmp( classname, "#3" ) == 0 ) {
        if( buttons_count < sizeof( buttons ) / sizeof( struct BUTTON )) {
          if( WinQueryWindowPos( hnext, &buttons[buttons_count].swp )) {
            if(!( buttons[buttons_count].swp.fl & SWP_HIDE )) {
              buttons[buttons_count].hwnd = hnext;
              buttons_count++;
            }
          }
        }
      }
    }
  }
  WinEndEnumWindows( henum );

  if( !WinQueryWindowRect( hwnd, &rect ) ||
      !WinCalcFrameRect  ( hwnd, &rect, TRUE ))
  {
    return;
  }

  // Resizes notebook window.
  if( notebook != NULLHANDLE )
  {
    pos[0].x = rect.xLeft;
    pos[0].y = rect.yBottom;
    pos[1].x = rect.xRight;
    pos[1].y = rect.yTop;

    if( buttons_count ) {
      WinMapDlgPoints( hwnd, pos, 2, FALSE );
      pos[0].y += 11;
      WinMapDlgPoints( hwnd, pos, 2, TRUE  );
    }

    WinSetWindowPos( notebook, 0, pos[0].x,  pos[0].y,
                                  pos[1].x - pos[0].x, pos[1].y - pos[0].y, SWP_MOVE | SWP_SIZE );
  }

  // Adjust buttons.
  if( buttons_count )
  {
    int total_width = buttons_count * 2;
    int start;
    int i;

    for( i = 0; i < buttons_count; i++ ) {
      total_width += buttons[i].swp.cx;
    }

    start = ( rect.xRight - rect.xLeft + 1 - total_width ) / 2;

    for( i = 0; i < buttons_count; i++ ) {
      WinSetWindowPos( buttons[i].hwnd, 0, start, buttons[i].swp.y, 0, 0, SWP_MOVE );
      WinInvalidateRect( buttons[i].hwnd, NULL, FALSE );
      start += buttons[i].swp.cx + 2;
    }
  }
}

static MRESULT EXPENTRY ColorRectWinProc(HWND hwnd, ULONG msg,
                                         MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_PAINT:
      {
        LONG		lColor, lTextColor;
        ULONG		ulThick;
        RECTL		rectWin;
        CHAR		szBuf[128];
        HPS		hps = WinBeginPaint( hwnd, NULLHANDLE, NULL );

        // Switch HPS into RGB mode.
        GpiCreateLogColorTable( hps, 0, LCOLF_RGB, 0, 0, NULL );

        WinQueryWindowRect( hwnd, &rectWin );

        if ( WinQueryPresParam( hwnd, PP_BACKGROUNDCOLOR, 0, NULL,
                                sizeof(LONG), &lColor, QPF_NOINHERIT ) == 0 )
          lColor = SYSCLR_DIALOGBACKGROUND;

        lTextColor = ~lColor & 0x00FFFFFF;
        ulThick = WinQueryWindowULong( hwnd, QWL_USER ) != 0 ? 2 : 1 ;
        WinDrawBorder( hps, &rectWin, ulThick, ulThick, lTextColor,
                       lColor, DB_INTERIOR );

        WinQueryWindowText( hwnd, sizeof(szBuf), &szBuf );

        WinDrawText( hps, strlen( &szBuf ), &szBuf, &rectWin,
                     lTextColor, lColor, DT_CENTER | DT_VCENTER );

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
/*      if ( WinQueryWindowULong( hwnd, QWL_USER ) != 0 )
      {
        HOBJECT	hobj = WinQueryObject( "<WP_HIRESCLRPAL>" );
        WinSetObjectData( hobj, "OPEN=DEFAULT" );
      }*/
      return (MRESULT)FALSE;

    case WM_BUTTON1CLICK:
      return (MRESULT)FALSE;

    case WM_BUTTON1DBLCLK:
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

static MRESULT EXPENTRY DialogNBProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      ctrlDlgCenterOwner( hwnd );
      _nbAdjust( hwnd );
      break;

    case WM_WINDOWPOSCHANGED:
      if ( ((PSWP)mp1)[0].fl & SWP_SIZE )
        _nbAdjust( hwnd );
      break;
  }

  return DialogNBProcOrg( hwnd, msg, mp1, mp2 );
}


// BOOL ctrlStaticToColorRect(HWND hwndStatic, LONG lColor)
//
// "Convert" static text control to control to select color which can get
// colors from Color Palette.

BOOL ctrlStaticToColorRect(HWND hwndStatic, LONG lColor)
{
  PFNWP		pfnwp;

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
  RECTL			rectParent;
  POINTL		pt;
  SWP			swp;
  HWND			hwndOwner = WinQueryWindow( hwnd, QW_OWNER );
  ULONG			ulCXScr = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
  ULONG			ulCYScr = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

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
  PCHAR		pCh;

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


// BOOL ctrlSubclassNotebook(HWND hwndDlg)
//
// Subclasses dialog window with notebook:
// - Centers dialog window by owner.
// - Automatically change the notebook size and location of the buttons
//   (at the bottom and horizontal center).

BOOL ctrlSubclassNotebook(HWND hwndDlg)
{
  PFNWP		pfnwp;

  if ( pfnwp = WinSubclassWindow( hwndDlg, DialogNBProc ) )
  {
    DialogNBProcOrg = pfnwp;
    ctrlDlgCenterOwner( hwndDlg );
    _nbAdjust( hwndDlg );
    return TRUE;
  }

  return FALSE;
}
