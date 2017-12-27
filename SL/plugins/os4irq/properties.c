#include <ds.h>
#include <sl.h>
#include <stdio.h>
#include <ctype.h>		/* isspace() */
#include <string.h>
#include "os4irq.h"
#include "ctrl.h"
#include <debug.h>

extern GRPARAM		stGrParam;		// os4irq.c
extern HMODULE		hDSModule;		// Module handle, os4irq.c
extern IRQPROPERTIES	stIRQProperties;	// os4irq.c
extern GRAPH		stGraph;		// os4irq.c
extern PSZ		apszFields[FIELDS];	// os4irq.c

// Helpers, os4irq.c
extern PHiniWriteStr		piniWriteStr;
extern PHiniWriteULong		piniWriteULong;
extern PHiniWriteData		piniWriteData;
extern PHgrSetTimeScale		pgrSetTimeScale;
extern PHupdateUnlock		pupdateUnlock;
extern PHupdateLock		pupdateLock;
extern PHstrLoad		pstrLoad;
extern PHctrlStaticToColorRect	pctrlStaticToColorRect;
extern PHctrlQueryText		pctrlQueryText;

static BOOL		_fNameFixWidth;
static ULONG		clrBackground;
static ULONG		clrBorder;
static ULONG		clrLine;
static ULONG		aulDefFields[FIELDS] =	// fields default order
{
  DEF_ITEMFLD_0,
  DEF_ITEMFLD_1,
  DEF_ITEMFLD_2,
  DEF_ITEMFLD_3,
  DEF_ITEMFLD_4,
  DEF_ITEMFLD_5
};

static VOID _default(HWND hwnd)
{
  LONG		lColor;
  HWND		hwndControl;
  SHORT		sIndex;
  ULONG		ulIdx;
  ULONG		ulField;

  // Colors

  lColor = DEF_GRBGCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );
  lColor = DEF_GRBORDERCOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );
  lColor = DEF_LINECOLOR;
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_LINECOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &lColor );

  // IRQ names

  WinSetDlgItemText( hwnd, IDD_EF_NO, "" );
  WinSetDlgItemText( hwnd, IDD_EF_NAME, "" );
  WinSendDlgItemMsg( hwnd, IDD_LB_NAMES, LM_DELETEALL, 0, 0 );

  // Item fields

  _fNameFixWidth = DEF_ITEMNAMEFIXWIDTH;
  hwndControl = WinWindowFromID( hwnd, IDD_LB_FIELDS );
  WinSendMsg( hwndControl, LM_DELETEALL, 0, 0 );
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    // Get field default number (w/wo flag "hidden")
    ulField = aulDefFields[ulIdx];

    // Insert field name and set field number (w/wo flag "hidden") for item.
    sIndex = WinInsertLboxItem( hwndControl, LIT_END,
                                apszFields[ulField & ~FLD_FL_HIDDEN] );
    if ( sIndex != LIT_MEMERROR && sIndex != LIT_ERROR )
      WinSendMsg( hwndControl, LM_SETITEMHANDLE, MPFROMSHORT( sIndex ),
                  MPFROMLONG( ulField ) );
  }
  // Select first item.
  WinSendMsg( hwndControl, LM_SELECTITEM, 0, MPFROMSHORT( 1 ) );

  // Graph time slider. Position of the slider arm.
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ),
                     MPFROMSHORT( (DEF_TIMEWIN - 60) / 5 ) );

  // Update interval
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETCURRENTVALUE,
                     MPFROMLONG( DEF_INTERVAL / 100 - 1 ), 0 );
}

