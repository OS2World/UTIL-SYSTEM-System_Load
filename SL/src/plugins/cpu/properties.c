#define INCL_DOSMISC
#define INCL_DOSERRORS
#include <ds.h>
#include <sl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cputemp.h"
#include "cpu.h"
#include "termzone.h"
#include <debug.h>     // Must be the last.

#define COLOR_VPAD	8

extern PFNCPUTEMPQUERY pfnCPUTempQuery; // cpu.c
extern GRPARAM         stGrParam;       // cpu.c
extern GRAPH           stGraph;         // cpu.c
extern HMODULE         hDSModule;       // Module handle, cpu.c
extern HAB             hab;
extern ULONG           cCPU;            // CPU count, cpu.c
extern PGRVALPARAM     pGrValParam;     // All graphs (CPU and IRQ) paramethrs.
extern ULONG           ulTimeWindow;    // Graph's time window, cpu.c
extern LONG            aulDefColors[];
extern PSZ             pszTZPathname;   // ACPI pathname for temperature, cpu.c
extern ULONG           ulTemperature;   // Current t*10, cpu.c
extern ULONG           ulPTMU;          // TZ_MU_CELSIUS/TZ_MU_FAHRENHEIT, cpu.c
extern BOOL            fRealFreq;       // Show real frequency, cpu.c
extern BOOL            fThickLines;     // Thick graph lines, cpu.c
extern ULONG           ulSeparateRates; // Show separate rates user/IRQ loads

// Helpers, cpu.c
extern PHiniWriteStr			piniWriteStr;
extern PHiniWriteULong			piniWriteULong;
extern PHgrSetTimeScale			pgrSetTimeScale;
extern PHutilGetTextSize		putilGetTextSize;
extern PHutilLoadInsertStr		putilLoadInsertStr;
extern PHutilQueryProgPath		putilQueryProgPath;
extern PHupdateLock			pupdateLock;
extern PHupdateUnlock			pupdateUnlock;
extern PHstrLoad			pstrLoad;
extern PHctrlStaticToColorRect		pctrlStaticToColorRect;
extern PHctrlDlgCenterOwner		pctrlDlgCenterOwner;
extern PHctrlQueryText			pctrlQueryText;
extern PHctrlSetDefaultFont		pctrlSetDefaultFont;
extern PHhelpShow			phelpShow;

static LONG  clrSaveBackground;
static LONG  clrSaveBorder;
static LONG  aclrSaveCPU[65];
static BOOL  fSaveThickLines;


//	Page 1
//	------

static VOID _p1undo(HWND hwnd)
{
  ULONG      ulIdx;

  // Temperature

  WinSetWindowText( WinWindowFromID( hwnd, IDD_CB_PATHNAME ), pszTZPathname );
  WinSendDlgItemMsg( hwnd,
                     ulPTMU == TZ_MU_CELSIUS ? IDD_RB_TEMP_C : IDD_RB_TEMP_F,
                     BM_SETCHECK, MPFROMSHORT( 1 ), 0 );

  // Show real frequency checkbox
  WinCheckButton( hwnd, IDD_CB_REALFREQ, fRealFreq );
  // Thick lines checkbox
  WinCheckButton( hwnd, IDD_CB_THICKLINES, fSaveThickLines );
  fThickLines = fSaveThickLines;

  // Colors
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrSaveBackground );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrSaveBorder );
  for( ulIdx = 0; ulIdx <= cCPU; ulIdx++ )
    WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_IRQCOL - cCPU + ulIdx ),
                     PP_BACKGROUNDCOLOR, sizeof(LONG), &aclrSaveCPU[ulIdx] );

  // Graph time slider. Position of the slider arm
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ),
                     MPFROMSHORT( (ulTimeWindow - 60) / 5 ) );
}

