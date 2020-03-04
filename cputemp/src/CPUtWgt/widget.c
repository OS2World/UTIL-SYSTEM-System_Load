#pragma strings(readonly)

#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT
#define INCL_WINSWITCHLIST
#define INCL_WINRECTANGLES
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINLISTBOXES
#define INCL_WINENTRYFIELDS
#define INCL_WINTIMER
#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#define INCL_WINMENUS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#ifdef __WATCOMC__
#define __IBMC__
#endif
#define DONT_REPLACE_MALLOC             // in case mem debug is enabled
#define DONT_REPLACE_FOR_DBCS           // do not replace strchr with DBCS version
#include "setup.h"                      // code generation and debugging options

// disable wrappers, because we're not linking statically
#ifdef DOSH_STANDARDWRAPPERS
    #undef DOSH_STANDARDWRAPPERS
#endif
#ifdef WINH_STANDARDWRAPPERS
    #undef WINH_STANDARDWRAPPERS
#endif

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\prfh.h"               // INI file helper routines;
                                        // this include is required for some
                                        // of the structures in shared\center.h
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers
#include "helpers\comctl.h"

// XWorkplace implementation headers
#include "shared\center.h"              // public XCenter interfaces
#include "shared\common.h"              // the majestic XWorkplace include file

#pragma hdrstop                         // VAC++ keeps crashing otherwise


#include "cputemp.h"                    // CPUTEMP.DLL API.
#include "cput.h"                       // Resource IDs.

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

#define UPDATE_DELTA_TIME	3000
#define BAR_WIDTH		10
#define BARCLR_1		0x0000E010
#define BARCLR_2		0x00E0EA00
#define BARCLR_3		0x00F00000

#define MENU_CMD_CELSIUS	2000
#define MENU_CMD_FAHRENHEIT     2001

/* ******************************************************************
 *
 *   XCenter widget class definition
 *
 ********************************************************************/

/*
 *      This contains the name of the PM window class and
 *      the XCENTERWIDGETCLASS definition(s) for the widget
 *      class(es) in this DLL.
 */

#define WNDCLASS_WIDGET_CPUTEMP "XWPCenterCPUTempWidget"

static const XCENTERWIDGETCLASS G_WidgetClasses[] =
{
  WNDCLASS_WIDGET_CPUTEMP,    // PM window class name.
  0,                          // Additional flag, not used here.
  "CPUTemp",                  // Internal widget class name.
  "CPU Temperature",          // Widget class name displayed to user.
  WGTF_UNIQUEPERXCENTER |     // Widget class flags.
  WGTF_TRAYABLE |
  WGTF_TOOLTIP,
  NULL                        // No settings dialog.
};

/* ******************************************************************
 *
 *   Function imports from XFLDR.DLL
 *
 ********************************************************************/

/*
 *      The actual imports are then made by WgtInitModule.
 */

// resolved function pointers from XFLDR.DLL
PCMNQUERYDEFAULTFONT pcmnQueryDefaultFont = NULL;

PCTRDISPLAYHELP pctrDisplayHelp = NULL;
PCTRFREESETUPVALUE pctrFreeSetupValue = NULL;
PCTRPARSECOLORSTRING pctrParseColorString = NULL;
PCTRSCANSETUPSTRING pctrScanSetupString = NULL;
PCTRSETSETUPSTRING pctrSetSetupString = NULL;

PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;
PGPIHSWITCHTORGB pgpihSwitchToRGB = NULL;

PWINHFREE pwinhFree = NULL;
PWINHQUERYPRESCOLOR pwinhQueryPresColor = NULL;
PWINHQUERYWINDOWFONT pwinhQueryWindowFont = NULL;
PWINHSETWINDOWFONT pwinhSetWindowFont = NULL;
PWINHINSERTMENUITEM pwinhInsertMenuItem = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCLEAR pxstrClear = NULL;
PXSTRINIT pxstrInit = NULL;
PXSTRCPY pxstrcpy = NULL;

static const RESOLVEFUNCTION G_aImports[] =
{
  "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,

  "ctrDisplayHelp", (PFN*)&pctrDisplayHelp,
  "ctrFreeSetupValue", (PFN*)&pctrFreeSetupValue,
  "ctrParseColorString", (PFN*)&pctrParseColorString,
  "ctrScanSetupString", (PFN*)&pctrScanSetupString,
  "ctrSetSetupString", (PFN*)&pctrSetSetupString,

  "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
  "gpihSwitchToRGB", (PFN*)&pgpihSwitchToRGB,

  "winhFree", (PFN*)&pwinhFree,
  "winhQueryPresColor", (PFN*)&pwinhQueryPresColor,
  "winhQueryWindowFont", (PFN*)&pwinhQueryWindowFont,
  "winhSetWindowFont", (PFN*)&pwinhSetWindowFont,
  "winhInsertMenuItem", (PFN*)&pwinhInsertMenuItem,

  "xstrcat", (PFN*)&pxstrcat,
  "xstrClear", (PFN*)&pxstrClear,
  "xstrInit", (PFN*)&pxstrInit,
  "xstrcpy", (PFN*)&pxstrcpy
};

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