static VOID _undo(HWND hwnd)
{
  HWND		hwndControl;
  ULONG		ulIdx;
  ULONG		ulField;
  SHORT		sIndex;
  CHAR		szBuf[MAX_IRQNAME_LEN + 10];

  // Colors
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrBackground );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrBorder );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_LINECOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrLine );

  // IRQ names

  WinSetDlgItemText( hwnd, IDD_EF_NO, "" );
  WinSetDlgItemText( hwnd, IDD_EF_NAME, "" );
  WinSendDlgItemMsg( hwnd, IDD_LB_NAMES, LM_DELETEALL, 0, 0 );

  hwndControl = WinWindowFromID( hwnd, IDD_LB_NAMES );
  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
  {
    if ( stIRQProperties.apszNames[ulIdx] == NULL )
      continue;
    sprintf( &szBuf, "IRQ %u: %s", ulIdx, stIRQProperties.apszNames[ulIdx] );
    WinInsertLboxItem( hwndControl, LIT_END, &szBuf );
  }

  // Item fields

  _fNameFixWidth = stIRQProperties.fNameFixWidth;
  hwndControl = WinWindowFromID( hwnd, IDD_LB_FIELDS );
  WinSendMsg( hwndControl, LM_DELETEALL, 0, 0 );

  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    // Get field number (w/wo flag "hidden")
    ulField = stIRQProperties.aulFields[ulIdx];

    // Insert field name and set field number (w/wo flag "hidden") for item.
    sIndex = WinInsertLboxItem( hwndControl, LIT_END,
                                apszFields[ulField & ~FLD_FL_HIDDEN] );
    if ( sIndex != LIT_MEMERROR && sIndex != LIT_ERROR )
      WinSendMsg( hwndControl, LM_SETITEMHANDLE, MPFROMSHORT( sIndex ),
                  MPFROMLONG( ulField ) );
  }
  // Select first item.
  WinSendMsg( hwndControl, LM_SELECTITEM, 0, MPFROMSHORT( 1 ) );

  // Update interval
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETCURRENTVALUE,
                     MPFROMLONG( stIRQProperties.ulUpdateInterval / 100 - 1 ), 0 );

  // Graph time slider. Position of the slider arm.
  WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_SETSLIDERINFO,
                     MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ),
                     MPFROMSHORT( (stIRQProperties.ulTimeWindow - 60) / 5 ) );
}

static VOID _wmInitDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  MRESULT	mr;
  CHAR		szBuf[MAX_IRQNAME_LEN + 10];
  PSZ		apszDalays[20];
  PSZ		pszPtr = &szBuf;
  ULONG		ulIdx;

  // Save current colors for undo.
  clrBackground = stGrParam.clrBackground;
  clrBorder = stGrParam.clrBorder;
  clrLine = stGrParam.pParamVal[0].clrGraph;

  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BGCOL ),
                                     clrBackground );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ),
                                     clrBorder );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_LINECOL ),
                                     clrLine );

  // IRQ names

  WinSendDlgItemMsg( hwnd, IDD_EF_NO, EM_SETTEXTLIMIT, MPFROMSHORT( 3 ), 0 );
  WinSendDlgItemMsg( hwnd, IDD_EF_NAME, EM_SETTEXTLIMIT, MPFROMSHORT( 128 ), 0 );

  // Update interval

  for( ulIdx = 1; ulIdx < 21; ulIdx++ )
  {
    apszDalays[ulIdx - 1] = pszPtr;
    pszPtr += 1 + sprintf( pszPtr, "%u.%u", ulIdx / 10, ulIdx % 10 );
  }
  WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_SETARRAY,
                     MPFROMP( &apszDalays ), MPFROMLONG( 20 ) );

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

  debugStat();
}