static VOID _p1default(HWND hwnd)
{
  LONG       lColor;
  ULONG      ulIdx;
  CHAR       szBuf[128];

  // Temperature.
  // Try one by one pathnames from resource and set first line for which
  // temperature is found.

  WinSetWindowText( WinWindowFromID( hwnd, IDD_CB_PATHNAME ), NULL );
  for( ulIdx = IDS_PATHNAME_FIRST_ID;
       pstrLoad( hDSModule, ulIdx, sizeof(szBuf), &szBuf ) != 0 &&
       szBuf[0] == '\\'; ulIdx++ ) 
  {
    if ( tzQuery( szBuf, ulPTMU, &ulTemperature ) != TZ_OK )
      continue;

    WinSetWindowText( WinWindowFromID( hwnd, IDD_CB_PATHNAME ), szBuf );
    break;
  }

  WinSendDlgItemMsg( hwnd, IDD_RB_TEMP_C, BM_SETCHECK, MPFROMSHORT(1), 0 );

  // Show real frequenct checkbox.
  WinCheckButton( hwnd, IDD_CB_REALFREQ, DEF_REALFREQ );
  // Thick lines checkbox checkbox.
  WinCheckButton( hwnd, IDD_CB_THICKLINES, DEF_THICKLINES );
  fThickLines = DEF_THICKLINES;

  // Colors

  lColor = DEF_GRBGCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );
  lColor = DEF_GRBORDERCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );
  lColor = DEF_IRQ_COLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_IRQCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );

  for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
    WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_IRQCOL - cCPU + ulIdx ),
                     PP_BACKGROUNDCOLOR, sizeof(LONG),
                     &aulDefColors[ulIdx % DEF_COLORS] );

  // Graph time slider. Position of the slider arm
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ),
                     MPFROMSHORT( (DEF_TIMEWINDOW - 60) / 5 ) );
}

static BOOL _successNewPathnameMessage(HWND hwnd, ULONG ulT, BOOL fCelsius)
{
  // Temperature is found out successfully.

  CHAR       szPathname[128];
  PCHAR      pcPathname = szPathname;
  CHAR       szItem[128];
  CHAR       szTemp[16];
  ldiv_t     stT;
  LONG       lColor = SYSCLR_DIALOGBACKGROUND;
  ULONG      ulIdx, cItems;
  HWND       hwndDlg;
  HWND       hwndPathname = WinWindowFromID( hwnd, IDD_CB_PATHNAME );

  pctrlQueryText( hwndPathname, sizeof(szPathname), &szPathname );
  while( *pcPathname == '\\' && pcPathname[1] == '\\' )
    pcPathname++;

  // Is it a new (not one from the list) string?

  cItems = (USHORT)WinSendMsg( hwndPathname, LM_QUERYITEMCOUNT, 0, 0 );
  for( ulIdx = 0; ulIdx < cItems; ulIdx++ )
  {
    WinSendMsg( hwndPathname, LM_QUERYITEMTEXT,
                MPFROM2SHORT( ulIdx, sizeof(szItem) ), MPFROMP( &szItem ) );
    if ( stricmp( pcPathname, &szItem ) == 0 )
      return FALSE; // pathname is not new
  }

  // Show dialog with e-mail.

  stT = ldiv( ulT, 10 );
  _snprintf( szTemp, sizeof(szTemp), "%u.%u ø%c",
             stT.quot, stT.rem, fCelsius ? 'C':'F' );

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwnd, WinDefDlgProc, hDSModule,
                        IDD_SUCCESSTEMP, NULL );

  WinSetPresParam( WinWindowFromID( hwndDlg, IDD_EF_TEMP ),
                   PP_BACKGROUNDCOLORINDEX, sizeof(LONG), &lColor );
  WinSetPresParam( WinWindowFromID( hwndDlg, IDD_EF_EMAIL ),
                   PP_BACKGROUNDCOLORINDEX, sizeof(LONG), &lColor );
  WinSetPresParam( WinWindowFromID( hwndDlg, IDD_EF_PATHNAME ),
                   PP_BACKGROUNDCOLORINDEX, sizeof(LONG), &lColor );

  WinSetWindowText( WinWindowFromID( hwndDlg, IDD_EF_TEMP ), szTemp );
  WinSetWindowText( WinWindowFromID( hwndDlg, IDD_EF_PATHNAME ), szPathname );

  pctrlDlgCenterOwner( hwndDlg );
  WinProcessDlg( hwndDlg );
  WinDestroyWindow( hwndDlg );
  return TRUE;
}