static HMODULE		hMod = NULLHANDLE;

/*
 *@@ CPUTEMPSETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of CPUTEMPPRIVATE.
 *
 *      Putting these settings into a separate structure
 *      is no requirement technically. However, once the
 *      widget uses a settings dialog, the dialog must
 *      support changing the widget settings even if the
 *      widget doesn't currently exist as a window, so
 *      separating the setup data from the widget window
 *      data will come in handy for managing the setup
 *      strings.
 */

typedef struct _CPUTEMPSETUP {
    LONG        lcolBackground,      // background color
                lcolForeground;      // foreground color (for text)

    PSZ         pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!

    BOOL        fFahrenheit;
} CPUTEMPSETUP, *PCPUTEMPSETUP;

/*
 *@@ CPUTEMPPRIVATE:
 *      more window data for the widget.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpWidget and stored in XCENTERWIDGET.pUser.
 */

typedef struct _CPUTEMPPRIVATE {
  PXCENTERWIDGET pWidget;         // Reverse ptr to general widget data ptr.
  CPUTEMPSETUP   Setup;           // Settings that correspond to a setup string.
  ULONG          ulTimerId;       // Timer Id for updates.

  ULONG          ulMaxTemp;
  ULONG          ulCurTemp;

  BOOL           fTooltipShowing;
          // TRUE only while tooltip is currently showing over this widget.
  XSTRING        strTooltipText; // Tooltip text.
  BOOL           fContextMenuHacked;
} CPUTEMPPRIVATE, *PCPUTEMPPRIVATE;


static ULONG _convToFahrenheit(ULONG ulTemp)
{
  return 9 * ulTemp / 5 + 32;
}

// BOOL _queryTemp(PXCENTERWIDGET pWidget)
//
// - Reads sensors vaules, detect maximum of current temperatures of all CPUs
// and write it in CPUTEMPPRIVATE.ulCurTemp, write maximum T. allowed on
// installed CPUs to CPUTEMPPRIVATE.ulMaxTemp.
// - Updates tip text.

static BOOL _queryTemp(PXCENTERWIDGET pWidget)
{
  PCPUTEMPPRIVATE	pPrivate = (PCPUTEMPPRIVATE)pWidget->pUser;
  CPUTEMP		aCPUList[64];
  PCPUTEMP		pCPU = &aCPUList;
  ULONG			cCPU;
  ULONG			ulIdx;
  CHAR			szUM[16];
  CHAR			szBuf[128];
  ULONG			cbCPU, cbBuf;
  ULONG			ulRC;
  ULONG			ulId;

  pPrivate->ulCurTemp = 0;
  pPrivate->ulMaxTemp = 0;

  // Query current temperatures of all processors, maximum allowed T. and
  // number of processors.
  ulRC = cputempQuery( sizeof(aCPUList) / sizeof(CPUTEMP), &aCPUList, &cCPU );

  if ( ulRC != CPUTEMP_OK )
  {
    // Error...
    pPrivate->ulMaxTemp = 0;

    switch( ulRC )
    {
      case CPUTEMP_NO_DRIVER:
        ulId = IDS_NO_DRIVER;
        break;

      default:
        cbBuf = sprintf( &szBuf, "Error code: %u", ulRC );
        ulId = 0;
    }

    if ( ulId != 0 )
      cbBuf = WinLoadString( pWidget->pGlobals->hab, hMod, ulId, sizeof(szBuf),
                             &szBuf );

    pxstrcpy( &pPrivate->strTooltipText, &szBuf, cbBuf );
    return FALSE;
  }

  // Reset destination string.
  pxstrcpy( &pPrivate->strTooltipText, NULL, 0 );

  // Load "CPU "
  cbCPU = WinLoadString( pWidget->pGlobals->hab, hMod, IDS_CPU, 16, &szBuf );
  // Load "øC" or "øF"
  WinLoadString( pWidget->pGlobals->hab, hMod,
                 pPrivate->Setup.fFahrenheit ?
                   IDS_FAHRENHEIT_UM : IDS_CELSIUS_UM,
                 sizeof(szUM), &szUM );

  // Creating text for tip and determination of the maximum temperature of
  // processors.
  for( ulIdx = 0; ulIdx < cCPU; ulIdx++, pCPU++ )
  {
    cbBuf = cbCPU + sprintf( &szBuf[cbCPU], " %u: ", ulIdx ); // "CPU n: "

    if ( ( pCPU->ulCode != CPU_OK ) && ( pCPU->ulCode != CPU_OK_OFFLINE ) )
    {
      switch( pCPU->ulCode )
      {
        case CPU_UNSUPPORTED:
          ulId = IDS_UNSUPPORTED_CPU;
          break;

        case CPU_OFFLINE:
          ulId = IDS_OFFLINE;
          break;

        case CPU_INVALID_VALUE:
          ulId = IDS_INVALID_VALUE;
          break;

        default:
          cbBuf += sprintf( &szBuf[cbBuf], "Error %u", pCPU->ulCode );
          ulId = 0;
      }

      if ( ulId != 0 )
        cbBuf += WinLoadString( pWidget->pGlobals->hab, hMod, ulId,
                                sizeof(szBuf) - cbBuf - 2, &szBuf[cbBuf] );
    }
    else
    {
      // Append T. value and UM (like "40 øC\n") to the string "CPU n: "
      cbBuf += sprintf( &szBuf[cbBuf], "%u %s",
                        pPrivate->Setup.fFahrenheit
                          ? _convToFahrenheit( pCPU->ulCurTemp )
                          : pCPU->ulCurTemp,
                        &szUM );

      if ( pCPU->ulCode == CPU_OK_OFFLINE )
      {
        *((PUSHORT)&szBuf[cbBuf]) = '# ';
        cbBuf += 2;
      }

      // New maximum value to display.
      if ( pPrivate->ulCurTemp < pCPU->ulCurTemp )
      {
        pPrivate->ulCurTemp = pCPU->ulCurTemp;
        pPrivate->ulMaxTemp = pCPU->ulMaxTemp;
      }
    }

    *((PUSHORT)&szBuf[cbBuf]) = '\0\n';
    cbBuf += 1;

    // Append prepared string to the tip text.
    pxstrcat( &pPrivate->strTooltipText, &szBuf, cbBuf );
  }

  if ( pPrivate->fTooltipShowing )
    // Tooltip currently showing - refresh it.
    WinSendMsg( pPrivate->pWidget->pGlobals->hwndTooltip, TTM_UPDATETIPTEXT,
                (MPARAM)pPrivate->strTooltipText.psz, 0 );

  return TRUE;
}


