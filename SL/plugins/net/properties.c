#include <ds.h>
#include <sl.h>
#include <stdio.h>
#include "net.h"
#include "ctrl.h"
#include <debug.h>

extern GRPARAM		stGrParam;	// net.c
extern HMODULE		hDSModule;	// Module handle, net.c
extern PROPERTIES	stProperties;	// net.c
extern GRAPH		stGraph;	// net.c

static LONG		clrBackground;
static LONG		clrBorder;
static LONG		clrRX;
static LONG		clrTX;

extern PHiniWriteULong		piniWriteULong;
extern PHgrSetTimeScale		pgrSetTimeScale;
extern PHupdateLock		pupdateLock;
extern PHupdateUnlock		pupdateUnlock;
extern PHstrLoad		pstrLoad;
extern PHctrlStaticToColorRect	pctrlStaticToColorRect;


static VOID _undo(HWND hwnd)
{
  // Colors
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrBackground );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrBorder );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_RXCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrRX );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_TXCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrTX );

  // Graph time slider. Position of the slider arm
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ),
                     MPFROMSHORT( (stProperties.ulTimeWindow - 60) / 5 ) );

  // Update interval
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETCURRENTVALUE,
                     MPFROMLONG( stProperties.ulUpdateInterval / 100 - 5 ), 0 );

}

static VOID _default(HWND hwnd)
{
  LONG		lColor;

  // Colors

  lColor = DEF_GRBGCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );
  lColor = DEF_GRBORDERCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );
  lColor = DEF_RXCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_RXCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );
  lColor = DEF_TXCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_TXCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );

  // Graph time slider. Position of the slider arm
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ),
                     MPFROMSHORT( (DEF_TIMEWIN - 60) / 5 ) );

  // Update interval
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETCURRENTVALUE,
                     MPFROMLONG( DEF_INTERVAL / 100 - 5 ), 0 );
}

static VOID _wmInit(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
  PSZ		apszIntervals[15];
  CHAR		szBuf[128];
  PSZ		pszPtr = &szBuf;
  ULONG		ulIdx;
  MRESULT	mr;

  // Save current colors for undo.
  clrBackground = stGrParam.clrBackground;
  clrBorder = stGrParam.clrBorder;
  clrRX = stGrParam.pParamVal[0].clrGraph;
  clrTX = stGrParam.pParamVal[1].clrGraph;

  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BGCOL ),
                                     clrBackground );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ),
                                     clrBorder );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_RXCOL ),
                                     clrRX );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_TXCOL ),
                                     clrTX );

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

  // Update interval

  for( ulIdx = 5; ulIdx < 21; ulIdx++ )
  {
    apszIntervals[ulIdx - 5] = pszPtr;
    pszPtr += 1 + sprintf( pszPtr, "%u.%u", ulIdx / 10, ulIdx % 10 );
  }
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETARRAY,
                     MPFROMP( &apszIntervals ), MPFROMLONG( 16 ) );

  debugStat();
}

static VOID _wmDestroy(HWND hwnd)
{
  CHAR		szBuf[16];
  ULONG		ulIdx;

  // The colors already applied in function DialogProc() / WM_CONTROL.

  // Apply update period

  pupdateLock();

  if ( WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_QUERYVALUE,
         MPFROMP( &szBuf ),
         MPFROM2SHORT( sizeof(szBuf), SPBQ_DONOTUPDATE  ) ) )
    stProperties.ulUpdateInterval = ( (szBuf[0] - '0') * 1000 ) +
                                    ( (szBuf[2] - '0') * 100 );

  // Apply graph time window

  stProperties.ulTimeWindow = 60 + 5 *
    LONGFROMMR( WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_QUERYSLIDERINFO,
                                   MPFROM2SHORT( SMA_SLIDERARMPOSITION,
                                                 SMA_INCREMENTVALUE ), 0 ) );

  ulIdx = stProperties.ulTimeWindow * 1000;
  pgrSetTimeScale( &stGraph,
                ulIdx / stProperties.ulUpdateInterval, ulIdx );

  pupdateUnlock();

  // Store properties to the ini-file

  piniWriteULong( hDSModule, "GrBgColor", stGrParam.clrBackground );
  piniWriteULong( hDSModule, "GrBorderColor", stGrParam.clrBorder );
  piniWriteULong( hDSModule, "RXColor",
                             stGrParam.pParamVal[0].clrGraph );
  piniWriteULong( hDSModule, "TXColor",
                             stGrParam.pParamVal[1].clrGraph );

  piniWriteULong( hDSModule, "UpdateInterval",
                             stProperties.ulUpdateInterval );
  piniWriteULong( hDSModule, "TimeWindow",
                             stProperties.ulTimeWindow );
}


static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  CHAR		szBuf[64];

  switch( msg )
  {
    case WM_INITDLG:
      _wmInit( hwnd, mp1, mp2 );
      // go to WM_SL_UNDO
    case WM_SL_UNDO:
      _undo( hwnd );
      break;

    case WM_SL_DEFAULT:
      _default( hwnd );
      break;

    case WM_DESTROY:
      _wmDestroy( hwnd );
      break;

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_ST_BGCOL:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            WinQueryPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &stGrParam.clrBackground, QPF_NOINHERIT );
          break;

        case IDD_ST_BORDERCOL:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            WinQueryPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &stGrParam.clrBorder, QPF_NOINHERIT );
          break;

        case IDD_ST_RXCOL:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            WinQueryPresParam( WinWindowFromID( hwnd, IDD_ST_RXCOL ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &stGrParam.pParamVal[0].clrGraph, QPF_NOINHERIT );
          break;

        case IDD_ST_TXCOL:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            WinQueryPresParam( WinWindowFromID( hwnd, IDD_ST_TXCOL ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &stGrParam.pParamVal[1].clrGraph, QPF_NOINHERIT );
          break;

        case IDD_SLID_TIME:
          if ( SHORT2FROMMP( mp1 ) == SLN_SLIDERTRACK ||
               SHORT2FROMMP( mp1 ) == SLN_CHANGE )
          {
            // Time slider position changed.

            ULONG	ulLen;
            ULONG	ulSec =
              LONGFROMMR( WinSendDlgItemMsg( hwnd, IDD_SLID_TIME,
                     SLM_QUERYSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ),
                     0 ) ) * 5;
            ldiv_t	stMinSec = ldiv( ulSec, 60 );

            ulLen = sprintf( &szBuf, "%u ", 1 + stMinSec.quot );
            ulLen += pstrLoad( 0, IDS_MIN_SHORT,
                                          sizeof(szBuf) - ulLen,
                                          &szBuf[ulLen] );	  // "min."
            if ( stMinSec.rem != 0 )
            {
              ulLen += sprintf( &szBuf[ulLen], " %u ", stMinSec.rem );
              pstrLoad( 0, IDS_SEC_SHORT,
                                      sizeof(szBuf) - ulLen,
                                      &szBuf[ulLen] );		// "sec."
            }

            WinSetDlgItemText( hwnd, IDD_ST_GRAPHTIME, &szBuf );
          }
          break;
      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}



// Data source's interface routine dsLoadDlg()
// -------------------------------------------

DSEXPORT ULONG APIENTRY dsLoadDlg(HWND hwndOwner, PHWND pahWnd)
{
  pahWnd[0] = WinLoadDlg( hwndOwner, hwndOwner, DialogProc,
                          hDSModule, IDD_DSPROPERTIES, NULL );

  return 1;
}