static VOID _p1wmInit(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  MRESULT	mr;
  CHAR		szBuf[128];
  ULONG		cbBuf;
  ULONG		ulIdx;
  ULONG		ulCPU;
  HWND		hwndIRQColor = WinWindowFromID( hwnd, IDD_ST_IRQCOL );
  HWND		hwndColor;
  SWP		swpColor;
  HWND		hwndGB = WinWindowFromID( hwnd, IDD_GB_COLORS );
  SIZEL		sizeGBTitle;
  SWP		swpGB;
  HWND		hwndPathname = WinWindowFromID( hwnd, IDD_CB_PATHNAME );
  HPS		hps = WinGetPS( hwnd );

  // Need driver message and button "Install".

  if ( ( pfnCPUTempQuery == NULL ) ||
       ( pfnCPUTempQuery( 0, NULL, &ulIdx ) == CPUTEMP_NO_DRIVER ) )
  {
    WinShowWindow( WinWindowFromID( hwnd, IDD_ST_NEED_DRIVER ), TRUE );
  }

  // Save current colors for undo.

  clrSaveBackground = stGrParam.clrBackground;
  clrSaveBorder = stGrParam.clrBorder;
  for( ulIdx = 0; ulIdx <= cCPU; ulIdx++ )
    aclrSaveCPU[ulIdx] = pGrValParam[ulIdx].clrGraph;

  fSaveThickLines = fThickLines;

  // Default pathnames from resource.

  for( ulIdx = IDS_PATHNAME_FIRST_ID;
       pstrLoad( hDSModule, ulIdx, sizeof(szBuf), szBuf ) != 0 &&
       szBuf[0] == '\\'; ulIdx++ ) 
  {
    WinSendMsg( hwndPathname, LM_INSERTITEM, MPFROMSHORT(LIT_END),
                MPFROMP(szBuf) );
  }

  // Graph time slider

  // Position of the shaft to within the slider window (left +10)
  mr = WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_QUERYSLIDERINFO,
                          MPFROM2SHORT( SMA_SHAFTPOSITION, SMA_RANGEVALUE ), 0 );
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SHAFTPOSITION, SMA_RANGEVALUE ),
                     MPFROM2SHORT( 10, SHORT2FROMMR( mr ) ) );
  // Shaft Height 
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SHAFTDIMENSIONS, 10 ), MPFROMLONG( 16 ) );
  // Arm width and height
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMDIMENSIONS, 10 ),
                     MPFROM2SHORT( 10, 20 ) );
  // Size of a tick mark
  for( ulIdx = 0; ulIdx <= 48; ulIdx += 6 )
  {
    WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETTICKSIZE,
                       MPFROM2SHORT( ulIdx, ulIdx % 12 == 0 ? 8 : 5 ),
                       0 );
  }
  // Text above a tick mark
  pstrLoad( 0, IDS_MIN_SHORT, sizeof(szBuf) - 2, &szBuf[2] ); // "min."
  *((PUSHORT)&szBuf) = ' 1';
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSCALETEXT,
                     MPFROMSHORT( 0 ), MPFROMP( &szBuf ) );
  *((PUSHORT)&szBuf) = ' 5';
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSCALETEXT,
                     MPFROMSHORT( 48 ), MPFROMP( &szBuf ) );

  // Make color select rectangles.

  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BGCOL ),
                                     clrSaveBackground );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ),
                                     clrSaveBorder );
  pctrlStaticToColorRect( hwndIRQColor, aclrSaveCPU[cCPU] );

  // Add color select rectangles for each CPU above IRQ color rectangle.
  // Expand height of group box "Graph colors" to fit all color rectangles.

  // Query position and title's height of group box.
  WinQueryWindowPos( hwndGB, &swpGB );
  putilGetTextSize( hps, WinQueryWindowText( hwndGB, sizeof(szBuf), &szBuf ),
                    &szBuf, &sizeGBTitle );
  WinReleasePS( hps );

  // Query position of "IRQ" color rectangle (it already exists).
  WinQueryWindowPos( hwndIRQColor, &swpColor );
  cbBuf = pstrLoad( hDSModule, IDS_CPU, sizeof(szBuf) - 3, &szBuf );
  szBuf[cbBuf++] = ' ';

  // Create new windows (static text converted to color select rectangle) for
  // all processors.
  for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
  {
    ulCPU = cCPU - ulIdx - 1;
    ultoa( ulCPU, &szBuf[cbBuf], 10 ); 
    swpColor.y += swpColor.cy + COLOR_VPAD;

    // CPU window ID = IRQ window ID - 1 - ulIdx :
    // CPU0 color rectangle id = IDD_ST_IRQCOL - count of CPUs
    // CPU1 color rectangle id = IDD_ST_IRQCOL - count of CPUs + 1
    // Last CPU color rectangle id = IDD_ST_IRQCOL - 1
    // In other words, the IDs for CPUs and IRQ will have sequential values: 
    // from (IDD_ST_IRQCOL - N) for CPU0 to IDD_ST_IRQCOL for IRQ.
    hwndColor = WinCreateWindow( hwnd, WC_STATIC, &szBuf,
                        SS_TEXT | DT_LEFT | DT_TOP | WS_VISIBLE | WS_TABSTOP,
                        swpColor.x, swpColor.y, swpColor.cx, swpColor.cy, hwnd,
                        HWND_TOP, IDD_ST_IRQCOL - 1 - ulIdx, NULL, NULL );
    pctrlStaticToColorRect( hwndColor, aclrSaveCPU[ulCPU] );
    WinSetWindowPos( hwndColor, HWND_TOP, 0, 0, 0, 0, SWP_SHOW );
  }

  // Increasing the height of group box if necessary.
  if ( (swpColor.y + swpColor.cy + COLOR_VPAD) >=
       (swpGB.y + swpGB.cy - sizeGBTitle.cy) )
  {
    swpGB.cy = ( swpColor.y + swpColor.cy + COLOR_VPAD + sizeGBTitle.cy ) -
               swpGB.y;
    WinSetWindowPos( hwndGB, HWND_TOP, 0, 0, swpGB.cx, swpGB.cy, SWP_SIZE );
  }

  debugStat();
}