/* ******************************************************************
 *
 *   Widget setup management
 *
 ********************************************************************/

/*
 *      This section contains shared code to manage the
 *      widget's settings. This can translate a widget
 *      setup string into the fields of a binary setup
 *      structure and vice versa. This code is used by
 *      an open widget window, but could be shared with
 *      a settings dialog, if you implement one.
 */

/*
 *@@ WgtClearSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID WgtClearSetup(PCPUTEMPSETUP pSetup)
{
  if ( pSetup != NULL )
  {
    if ( pSetup->pszFont != NULL )
    {
      free( pSetup->pszFont );
      pSetup->pszFont = NULL;
    }
  }
}

/*
 *@@ WgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 */

VOID WgtScanSetup(PCSZ pcszSetupString, PCPUTEMPSETUP pSetup)
{
  PSZ p;

  // Background color.
  if ( p = pctrScanSetupString( pcszSetupString, "BGNDCOL" ) )
  {
    pSetup->lcolBackground = pctrParseColorString( p );
    pctrFreeSetupValue(p);
  }
  else
    // Default color:
    pSetup->lcolBackground = WinQuerySysColor( HWND_DESKTOP,
                                               SYSCLR_DIALOGBACKGROUND, 0 );

  // Text color.
  if ( p = pctrScanSetupString( pcszSetupString, "TEXTCOL" ) )
  {
    pSetup->lcolForeground = pctrParseColorString( p );
    pctrFreeSetupValue( p );
  }
  else
    pSetup->lcolForeground = WinQuerySysColor( HWND_DESKTOP,
                                               SYSCLR_WINDOWSTATICTEXT, 0 );

  // Font: we set the font presparam, which automatically affects the cached
  // presentation spaces.
  if ( p = pctrScanSetupString( pcszSetupString, "FONT" ) )
  {
    pSetup->pszFont = strdup( p );
    pctrFreeSetupValue( p );
  }
  // else: leave this field null

  // Temperature units: FAHR is '1' - fahrenheit.
  if ( p = pctrScanSetupString( pcszSetupString, "FAHR" ) )
  {
    pSetup->fFahrenheit = *p == '1';
    pctrFreeSetupValue( p );
  }
}

/*
 *@@ WgtSaveSetup:
 */