static VOID _wmDestroy(HWND hwnd)
{
  CHAR		szBuf[MAX_IRQNAME_LEN + 10];
  PCHAR		pCh;
  HWND		hwndControl;
  ULONG		ulIdx;
  ULONG		ulNo;

  // Apply IRQ names list

  // Remove existing names
  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
    if ( stIRQProperties.apszNames[ulIdx] != NULL )
    {
      debugFree( stIRQProperties.apszNames[ulIdx] );
      stIRQProperties.apszNames[ulIdx] = NULL;
    }

  // Store new names
  hwndControl = WinWindowFromID( hwnd, IDD_LB_NAMES );
  for( ulIdx = 0; ulIdx < WinQueryLboxCount( hwndControl ); ulIdx++ )
  {
    if ( WinQueryLboxItemText( hwndControl, (SHORT)ulIdx, &szBuf,
                               sizeof(szBuf) ) < 8 )
      continue;

    ulNo = (ULONG)strtoul( &szBuf[4], &pCh, 10 );
    if ( ulNo < IRQ_MAX && *pCh == ':' &&
         stIRQProperties.apszNames[ulNo] == NULL )
    {
      while( isspace( *(++pCh) ) );
      stIRQProperties.apszNames[ulNo] = debugStrDup( pCh );
    }
  }

  // Apply item fields (Led, Counter, Value increase)

  stIRQProperties.fNameFixWidth = _fNameFixWidth;
  hwndControl = WinWindowFromID( hwnd, IDD_LB_FIELDS );
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    stIRQProperties.aulFields[ulIdx] =
      (ULONG)WinSendMsg( hwndControl, LM_QUERYITEMHANDLE,
                         MPFROMSHORT( ulIdx ), 0 );
  }

  pupdateLock();

  // Apply update period

  if ( WinSendDlgItemMsg( hwnd, IDD_SB_INTERVAL, SPBM_QUERYVALUE,
         MPFROMP( &szBuf ),
         MPFROM2SHORT( sizeof(szBuf), SPBQ_DONOTUPDATE  ) ) )
  {
    stIRQProperties.ulUpdateInterval = ( (szBuf[0] - '0') * 1000 ) +
                                       ( (szBuf[2] - '0') * 100 );
  }

  // Apply graph time window

  stIRQProperties.ulTimeWindow = 60 + 5 *
    (ULONG)WinSendDlgItemMsg( hwnd, IDD_SLID_TIME, SLM_QUERYSLIDERINFO,
            MPFROM2SHORT( SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE ), 0 );

  ulIdx = stIRQProperties.ulTimeWindow * 1000;
  pgrSetTimeScale( &stGraph,
                ulIdx / stIRQProperties.ulUpdateInterval, ulIdx );

  pupdateUnlock();

  // Store properties to the ini-file

  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
  {
    sprintf( &szBuf, "IRQ%u", ulIdx );
    piniWriteStr( hDSModule, &szBuf,
                             stIRQProperties.apszNames[ulIdx] );
  }

  piniWriteData( hDSModule, "Fields",
                                &stIRQProperties.aulFields,
                                FIELDS * sizeof(ULONG) );
  piniWriteULong( hDSModule, "NameFixWidth",
                                (ULONG)stIRQProperties.fNameFixWidth );
  piniWriteULong( hDSModule, "GrBgColor", stGrParam.clrBackground );
  piniWriteULong( hDSModule, "GrBorderColor", stGrParam.clrBorder );
  piniWriteULong( hDSModule, "LineColor",
                             stGrParam.pParamVal[0].clrGraph );
  piniWriteULong( hDSModule, "UpdateInterval",
                                stIRQProperties.ulUpdateInterval );
  piniWriteULong( hDSModule, "TimeWindow",
                                stIRQProperties.ulTimeWindow );
}