static VOID _p1wmDestroy(HWND hwnd)
{
  CHAR       szBuf[128];
  ULONG      cbBuf, ulIdx;
  PCHAR      pcBuf = szBuf;

  // The colors already applied in function DialogProc() / WM_CONTROL

  // Apply "show real frequency"
  fRealFreq = WinQueryButtonCheckstate( hwnd, IDD_CB_REALFREQ ) != 0;

  // Apply temperature

  if ( pszTZPathname != NULL )
    free( pszTZPathname );
  ulTemperature = (ULONG)(-1);

  cbBuf = pctrlQueryText( WinWindowFromID(hwnd, IDD_CB_PATHNAME),
                                         sizeof(szBuf), &szBuf );
  while( *pcBuf == '\\' && pcBuf[1] == '\\' )
  {
    pcBuf++;
    cbBuf--;
  }

  if ( cbBuf == 0 )
    pszTZPathname = NULL;
  else
  {
    pszTZPathname = malloc( cbBuf + 1 );
    if ( pszTZPathname != NULL )
    {
      memcpy( pszTZPathname, pcBuf, cbBuf );
      pszTZPathname[cbBuf] = '\0';
      // Do not leave ulTemperature == (ULONG)(-1) for updates.
      tzQuery( pszTZPathname, ulPTMU, &ulTemperature );
    }
  }

  ulPTMU = WinSendDlgItemMsg( hwnd, IDD_RB_TEMP_C, BM_QUERYCHECK, 0, 0 ) != 0 ?
             TZ_MU_CELSIUS : TZ_MU_FAHRENHEIT;

  // Apply graph time window.

  ulTimeWindow = 60 + 5 *
    LONGFROMMR( WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_QUERYSLIDERINFO,
                                   MPFROM2SHORT( SMA_SLIDERARMPOSITION,
                                                 SMA_INCREMENTVALUE ), 0 ) );
  pupdateLock();
  pgrSetTimeScale( &stGraph, ulTimeWindow, ulTimeWindow * 1000 );
  pupdateUnlock();

  // Store properties to the ini-file

  piniWriteULong( hDSModule, "realFreq", (ULONG)fRealFreq );
  piniWriteULong( hDSModule, "thickLines", (ULONG)fThickLines );

  piniWriteStr( hDSModule, "TZPathname", pszTZPathname );
  piniWriteULong( hDSModule, "TMU", ulPTMU );
  piniWriteULong( hDSModule, "GrBgColor", stGrParam.clrBackground );
  piniWriteULong( hDSModule, "GrBorderColor", stGrParam.clrBorder );
  piniWriteULong( hDSModule, "TimeWindow", ulTimeWindow );

  for( ulIdx = 0; ulIdx <= cCPU; ulIdx++ )
  {
    if ( ulIdx < cCPU )
      cbBuf = _snprintf( szBuf, sizeof(szBuf), "CPU%uColor", ulIdx );
    else
      cbBuf = _snprintf( szBuf, sizeof(szBuf), "IRQColor" );

    piniWriteULong( hDSModule, szBuf, pGrValParam[ulIdx].clrGraph );
  }
}