VOID WgtSaveSetup(HWND hwnd, PCPUTEMPSETUP pSetup)
{
  XSTRING	strSetup;
  CHAR		szTemp[100];

  pxstrInit( &strSetup, 100 );

  sprintf( &szTemp, "BGNDCOL=%06lX;", pSetup->lcolBackground );
  pxstrcat( &strSetup, &szTemp, 0 );

  sprintf( &szTemp, "TEXTCOL=%06lX;", pSetup->lcolForeground );
  pxstrcat( &strSetup, &szTemp, 0 );

  if ( pSetup->pszFont != NULL )
  {
    // non-default font:
    sprintf( &szTemp, "FONT=%s;", pSetup->pszFont );
    pxstrcat( &strSetup, &szTemp, 0 );
  }

  sprintf( &szTemp, "FAHR=%c;", pSetup->fFahrenheit ? '1' : '0' );
  pxstrcat( &strSetup, &szTemp, 0 );

  WinSendMsg( WinQueryWindow( hwnd, QW_PARENT ), XCM_SAVESETUP,
              MPFROMHWND( hwnd ), MPFROMP( strSetup.psz ) );
  pxstrClear( &strSetup );
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

// None currently. To see how a setup dialog can be done,
// see the window list widget (w_winlist.c).

/* ******************************************************************
 *
 *   Callbacks stored in XCENTERWIDGETCLASS
 *
 ********************************************************************/

// If you implement a settings dialog, you must write a
// "show settings dlg" function and store its function pointer
// in XCENTERWIDGETCLASS.

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *      This code has the actual PM window class.
 */

/*
 *@@ WgtCreate:
 *      implementation for WM_CREATE.
 */

MRESULT WgtCreate(HWND hwnd, PXCENTERWIDGET pWidget)
{
  MRESULT		mrc = 0;
  PCPUTEMPPRIVATE	pPrivate = calloc( 1, sizeof(CPUTEMPPRIVATE) );

  // Link the two together.
  pWidget->pUser = pPrivate;
  pPrivate->pWidget = pWidget;

  // Initialize binary setup structure from setup string.
  WgtScanSetup( pWidget->pcszSetupString, &pPrivate->Setup );

  // Set window font (this affects all the cached presentation
  // spaces we use in WM_PAINT).
  pwinhSetWindowFont( hwnd,
                      pPrivate->Setup.pszFont != NULL
                       ? pPrivate->Setup.pszFont
                       // default font: use the same as in the rest of XWorkplace
                       // (either 9.WarpSans or 8.Helv)
                       : pcmnQueryDefaultFont() );

  pxstrInit( &pPrivate->strTooltipText, 100 );

  if ( _queryTemp( pWidget ) )
    pPrivate->ulTimerId = WinStartTimer( pWidget->pGlobals->hab, hwnd, 1,
                                         UPDATE_DELTA_TIME );

  return mrc;
}

/*
 *@@ MwgtControl:
 *      implementation for WM_CONTROL.
 *
 *      The XCenter communicates with widgets thru
 *      WM_CONTROL messages. At the very least, the
 *      widget should respond to XN_QUERYSIZE because
 *      otherwise it will be given some dumb default
 *      size.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

BOOL WgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  BOOL           brc = FALSE;
  // get widget data from QWL_USER (stored there by WM_CREATE)
  PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr( hwnd, QWL_USER );

  if ( pWidget != NULL )
  {
    // get private data from that widget data
    PCPUTEMPPRIVATE pPrivate = (PCPUTEMPPRIVATE)pWidget->pUser;

    if ( pPrivate != NULL )
    {
      USHORT  usID = SHORT1FROMMP(mp1),
              usNotifyCode = SHORT2FROMMP(mp1);

      // is this from the XCenter client?
      switch( usID )
      {
        case ID_XCENTER_CLIENT:

          switch( usNotifyCode )
          {
            case XN_QUERYSIZE:	// XCenter wants to know our size.
              {
                PSIZEL	pszl = (PSIZEL)mp2;
                POINTL	aptText[TXTBOX_COUNT];
                HPS	hps = WinGetPS( hwnd );
                CHAR	szBuf[32];

                brc = TRUE;
                pszl->cy = 20;      // Desired minimum height.

                // Make string "000 øF".
                *((PULONG)&szBuf) = (ULONG)' 000';
                WinLoadString( pWidget->pGlobals->hab, hMod, IDS_FAHRENHEIT_UM,
                               sizeof(szBuf) - 4, &szBuf[4] );

                // Query visible width of the text..
                if ( GpiQueryTextBox( hps, 6, &szBuf, TXTBOX_COUNT, &aptText ) )
                  pszl->cx = aptText[TXTBOX_TOPRIGHT].x -
                             aptText[TXTBOX_TOPLEFT].x;
                else
                  pszl->cx = 40;      // Default text width.

                pszl->cx += 3 + BAR_WIDTH; // Add bar's width and 3px space.

                WinReleasePS( hwnd );
              }
              break;

            /*
             * XN_SETUPCHANGED:
             *      XCenter has a new setup string for
             *      us in mp2.
             *
             *      NOTE: This only comes in with settings
             *      dialogs. Since we don't have one, this
             *      really isn't necessary.
             */

            case XN_SETUPCHANGED:
              {
                PCSZ pcszNewSetupString = (const char*)mp2;

                // Reinitialize the setup data.
                WgtClearSetup( &pPrivate->Setup );
                WgtScanSetup( pcszNewSetupString, &pPrivate->Setup );

                WinInvalidateRect( pWidget->hwndWidget, NULL, FALSE );
              }
              break;
          }

          break;

        case ID_XCENTER_TOOLTIP:

          switch ( usNotifyCode )
          {
            case TTN_NEEDTEXT:
              {
                PTOOLTIPTEXT	pttt = (PTOOLTIPTEXT)mp2;

                pttt->pszText = pPrivate->strTooltipText.psz;
                pttt->ulFormat = TTFMT_PSZ;
              }
              break;

            case TTN_SHOW:
              pPrivate->fTooltipShowing = TRUE;
              break;

            case TTN_POP:
              pPrivate->fTooltipShowing = FALSE;
              break;
          }
          break;
      }

    } // if ( pPrivate != NULL )
  } // if ( pWidget != NULL )

  return brc;
}