static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  CHAR		szBuf[MAX_IRQNAME_LEN + 10];
  PCHAR		pCh;
  HWND		hwndControl;

  switch( msg )
  {
    case WM_INITDLG:
      _wmInitDlg( hwnd, msg, mp1, mp2 );
    case WM_SL_UNDO:
      _undo( hwnd );
      break;

    case WM_SL_DEFAULT:
      _default( hwnd );
      break;

    case WM_DESTROY:
      _wmDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_SET:
          {
            // Button "Set" for IRQ name pressed.

            SHORT		sNo, sFoundNo = -1;
            ULONG		ulLen;
            ULONG		ulIdx;

            if ( !WinQueryDlgItemShort( hwnd, IDD_EF_NO, &sNo, FALSE ) ||
                 sNo >= IRQ_MAX )
            {
              CHAR	szText[256];

              pstrLoad( hDSModule, IDS_INVALID_NUM_TITLE,
                                   sizeof(szBuf), &szBuf );
              pstrLoad( hDSModule, IDS_INVALID_NUM_TEXT,
                                   sizeof(szText), &szText );
              WinMessageBox( HWND_DESKTOP, hwnd, &szText, &szBuf, 1,
                             MB_ICONHAND | MB_MOVEABLE | MB_CUACRITICAL | MB_OK );
              break;
            }

            // Search IRQ No. in the names list.
            hwndControl = WinWindowFromID( hwnd, IDD_LB_NAMES );
            for( ulIdx = 0; ulIdx < WinQueryLboxCount( hwndControl ); ulIdx++ )
            {
              if ( WinQueryLboxItemText( hwndControl, (SHORT)ulIdx, &szBuf, 10 ) < 8 )
                continue;

              sFoundNo = (SHORT)strtoul( &szBuf[4], &pCh, 10 );
              if ( sFoundNo < 0 || sFoundNo >= IRQ_MAX || *pCh != ':' )
                sFoundNo = -1;
              else if ( sFoundNo == sNo )
              {
                // Found - remove list item, quit search
                WinDeleteLboxItem( hwndControl, (SHORT)ulIdx );
                break;
              }
            }

            // Build a new names list item.
            ulLen = sprintf( &szBuf, "IRQ %u: ", sNo );
            pctrlQueryText( WinWindowFromID( hwnd, IDD_EF_NAME ),
                                 sizeof(szBuf) - ulLen - 1, &szBuf[ulLen] );

            // Insert new item to the IRQ names list.
            ulIdx = WinInsertLboxItem( hwndControl,
                                      sFoundNo == -1 ? LIT_END : ulIdx, &szBuf );
            // Select new item.
            WinSendMsg( hwndControl, LM_SELECTITEM, MPFROMSHORT( ulIdx ),
                        MPFROMSHORT( 1 ) );
          }
          break;

        case IDD_PB_UP:
        case IDD_PB_DOWN:
          {
            LONG		lSelIdx;
            ULONG		ulField;

            hwndControl = WinWindowFromID( hwnd, IDD_LB_FIELDS );
            lSelIdx = WinQueryLboxSelectedItem( hwndControl );
            // Save selected item text and field number.
            WinQueryLboxItemText( hwndControl, lSelIdx, &szBuf, sizeof(szBuf) );
            ulField = (ULONG)WinSendMsg( hwndControl, LM_QUERYITEMHANDLE,
                                         MPFROMSHORT( lSelIdx ), 0 );
            // Delete selected item.
            WinSendMsg( hwndControl, LM_DELETEITEM, MPFROMSHORT( lSelIdx ), 0 );
            // Change index.
            lSelIdx += SHORT1FROMMP( mp1 ) == IDD_PB_UP ? -1 : 1;
            // Insert item on new place with saved text and field number.
            WinInsertLboxItem( hwndControl, lSelIdx, &szBuf );
            WinSendMsg( hwndControl, LM_SETITEMHANDLE, MPFROMSHORT( lSelIdx ),
                        MPFROMLONG( ulField ) );
            // Select new item.
            WinSendMsg( hwndControl, LM_SELECTITEM, MPFROMSHORT( lSelIdx ),
                        MPFROMSHORT( 1 ) );
          }
          break;
      }
      return (MRESULT)FALSE;

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

        case IDD_ST_LINECOL:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            WinQueryPresParam( WinWindowFromID( hwnd, IDD_ST_LINECOL ),
                               PP_BACKGROUNDCOLOR, 0, NULL, sizeof(LONG),
                               &stGrParam.pParamVal[0].clrGraph, QPF_NOINHERIT );
          break;

        case IDD_LB_NAMES:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT &&
               WinQueryLboxItemText( HWNDFROMMP( mp2 ),
                                   WinQueryLboxSelectedItem( HWNDFROMMP(mp2) ),
                                   &szBuf, sizeof(szBuf) ) > 5 )
          {
            pCh = strchr( &szBuf, ':' );
            if ( pCh != NULL )
            {
              *pCh = '\0';
              WinSetDlgItemText( hwnd, IDD_EF_NO, &szBuf[4] );
              while( isspace( *(++pCh) ) );
              WinSetDlgItemText( hwnd, IDD_EF_NAME, pCh );
            }
          }
          break;

        case IDD_LB_FIELDS:
          if ( SHORT2FROMMP( mp1 ) == LN_SELECT )
          {
            LONG	lSelIdx = WinQueryLboxSelectedItem( HWNDFROMMP( mp2 ) );
            ULONG	ulField = (ULONG)WinSendMsg( HWNDFROMMP( mp2 ),
                                    LM_QUERYITEMHANDLE, MPFROMSHORT(lSelIdx), 0 );
            BOOL	fFldNameSel = (ulField & ~FLD_FL_HIDDEN) == FLD_NAME;

            // Enable/disable buttons Up/Down.
            WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_UP ), lSelIdx > 0 );
            WinEnableWindow( WinWindowFromID( hwnd, IDD_PB_DOWN ),
                             lSelIdx < WinQueryLboxCount( HWNDFROMMP(mp2) ) - 1 );
            // Check button "Visible"
            WinCheckButton( hwnd, IDD_CB_VISIBLE,
                            (ulField & FLD_FL_HIDDEN) == 0 );
            // Enable/disable check button "Fixed width"
            WinEnableWindow( WinWindowFromID( hwnd, IDD_CB_FIXEDWIDTH ),
                             fFldNameSel );
            // Check check button "Fixed width"
            WinCheckButton( hwnd, IDD_CB_FIXEDWIDTH,
                            _fNameFixWidth || !fFldNameSel );
          }
          break;

        case IDD_CB_VISIBLE:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED ||
               SHORT2FROMMP( mp1 ) == BN_DBLCLICKED )
          {
            LONG	lSelIdx;
            ULONG	ulField;
            USHORT	sChecked = WinQueryButtonCheckstate( hwnd, SHORT1FROMMP( mp1 ) );

            // Get field number from selected item's handle.
            hwndControl = WinWindowFromID( hwnd, IDD_LB_FIELDS );
            lSelIdx = WinQueryLboxSelectedItem( hwndControl );
            ulField = (ULONG)WinSendMsg( hwndControl, LM_QUERYITEMHANDLE,
                                         MPFROMSHORT( lSelIdx ), 0 );
            // Set/remove flag "hidden".
            if ( sChecked )
              ulField &= ~FLD_FL_HIDDEN;
            else
              ulField |= FLD_FL_HIDDEN;
            // Set new field number to selected item's handle.
            WinSendMsg( hwndControl, LM_SETITEMHANDLE, MPFROMSHORT( lSelIdx ),
                        MPFROMLONG( ulField ) );
          }
          break;

        case IDD_CB_FIXEDWIDTH:
          if ( SHORT2FROMMP( mp1 ) == BN_CLICKED ||
               SHORT2FROMMP( mp1 ) == BN_DBLCLICKED )
            _fNameFixWidth = WinQueryButtonCheckstate( hwnd,
                               SHORT1FROMMP( mp1 ) ) != 0;
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


DSEXPORT ULONG APIENTRY dsLoadDlg(HWND hwndOwner, PHWND pahWnd)
{
  pahWnd[0] = WinLoadDlg( hwndOwner, hwndOwner, DialogProc,
                          hDSModule, IDD_IRQ, NULL );

  return 1;
}