static MRESULT EXPENTRY p1DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  CHAR		szBuf[64];

  switch( msg )
  {
    case WM_INITDLG:
      _p1wmInit( hwnd, mp1, mp2 );
      _p1undo( hwnd );
      return (MRESULT)FALSE;

    case WM_DESTROY:
      _p1wmDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_UNDO:
          _p1undo( hwnd );
          break;

        case IDD_PB_DEFAULT:
          _p1default( hwnd );
          break;

        case IDD_PB_HELP:
          // Button "Help" pressed - show help panel (see cpu.ipf).
          phelpShow( hDSModule, 20802 );
          break;

        case IDD_PB_TEST:
          {
            // Button "Test" to query CPU temperature pressed.
            // Trying to query CPU temperature.

            ULONG      ulT;
            BOOL       fCelsius;
            ULONG      ulStrId;
            CHAR       szTitle[128];
            CHAR       szMsg[256];
            ULONG      ulMBFlags = MB_MOVEABLE | MB_ICONHAND | MB_CANCEL;

            // Get path for ACPI table.
            pctrlQueryText( WinWindowFromID( hwnd, IDD_CB_PATHNAME ),
                            sizeof(szMsg), szMsg );

            // Get measurement units.
            fCelsius = WinSendDlgItemMsg( hwnd, IDD_RB_TEMP_C, BM_QUERYCHECK,
                                          0, 0 ) != 0;

            // Try to query temperature of CPU.
            // Select a message from the resource corresponding to
            // the error code.
            switch( tzQuery( szMsg,
                             fCelsius ? TZ_MU_CELSIUS : TZ_MU_FAHRENHEIT,
                             &ulT ) )
            {
              case TZ_OK:
                // Temperature is found out successfully.

                // Show dialog with e-mail for new custom pathname.
                if ( _successNewPathnameMessage( hwnd, ulT, fCelsius ) )
                  return (MRESULT)FALSE;

                // Show a standard message for predefined pathname.
                ulStrId = IDMSG_TZ_OK;
                ulMBFlags = MB_MOVEABLE | MB_INFORMATION | MB_OK;
                break;

              case TZ_ACPI_INCOMP_VER:
                ulStrId = IDMSG_TZ_ACPI_INCOMP_VER;
                break;

              case TZ_INVALID_TYPE:
                ulStrId = IDMSG_TZ_INVALID_TYPE;
                break;

              case TZ_INVALID_VALUE:
                ulStrId = IDMSG_TZ_INVALID_VALUE;
                break;

              case TZ_BAD_PATHNAME:
                ulStrId = IDMSG_TZ_BAD_PATHNAME;
                break;

              default: // TZ_ACPI_EVALUATE_FAIL, TZ_NOT_FOUND
                ulStrId = IDMSG_TZ_NOT_FOUND;
                break;
            }

            if ( WinLoadMessage( NULLHANDLE, hDSModule, ulStrId, sizeof(szMsg),
                                 szMsg ) == 0 )
            {
              debug( "Cannot load message #%u", ulStrId );
              return (MRESULT)FALSE;
            }

            // Groupbox caption will be used for title of message box
            WinQueryWindowText( WinWindowFromID( hwnd, IDD_GB_TEMPERATURE ),
                                sizeof(szTitle), szTitle );

            WinMessageBox( HWND_DESKTOP, hwnd, szMsg, szTitle, 1, ulMBFlags );
          }
          break;
      }
      return (MRESULT)FALSE;

    case WM_CONTROL:
      switch( SHORT1FROMMP(mp1) )
      {
        case IDD_CB_THICKLINES:
          if ( SHORT2FROMMP(mp1) == BN_CLICKED  )
            fThickLines = WinQueryButtonCheckstate( hwnd, IDD_CB_THICKLINES )
                            != 0;
          break;

        case IDD_ST_BGCOL:
          if ( SHORT2FROMMP(mp1) == EN_CHANGE )
            WinQueryPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &stGrParam.clrBackground, QPF_NOINHERIT );
          break;

        case IDD_ST_BORDERCOL:
          if ( SHORT2FROMMP(mp1) == EN_CHANGE )
            WinQueryPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &stGrParam.clrBorder, QPF_NOINHERIT );
          break;

        case IDD_SLID_TIME:
          if ( SHORT2FROMMP(mp1) == SLN_SLIDERTRACK ||
               SHORT2FROMMP(mp1) == SLN_CHANGE )
          {
            // Time slider position changed.

            ULONG      ulLen;
            ULONG      ulSec =
              LONGFROMMR( WinSendDlgItemMsg( hwnd, IDD_SLID_TIME,
                       SLM_QUERYSLIDERINFO,
                       MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE),
                       0 ) ) * 5;
            ldiv_t	stMinSec = ldiv( ulSec, 60 );

            ulLen = sprintf( szBuf, "%u ", 1 + stMinSec.quot );
            ulLen += pstrLoad( 0, IDS_MIN_SHORT, sizeof(szBuf) - ulLen,
                               &szBuf[ulLen] );      // "min."
            if ( stMinSec.rem != 0 )
            {
              ulLen += sprintf( &szBuf[ulLen], " %u ", stMinSec.rem );
              pstrLoad( 0, IDS_SEC_SHORT, sizeof(szBuf) - ulLen,
                        &szBuf[ulLen] );             // "sec."
            }

            WinSetDlgItemText( hwnd, IDD_ST_GRAPHTIME, szBuf );
          }
          break;

        default:
          if ( ( SHORT1FROMMP(mp1) >= IDD_ST_IRQCOL - cCPU ) &&
               ( SHORT1FROMMP(mp1) <= IDD_ST_IRQCOL ) &&
               ( SHORT2FROMMP(mp1) == EN_CHANGE ) )
          {
            // One of CPU's color rectangle changed.
            ULONG	ulIdx = SHORT1FROMMP(mp1) - ( IDD_ST_IRQCOL - cCPU );

            WinQueryPresParam( WinWindowFromID( hwnd, SHORT1FROMMP( mp1 ) ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &pGrValParam[ulIdx].clrGraph, QPF_NOINHERIT );
          }
      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


// Data source's interface routine dsLoadDlg()
// -------------------------------------------

DSEXPORT HWND APIENTRY dsLoadDlg(HWND hwndOwner, ULONG ulPage,
                                 ULONG cbTitle, PCHAR acTitle)
{
  return WinLoadDlg( hwndOwner, hwndOwner, p1DialogProc,
                     hDSModule, IDD_DSPROPERTIES1, NULL );
}