/*
 *@@ WgtPaint:
 *      implementation for WM_PAINT.
 *
 *      This really does nothing, except painting a
 *      3D rectangle and printing a question mark.
 */

VOID WgtPaint(HWND hwnd, PXCENTERWIDGET pWidget)
{
  PCPUTEMPPRIVATE	pPrivate = (PCPUTEMPPRIVATE)pWidget->pUser;
  HPS			hps;
  RECTL			rclWin;
  RECTL			rclBar;
  CHAR			acBuf[32];
  ULONG			cbBuf;
  ULONG			ulIdx;
  POINTL		pt1, pt2;
  ULONG			ulBarCY;
  LONG			lcolBar;

  if ( pPrivate == NULL )
    return;

  hps = WinBeginPaint( hwnd, NULLHANDLE, NULL );
  if ( hps == NULLHANDLE )
    return;

  WinQueryWindowRect( hwnd, &rclWin );        // exclusive
  pgpihSwitchToRGB( hps );

  // Erase background.
  WinFillRect( hps, &rclWin, pPrivate->Setup.lcolBackground );

  // Bar's frame.
  rclBar = rclWin;
  rclBar.xRight = rclBar.xLeft + BAR_WIDTH;
  rclBar.yTop--;
  pgpihDraw3DFrame( hps,
                    &rclBar,         // inclusive
                    1,               // width
                    // The XCenter gives us the system 3D colors.
                    pWidget->pGlobals->lcol3DDark,
                    pWidget->pGlobals->lcol3DLight );

  // pPrivate->ulMaxTemp equals zero when temperature is not available.
  if ( pPrivate->ulMaxTemp != 0 )
  {
    ULONG	ulCurTemp = pPrivate->ulCurTemp;
    CHAR	szUM[16];

    // Paint temperature bar.

    pt1.x = rclBar.xLeft + 1;
    pt1.y = rclBar.yBottom + 1;
    pt2.x = rclBar.xRight - 1;
    pt2.y = pt1.y;
    ulBarCY = rclBar.yTop - rclBar.yBottom - 1;

    for( ulIdx = 0;
         ulIdx < ( (ulCurTemp > pPrivate->ulMaxTemp) ?
           ulBarCY : ((ulBarCY * ulCurTemp) / pPrivate->ulMaxTemp) );
         ulIdx++ )
    {
      do
      {
        if ( ulIdx == 0 )
          lcolBar = BARCLR_1;
        else if ( ulIdx == (ulBarCY >> 1) )
          lcolBar = BARCLR_2;
        else if ( ulIdx == ulBarCY - (ulBarCY >> 2) )
          lcolBar = BARCLR_3;
        else
          break;

        GpiSetColor( hps, lcolBar );
      }
      while( FALSE );

      GpiMove( hps, &pt1 );
      GpiLine( hps, &pt2 );
      pt1.y++;
      pt2.y++;
    }

    // Print temperature value.

    if ( pPrivate->Setup.fFahrenheit )
    {
      // Convert to fahrenheit.
      ulCurTemp = _convToFahrenheit( ulCurTemp );
      ulIdx = IDS_FAHRENHEIT_UM;
    }
    else
      ulIdx = IDS_CELSIUS_UM;

    // Load suffix øC / øF.
    WinLoadString( pWidget->pGlobals->hab, hMod, ulIdx, sizeof(szUM), &szUM );
    // Build string.
    cbBuf = sprintf( &acBuf, "%u %s", ulCurTemp, &szUM );
    // Draw string.
    rclWin.xLeft += BAR_WIDTH + 1;
    WinDrawText( hps, cbBuf, &acBuf, &rclWin, pPrivate->Setup.lcolForeground,
                 pPrivate->Setup.lcolBackground, DT_CENTER | DT_VCENTER );
  }

  WinEndPaint(hps);
}

/*
 *@@ WgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 *
 *      While this isn't exactly required, it's a nice
 *      thing for a widget to react to colors and fonts
 *      dropped on it. While we're at it, we also save
 *      these colors and fonts in our setup string data.
 *
 *@@changed V0.9.13 (2001-06-21) [umoeller]: changed XCM_SAVESETUP call for tray support
 */

VOID WgtPresParamChanged(HWND hwnd,
                         ULONG ulAttrChanged,
                         PXCENTERWIDGET pWidget)
{
  PCPUTEMPPRIVATE	pPrivate = (PCPUTEMPPRIVATE)pWidget->pUser;

  if ( pPrivate != NULL )
  {
    BOOL fInvalidate = TRUE;

    switch( ulAttrChanged )
    {
      case 0:     // layout palette thing dropped
      case PP_BACKGROUNDCOLOR:    // background color (no ctrl pressed)
      case PP_FOREGROUNDCOLOR:    // foreground color (ctrl pressed)
        // update our setup data; the presparam has already
        // been changed, so we can just query it
        pPrivate->Setup.lcolBackground
            = pwinhQueryPresColor(hwnd,
                                  PP_BACKGROUNDCOLOR,
                                  FALSE,
                                  SYSCLR_DIALOGBACKGROUND);
        pPrivate->Setup.lcolForeground
            = pwinhQueryPresColor(hwnd,
                                  PP_FOREGROUNDCOLOR,
                                  FALSE,
                                  SYSCLR_WINDOWSTATICTEXT);
        break;

      case PP_FONTNAMESIZE:       // font dropped:
        {
            PSZ pszFont = 0;
            if (pPrivate->Setup.pszFont)
            {
                free(pPrivate->Setup.pszFont);
                pPrivate->Setup.pszFont = NULL;
            }

            pszFont = pwinhQueryWindowFont(hwnd);
            if (pszFont)
            {
                // we must use local malloc() for the font;
                // the winh* code uses a different C runtime
                pPrivate->Setup.pszFont = strdup(pszFont);
                pwinhFree(pszFont);
            }
        }
        break;

      default:
        fInvalidate = FALSE;
    }

    if ( fInvalidate )
    {
      WinInvalidateRect( hwnd, NULL, FALSE );
      WgtSaveSetup( hwnd, &pPrivate->Setup );
    }
  } // if ( pPrivate != NULL )
}

/*
 *@@ WgtDestroy:
 *      implementation for WM_DESTROY.
 *
 *      This must clean up all allocated resources.
 */

VOID WgtDestroy(HWND hwnd, PXCENTERWIDGET pWidget)
{
  PCPUTEMPPRIVATE	pPrivate = (PCPUTEMPPRIVATE)pWidget->pUser;

  if ( pPrivate != NULL )
  {
    WinStopTimer( pWidget->pGlobals->hab, hwnd, pPrivate->ulTimerId );
    WgtClearSetup( &pPrivate->Setup );
    pxstrClear( &pPrivate->strTooltipText );

    free( pPrivate );
  }
}

VOID WgtTimer(HWND hwnd, PXCENTERWIDGET pWidget)
{
  if ( _queryTemp( pWidget ) )
    WinInvalidateRect( pWidget->hwndWidget, NULL, FALSE );
}

VOID WgtCtxMenu(HWND hwnd, PXCENTERWIDGET pWidget)
{
  PCPUTEMPPRIVATE	pPrivate = (PCPUTEMPPRIVATE)pWidget->pUser;
  HWND			hwndMenu = pWidget->hwndContextMenu;
  CHAR			szBuf[32];
  SHORT			sAttrCels = 0, sAttrFahr = 0;

  if ( pWidget->hwndContextMenu == NULLHANDLE )
    return;

  if ( pPrivate->Setup.fFahrenheit )
    sAttrFahr = MIA_CHECKED;
  else
    sAttrCels = MIA_CHECKED;

  if ( !pPrivate->fContextMenuHacked )
  {
    // First call: add items: Celsius, Fahrenheit and separator.

    WinLoadString( pWidget->pGlobals->hab, hMod, IDS_CELSIUS_ITEM,
                   sizeof(szBuf), &szBuf );
    pwinhInsertMenuItem( hwndMenu, 0, MENU_CMD_CELSIUS, &szBuf, MIS_TEXT,
                         sAttrCels );

    WinLoadString( pWidget->pGlobals->hab, hMod, IDS_FAHRENHEIT_ITEM,
                   sizeof(szBuf), &szBuf );
    pwinhInsertMenuItem( hwndMenu, 1, MENU_CMD_FAHRENHEIT, &szBuf, MIS_TEXT,
                         sAttrFahr );

    pwinhInsertMenuItem( hwndMenu, 2, 0, NULL, MIS_SEPARATOR, 0 );

    pPrivate->fContextMenuHacked = TRUE;
    return;
  }

  // Check on item which corresponds to the current settings:
  // Celsius or Fahrenheit.
  WinSendMsg( hwndMenu, MM_SETITEMATTR,
              MPFROM2SHORT( MENU_CMD_CELSIUS, TRUE ),
              MPFROM2SHORT( MIA_CHECKED, sAttrCels ) );
  WinSendMsg( hwndMenu, MM_SETITEMATTR,
              MPFROM2SHORT( MENU_CMD_FAHRENHEIT, TRUE ),
              MPFROM2SHORT( MIA_CHECKED, sAttrFahr ) );
}

BOOL WgtCommand(HWND hwnd, PXCENTERWIDGET pWidget, USHORT usCmd)
{
  PCPUTEMPPRIVATE	pPrivate = (PCPUTEMPPRIVATE)pWidget->pUser;

  if ( usCmd != MENU_CMD_CELSIUS && usCmd != MENU_CMD_FAHRENHEIT )
    return FALSE;

  // Switch Celsius / Fahrenheit.
  pPrivate->Setup.fFahrenheit = usCmd == MENU_CMD_FAHRENHEIT;
  _queryTemp( pWidget );
  WinInvalidateRect( pWidget->hwndWidget, NULL, FALSE );
  // Save changes in settings.
  WgtSaveSetup( hwnd, &pPrivate->Setup );

  return TRUE;
}

/*
 *@@ fnwpWidget:
 *      window procedure for the widget class.
 *
 *      There are a few rules which widget window procs
 *      must follow. See XCENTERWIDGETCLASS in center.h
 *      for details.
 *
 *      Other than that, this is a regular window procedure
 *      which follows the basic rules for a PM window class.
 */

MRESULT EXPENTRY fnwpWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  // get widget data from QWL_USER (stored there by WM_CREATE)
  PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr( hwnd, QWL_USER );
                  // this ptr is valid after WM_CREATE

  switch( msg )
  {
    /*
     * WM_CREATE:
     *      as with all widgets, we receive a pointer to the
     *      XCENTERWIDGET in mp1, which was created for us.
     *
     *      The first thing the widget MUST do on WM_CREATE
     *      is to store the XCENTERWIDGET pointer (from mp1)
     *      in the QWL_USER window word by calling:
     *
     *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
     *
     *      We use XCENTERWIDGET.pUser for allocating
     *      CPUTEMPPRIVATE for our own stuff.
     */

    case WM_CREATE:
      WinSetWindowPtr( hwnd, QWL_USER, mp1 );
      pWidget = (PXCENTERWIDGET)mp1;

      return ( ( pWidget != NULL ) && ( pWidget->pfnwpDefWidgetProc != NULL ) )
        ? WgtCreate( hwnd, pWidget )
        : (MRESULT)TRUE;                 // Stop window creation!!

    /*
     * WM_CONTROL:
     *      process notifications/queries from the XCenter.
     */

    case WM_CONTROL:
      return (MRESULT)WgtControl( hwnd, mp1, mp2 );

    /*
     * WM_PAINT:
     *
     */

    case WM_PAINT:
      WgtPaint(hwnd, pWidget);
      return 0;

    /*
     * WM_PRESPARAMCHANGED:
     *
     */

    case WM_PRESPARAMCHANGED:
      if ( pWidget != NULL )
        // this gets sent before this is set!
        WgtPresParamChanged( hwnd, (ULONG)mp1, pWidget );
      return 0;

    /*
     * WM_DESTROY:
     *      clean up. This _must_ be passed on to
     *      ctrDefWidgetProc.
     */

    case WM_DESTROY:
      WgtDestroy( hwnd, pWidget );
      // we _MUST_ pass this on, or the default widget proc cannot clean up.
      break;

    /*
     * WM_TIMER:
     *      It's time to update the data.
     */

    case WM_TIMER:
      if ( ((PCPUTEMPPRIVATE)pWidget->pUser)->ulTimerId != SHORT1FROMMP(mp1) )
        break;

      WgtTimer( hwnd, pWidget );
      return 0;

    case WM_CONTEXTMENU:
      WgtCtxMenu( hwnd, pWidget );
      break;

    case WM_COMMAND:
      if ( WgtCommand( hwnd, pWidget, SHORT1FROMMP( mp1 ) ) )
        return 0;
      break;
  } // end switch(msg)

  return pWidget->pfnwpDefWidgetProc( hwnd, msg, mp1, mp2 );
}

/* ******************************************************************
 *
 *   Exported procedures
 *
 ********************************************************************/

/*
 *@@ WgtInitModule:
 *      required export with ordinal 1, which must tell
 *      the XCenter how many widgets this DLL provides,
 *      and give the XCenter an array of XCENTERWIDGETCLASS
 *      structures describing the widgets.
 *
 *      With this call, you are given the module handle of
 *      XFLDR.DLL. For convenience, you may resolve imports
 *      for some useful functions which are exported thru
 *      src\shared\xwp.def. See the code below.
 *
 *      This function must also register the PM window classes
 *      which are specified in the XCENTERWIDGETCLASS array
 *      entries. For this, you are given a HAB which you
 *      should pass to WinRegisterClass. For the window
 *      class style (4th param to WinRegisterClass),
 *      you should specify
 *
 +          CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT
 *
 *      Your widget window _will_ be resized, even if you're
 *      not planning it to be.
 *
 *      This function only gets called _once_ when the widget
 *      DLL has been successfully loaded by the XCenter. If
 *      there are several instances of a widget running (in
 *      the same or in several XCenters), this function does
 *      not get called again. However, since the XCenter unloads
 *      the widget DLLs again if they are no longer referenced
 *      by any XCenter, this might get called again when the
 *      DLL is re-loaded.
 *
 *      There will ever be only one load occurence of the DLL.
 *      The XCenter manages sharing the DLL between several
 *      XCenters. As a result, it doesn't matter if the DLL
 *      has INITINSTANCE etc. set or not.
 *
 *      If this returns 0, this is considered an error, and the
 *      DLL will be unloaded again immediately.
 *
 *      If this returns any value > 0, *ppaClasses must be
 *      set to a static array (best placed in the DLL's
 *      global data) of XCENTERWIDGETCLASS structures,
 *      which must have as many entries as the return value.
 */

ULONG EXPENTRY WgtInitModule(HAB hab,         // XCenter's anchor block
                             HMODULE hmodPlugin, // module handle of the widget DLL
                             HMODULE hmodXFLDR,    // XFLDR.DLL module handle
                             PCXCENTERWIDGETCLASS *ppaClasses,
                             PSZ pszErrorMsg)  // if 0 is returned, 500 bytes of error msg
{
  ULONG   ulrc = 0,
          ul = 0;
  BOOL    fImportsFailed = FALSE;

  // resolve imports from XFLDR.DLL (this is basically
  // a copy of the doshResolveImports code, but we can't
  // use that before resolving...)
  for( ul = 0; ul < sizeof(G_aImports) / sizeof(G_aImports[0]); ul++ )
  {
    if ( DosQueryProcAddr( hmodXFLDR, 0, (PSZ)G_aImports[ul].pcszFunctionName,
                            G_aImports[ul].ppFuncAddress ) != NO_ERROR )
    {
      strcpy( pszErrorMsg, "Import " );
      strcat( pszErrorMsg, G_aImports[ul].pcszFunctionName );
      strcat( pszErrorMsg, " failed." );
      fImportsFailed = TRUE;
      break;
    }
  }

  if ( !fImportsFailed )
  {
    // all imports OK: register our PM window class
    if ( !WinRegisterClass( hab,
                            WNDCLASS_WIDGET_CPUTEMP,
                            fnwpWidget,
                            CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT,
                            sizeof(PCPUTEMPPRIVATE) )
                                 // extra memory to reserve for QWL_USER
                          )
      strcpy( pszErrorMsg, "WinRegisterClass failed." );
    else
    {
      // no error:
      // return widget classes
      *ppaClasses = G_WidgetClasses;

      // return no. of classes in this DLL (one here):
      ulrc = sizeof(G_WidgetClasses) / sizeof(G_WidgetClasses[0]);

      // Store module handle.
      hMod = hmodPlugin;
    }
  }

  return ulrc;
}

/*
 *@@ WgtUnInitModule:
 *      optional export with ordinal 2, which can clean
 *      up global widget class data.
 *
 *      This gets called by the XCenter right before
 *      a widget DLL gets unloaded. Note that this
 *      gets called even if the "init module" export
 *      returned 0 (meaning an error) and the DLL
 *      gets unloaded right away.
 */

VOID EXPENTRY WgtUnInitModule(VOID)
{
}

/*
 *@@ WgtQueryVersion:
 *      this new export with ordinal 3 can return the
 *      XWorkplace version number which is required
 *      for this widget to run. For example, if this
 *      returns 0.9.10, this widget will not run on
 *      earlier XWorkplace versions.
 *
 *      NOTE: This export was mainly added because the
 *      prototype for the "Init" export was changed
 *      with V0.9.9. If this returns 0.9.9, it is
 *      assumed that the INIT export understands
 *      the new FNWGTINITMODULE_099 format (see center.h).
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

VOID EXPENTRY WgtQueryVersion(PULONG pulMajor, PULONG pulMinor,
                              PULONG pulRevision)
{
  // report 0.9.9
  *pulMajor = 0;
  *pulMinor = 9;
  *pulRevision = 9;
}

