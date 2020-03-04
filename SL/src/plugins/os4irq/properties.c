#include <ds.h>
#include <sl.h>
#include <stdio.h>
#include <ctype.h>     // isspace()
#include <string.h>
#include "os4irq.h"
#include <debug.h>     // Must be the last.

extern GRPARAM         stGrParam;          // os4irq.c
extern HMODULE         hDSModule;          // Module handle, os4irq.c
extern IRQPROPERTIES   stIRQProperties;    // os4irq.c
extern GRAPH           stGraph;            // os4irq.c
extern BOOL            fThickLines;        // Thick graph lines, os4irq.c
extern PSZ             apszFields[FIELDS]; // os4irq.c

// Helpers, os4irq.c
extern PHiniWriteStr             piniWriteStr;
extern PHiniWriteULong           piniWriteULong;
extern PHiniWriteData            piniWriteData;
extern PHgrSetTimeScale          pgrSetTimeScale;
extern PHupdateUnlock            pupdateUnlock;
extern PHupdateLock              pupdateLock;
extern PHstrLoad                 pstrLoad;
extern PHctrlStaticToColorRect   pctrlStaticToColorRect;
extern PHctrlQueryText           pctrlQueryText;

static BOOL            _fNameFixWidth;
static ULONG           clrSaveBackground;
static ULONG           clrSaveBorder;
static ULONG           clrSaveLine;
static BOOL            fSaveThickLines;
static ULONG           aulDefFields[FIELDS] =        // Fields default order
{
  DEF_ITEMFLD_0,
  DEF_ITEMFLD_1,
  DEF_ITEMFLD_2,
  DEF_ITEMFLD_3,
  DEF_ITEMFLD_4,
  DEF_ITEMFLD_5
};


#define STRCMP(s1,s2) strcmp((s1)==NULL ? "":(s1),(s2)==NULL ? "":(s2))
#define STRDUP(s) ((s) == NULL ? NULL : strdup(s))

/* Returns a pointer to a buffer with a string from which the leading and
   trailing spaces are removed. Returns NULL if the string is empty.
*/
static PSZ _queryEFText(HWND hwnd, ULONG ulId, ULONG cbBuf, PCHAR pcBuf)
{
  PCHAR      pcEnd;

  if ( cbBuf == 0 )
    return NULL;

  if ( WinQueryDlgItemText( hwnd, ulId, cbBuf, pcBuf ) == 0 )
  {
    pcBuf[0] = '\0';
    return NULL;
  }

  while( isspace( *pcBuf ) )
    pcBuf++;

  pcEnd = strchr( pcBuf, '\0' );
  while( ( pcEnd > pcBuf ) && isspace( *(pcEnd-1) ) )
    pcEnd--;

  *pcEnd = '\0';

  return pcBuf == pcEnd ? NULL : pcBuf;
}


/*
    Page 1
*/

static VOID _pg1default(HWND hwnd)
{
  LONG       lColor;
  HWND       hwndControl;
  SHORT      sIndex;
  ULONG      ulIdx;
  ULONG      ulField;

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

  // Thick lines checkbox checkbox.
  WinCheckButton( hwnd, IDD_CB_THICKLINES, DEF_THICKLINES );
  fThickLines = DEF_THICKLINES;

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

static VOID _pg1undo(HWND hwnd)
{
  HWND       hwndControl;
  ULONG      ulIdx;
  ULONG      ulField;
  SHORT      sIndex;

  // Colors
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BGCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrSaveBackground );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrSaveBorder );
  WinSetPresParam( WinWindowFromID( hwnd, IDD_ST_LINECOL ), PP_BACKGROUNDCOLOR,
                   sizeof(LONG), &clrSaveLine );

  // Thick lines checkbox
  WinCheckButton( hwnd, IDD_CB_THICKLINES, fSaveThickLines );
  fThickLines = fSaveThickLines;

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

static VOID _pg1WMInitDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  MRESULT    mr;
  CHAR       szBuf[MAX_IRQNAME_LEN + 10];
  PSZ        apszDalays[20];
  PSZ        pszPtr = &szBuf;
  ULONG      ulIdx;

  // Save current colors for undo.
  clrSaveBackground = stGrParam.clrBackground;
  clrSaveBorder = stGrParam.clrBorder;
  clrSaveLine = stGrParam.pParamVal[0].clrGraph;
  fSaveThickLines = fThickLines;

  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BGCOL ),
                                     clrSaveBackground );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_BORDERCOL ),
                                     clrSaveBorder );
  pctrlStaticToColorRect( WinWindowFromID( hwnd, IDD_ST_LINECOL ),
                                     clrSaveLine );

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

static VOID _pg1WMDestroy(HWND hwnd)
{
  CHAR       szBuf[MAX_IRQNAME_LEN + 10];
  HWND       hwndControl;
  ULONG      ulIdx;

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

  piniWriteData( hDSModule, "Fields", stIRQProperties.aulFields,
                 FIELDS * sizeof(ULONG) );
  piniWriteULong( hDSModule, "NameFixWidth",
                  (ULONG)stIRQProperties.fNameFixWidth );
  piniWriteULong( hDSModule, "GrBgColor", stGrParam.clrBackground );
  piniWriteULong( hDSModule, "GrBorderColor", stGrParam.clrBorder );
  piniWriteULong( hDSModule, "LineColor",
                             stGrParam.pParamVal[0].clrGraph );
  piniWriteULong( hDSModule, "thickLines", (ULONG)fThickLines );
  piniWriteULong( hDSModule, "UpdateInterval",
                                stIRQProperties.ulUpdateInterval );
  piniWriteULong( hDSModule, "TimeWindow",
                                stIRQProperties.ulTimeWindow );
}

static MRESULT EXPENTRY pg1DialogProc(HWND hwnd, ULONG msg,
                                      MPARAM mp1, MPARAM mp2)
{
  CHAR       szBuf[MAX_IRQNAME_LEN + 10];
  HWND       hwndControl;

  switch( msg )
  {
    case WM_INITDLG:
      _pg1WMInitDlg( hwnd, msg, mp1, mp2 );
      _pg1undo( hwnd );
      break;

    case WM_DESTROY:
      _pg1WMDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_UNDO:
          _pg1undo( hwnd );
          break;

        case IDD_PB_DEFAULT:
          _pg1default( hwnd );
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
      switch( SHORT1FROMMP(mp1) )
      {
        case IDD_CB_THICKLINES:
          if ( SHORT2FROMMP(mp1) == BN_CLICKED  )
            fThickLines = WinQueryButtonCheckstate( hwnd, IDD_CB_THICKLINES )
                            != 0;
          break;

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


/*
    Page 2
*/

typedef struct _IRQRECORD {
  MINIRECORDCORE       stRecCore;
  ULONG                ulIRQLevel;  // IRQ number.
  PSZ                  pszShowName; // Equals to pszShowName[0]/pszShowName[1]
  PSZ                  pszName;     // User defined name (allocated memory).
} IRQRECORD, *PIRQRECORD;

typedef struct _PG2WINDATA {
  PSZ        pszShowName[2];     // 0 - "auto detected", 1 - "user defined".
  PIRQRECORD pRecord;            // Cursored record.
  BOOL       fIgnoreSBIRQChanges;
} PG2WINDATA, *PPG2WINDATA;


/* Clears the name list container. Releases memory allocated for user-defined
   names.  */
static VOID _pg2CntClear(HWND hwndCnt)
{
  PIRQRECORD           pRecord;

  pRecord = (PIRQRECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD, 0,
                                    MPFROM2SHORT(CMA_FIRST,CMA_ITEMORDER) );
  while( ( pRecord != NULL ) && ( pRecord != ((PIRQRECORD)(-1)) ) )
  {
    if ( pRecord->pszName != NULL )
      free( pRecord->pszName );

    pRecord = (PIRQRECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD,
                                      MPFROMP(pRecord),
                                      MPFROM2SHORT(CMA_NEXT,CMA_ITEMORDER) );
  }

  WinSendMsg( hwndCnt, CM_REMOVERECORD, 0, MPFROM2SHORT(0, CMA_FREE) );
}

/* The function of comparing records for sorting. Compares IRQ numbers. */
static SHORT EXPENTRY _pg2CnrSort(PRECORDCORE p1, PRECORDCORE p2,
                                  PVOID pStorage)
{
  return (LONG)(((PIRQRECORD)p1)->ulIRQLevel) -
         (LONG)(((PIRQRECORD)p2)->ulIRQLevel);
}

/* Creates a new entry in the container.
   The pszShowName argument must be equal to one of the PG2WINDATA.pszShowName
   array entry.
   The string specified by pszName will be duplicated in the new record.
   Returns a pointer to the created record.
*/
static PIRQRECORD _pg2CnrInsert(HWND hwndCnt, ULONG ulIRQLevel, PSZ pszShowName,
                                PSZ pszName, BOOL fInvalidate)
{
  PIRQRECORD           pRecord;
  RECORDINSERT         stRecIns;

  pRecord = (PIRQRECORD)WinSendMsg( hwndCnt, CM_ALLOCRECORD,
                      MPFROMLONG(sizeof(IRQRECORD) - sizeof(MINIRECORDCORE)),
                      MPFROMLONG(1) );
  if ( pRecord != NULL )
  {
    pRecord->ulIRQLevel = ulIRQLevel;
    pRecord->pszShowName = pszShowName;
    pRecord->pszName = STRDUP(pszName);

    stRecIns.cb = sizeof(RECORDINSERT);
    stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
    stRecIns.pRecordParent = NULL;
    stRecIns.zOrder = (USHORT)CMA_TOP;
    stRecIns.cRecordsInsert = 1;
    stRecIns.fInvalidateRecord = fInvalidate;
    WinSendMsg( hwndCnt, CM_INSERTRECORD, (PRECORDCORE)pRecord, &stRecIns );
  }

  return pRecord;
}

/* Fills the form with current data. */
static VOID _pg2undo(HWND hwnd)
{
  HWND                 hwndCnt = WinWindowFromID( hwnd, IDD_CN_NAMES );
  PPG2WINDATA          pWinData;
  PIRQPRES             pPres;
  ULONG                ulIdx, ulShowName;

  pWinData = (PPG2WINDATA)WinQueryWindowPtr( hwnd, QWL_USER );
  if ( pWinData == NULL )
    return;

  _pg2CntClear( hwndCnt );

  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
  {
    pPres = &stIRQProperties.aPres[ulIdx];

    ulShowName = (pPres->ulFlags & PRESFL_SHOWAUTODETECT) != 0 ? 0 : 1;
    if ( ulShowName || ( pPres->pszName != NULL ) )
      _pg2CnrInsert( hwndCnt, ulIdx, pWinData->pszShowName[ulShowName],
                     pPres->pszName, FALSE );
  }

  WinSendMsg( hwndCnt, CM_INVALIDATERECORD, MPFROMP(NULL),
              MPFROM2SHORT(0,CMA_TEXTCHANGED) );
}

static VOID _pg2WMInitDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  HWND                 hwndSB = WinWindowFromID( hwnd, IDD_SB_IRQ );
  HWND                 hwndCB = WinWindowFromID( hwnd, IDD_CB_SHOW_NAME );
  HWND                 hwndCnt = WinWindowFromID( hwnd, IDD_CN_NAMES );
  PFIELDINFO           pFieldInfo;
  PFIELDINFO           pFldInf;
  CNRINFO              stCnrInf = { 0 };
  FIELDINFOINSERT      stFldInfIns = { 0 };
  CHAR                 acBuf[32];
  PPG2WINDATA          pWinData = malloc( sizeof(PG2WINDATA) );


  if ( pWinData == NULL )
  {
    debugCP( "Not enough memory" );
    return;
  }
  WinSetWindowPtr( hwnd, QWL_USER, pWinData );

  pWinData->fIgnoreSBIRQChanges = FALSE;
  pWinData->pRecord = NULL;

  WinLoadString( NULLHANDLE, hDSModule, IDS_AUTO_DETECTED, sizeof(acBuf), acBuf );
  pWinData->pszShowName[0] = strdup( acBuf );
  WinLoadString( NULLHANDLE, hDSModule, IDS_USER_DEFINED, sizeof(acBuf), acBuf );
  pWinData->pszShowName[1] = strdup( acBuf );

  // IRQ select spin button
  WinSendMsg( hwndSB, SPBM_SETLIMITS, MPFROMLONG(IRQ_MAX - 1), MPFROMLONG(0) );

  // Combobox to select "auto detected"/"user defined"
  WinSendMsg( hwndCB, LM_INSERTITEM, MPFROMSHORT(LIT_END),
              MPFROMP(pWinData->pszShowName[0]) );
  WinSendMsg( hwndCB, LM_INSERTITEM, MPFROMSHORT(LIT_END),
              MPFROMP(pWinData->pszShowName[1]) );
  WinSendMsg( hwndCB, LM_SELECTITEM, MPFROMSHORT(0), MPFROMSHORT(TRUE) );

  // Set container fields

  pFldInf = (PFIELDINFO)WinSendMsg( hwndCnt, CM_ALLOCDETAILFIELDINFO,
                                    MPFROMLONG(3), NULL );
  if ( pFldInf == NULL )
    debugCP( "WTF?!" );
  else
  {
    pFieldInfo = pFldInf;

    WinLoadString( NULLHANDLE, hDSModule, IDS_FLD_NUM, sizeof(acBuf), acBuf );
    pFieldInfo->cb = sizeof(FIELDINFO);
    pFieldInfo->flData = CFA_ULONG | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_VCENTER | CFA_SEPARATOR;
    pFieldInfo->flTitle = CFA_CENTER;
    pFieldInfo->pTitleData = strdup( acBuf );
    pFieldInfo->offStruct = FIELDOFFSET(IRQRECORD, ulIRQLevel);
    pFieldInfo = pFieldInfo->pNextFieldInfo;

    WinLoadString( NULLHANDLE, hDSModule, IDS_FLD_SHOW_NAME, sizeof(acBuf), acBuf );
    pFieldInfo->cb = sizeof(FIELDINFO);
    pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER | CFA_VCENTER | CFA_SEPARATOR;
    pFieldInfo->flTitle = CFA_CENTER;
    pFieldInfo->pTitleData = strdup( acBuf );
    pFieldInfo->offStruct = FIELDOFFSET(IRQRECORD, pszShowName);
    pFieldInfo = pFieldInfo->pNextFieldInfo;

    WinLoadString( NULLHANDLE, hDSModule, IDS_FLD_USER_DEF_NAME, sizeof(acBuf), acBuf );
    pFieldInfo->cb = sizeof(FIELDINFO);
    pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_LEFT;
    pFieldInfo->flTitle = CFA_CENTER;
    pFieldInfo->pTitleData = strdup( acBuf );
    pFieldInfo->offStruct = FIELDOFFSET(IRQRECORD, pszName);
    stCnrInf.pFieldInfoLast = pFieldInfo;

    stFldInfIns.cb = sizeof(FIELDINFOINSERT);
    stFldInfIns.pFieldInfoOrder = (PFIELDINFO)CMA_FIRST;
    stFldInfIns.cFieldInfoInsert = 3;
    WinSendMsg( hwndCnt, CM_INSERTDETAILFIELDINFO, MPFROMP(pFldInf),
                MPFROMP(&stFldInfIns) );
  }

  stCnrInf.cb = sizeof(CNRINFO);
  stCnrInf.flWindowAttr = CV_DETAIL | CA_DETAILSVIEWTITLES;
  stCnrInf.pSortRecord = _pg2CnrSort;
  WinSendMsg( hwndCnt, CM_SETCNRINFO, MPFROMP( &stCnrInf ),
              MPFROMLONG(CMA_PSORTRECORD | CMA_PFIELDINFOLAST |
                         CMA_FLWINDOWATTR) );
}

static VOID _pg2WMDestroy(HWND hwnd)
{
  HWND                 hwndCnt = WinWindowFromID( hwnd, IDD_CN_NAMES );
  PFIELDINFO           pFieldInfo;
  PPG2WINDATA          pWinData;
  PIRQRECORD           pRecord;
  PIRQRECORD           pIRQRecords[IRQ_MAX] = { NULL };
  ULONG                ulIdx, ulFlags;
  CHAR                 szIRQNameKey[16];
  CHAR                 szIRQFlagsKey[16];

  pWinData = (PPG2WINDATA)WinQueryWindowPtr( hwnd, QWL_USER );
  if ( pWinData == NULL )
    return;

  pRecord = (PIRQRECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD, 0,
                                    MPFROM2SHORT(CMA_FIRST,CMA_ITEMORDER) );
  while( ( pRecord != NULL ) && ( pRecord != ((PIRQRECORD)-1) ) )
  {
    if ( pRecord->ulIRQLevel >= IRQ_MAX )
      debug( "Invalid pRecord->ulIRQLevel: %lu, max. is %lu",
             pRecord->ulIRQLevel, IRQ_MAX - 1 );
    else
      pIRQRecords[pRecord->ulIRQLevel] = pRecord;

    pRecord = (PIRQRECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD,
                                      MPFROMP(pRecord),
                                      MPFROM2SHORT(CMA_NEXT,CMA_ITEMORDER) );
  }

  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
  {
    pRecord = pIRQRecords[ulIdx];

    if ( stIRQProperties.aPres[ulIdx].pszName != NULL )
      free( stIRQProperties.aPres[ulIdx].pszName );

    sprintf( szIRQNameKey, "IRQ%u", ulIdx );
    sprintf( szIRQFlagsKey, "IRQ%u_FLAGS", ulIdx );

    if ( pRecord == NULL )
    {
      if ( stIRQProperties.aPres[ulIdx].pszName != NULL )
        piniWriteData( hDSModule, szIRQNameKey, NULL, 0 );
      if ( stIRQProperties.aPres[ulIdx].ulFlags != PRESFL_SHOWAUTODETECT )
        piniWriteData( hDSModule, szIRQFlagsKey, NULL, 0 );

      stIRQProperties.aPres[ulIdx].pszName = NULL;
      stIRQProperties.aPres[ulIdx].ulFlags = PRESFL_SHOWAUTODETECT;
      continue;
    }

    ulFlags = pRecord->pszShowName == pWinData->pszShowName[0]
                ? PRESFL_SHOWAUTODETECT : 0;

    stIRQProperties.aPres[ulIdx].pszName = STRDUP( pRecord->pszName );
    stIRQProperties.aPres[ulIdx].ulFlags = ulFlags;

    piniWriteStr( hDSModule, szIRQNameKey, pRecord->pszName );
    piniWriteULong( hDSModule, szIRQFlagsKey, ulFlags );
  }

  _pg2CntClear( hwndCnt );

  if ( pWinData->pszShowName[0] != NULL )
    free( pWinData->pszShowName[0] );
  if ( pWinData->pszShowName[1] != NULL )
    free( pWinData->pszShowName[1] );

  free( pWinData );

  // Free memory allocated for container headers
  pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO, 0,
                                       MPFROMSHORT(CMA_FIRST) );
  while( ( pFieldInfo != NULL ) && ( (LONG)pFieldInfo != -1 ) )
  {
    if ( pFieldInfo->pTitleData != NULL )
      free( pFieldInfo->pTitleData );

    pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO,
                    MPFROMP( pFieldInfo ), MPFROMSHORT( CMA_NEXT ) );
  }
}

static VOID _pg2CNNamesEmphasis(HWND hwnd, PNOTIFYRECORDEMPHASIS pNotify)
{
  PIRQRECORD           pRecord = (PIRQRECORD)pNotify->pRecord;
  PPG2WINDATA          pWinData;

  if ( (pNotify->fEmphasisMask & CRA_CURSORED) == 0 ||
       (pNotify->pRecord->flRecordAttr & CRA_SELECTED) == 0 )
    return;

  // A new entry is selected in the container. We fill in the form for editing.

  WinSendDlgItemMsg( hwnd, IDD_SB_IRQ, SPBM_SETCURRENTVALUE,
                     MPFROMLONG(pRecord->ulIRQLevel), 0 );

  pWinData = (PPG2WINDATA)WinQueryWindowPtr( hwnd, QWL_USER );
  if ( pWinData != NULL )
  {
    pWinData->pRecord = pRecord;
    WinSendDlgItemMsg( hwnd, IDD_CB_SHOW_NAME, LM_SELECTITEM, MPFROMSHORT(
                         pRecord->pszShowName == pWinData->pszShowName[0]
                           ? 0 : 1 ),
                       MPFROMSHORT(TRUE) );
  }

  WinSetDlgItemText( hwnd, IDD_EF_USER_DEF_NAME, pRecord->pszName );
}

/* The user made changes for the selected IRQ. Change the Enabled state of the
   Set button.  */
static VOID _pg2checkChanges(HWND hwnd)
{
  ULONG                ulIRQLevel;
  SHORT                sShowName;
  CHAR                 acBuf[MAX_IRQNAME_LEN + 1];
  PCHAR                pcBuf;
  PPG2WINDATA          pWinData;

  pWinData = (PPG2WINDATA)WinQueryWindowPtr( hwnd, QWL_USER );
  if ( pWinData == NULL )
    return;

  if ( !(BOOL)WinSendDlgItemMsg( hwnd, IDD_SB_IRQ, SPBM_QUERYVALUE,
                     MPFROMP(&ulIRQLevel), MPFROM2SHORT(0,SPBQ_DONOTUPDATE) ) )
  {
    debugCP( "Can not query a current spin button value" );
    return;
  }

  sShowName = (SHORT)WinSendDlgItemMsg( hwnd, IDD_CB_SHOW_NAME,
                                        LM_QUERYSELECTION, MPFROMSHORT(0), 0 );
  if ( sShowName == LIT_NONE )
  {
    debugCP( "Can not query selected \"Show name\" value" );
    return;
  }

  pcBuf = _queryEFText( hwnd, IDD_EF_USER_DEF_NAME, sizeof(acBuf), acBuf );

  WinEnableControl( hwnd, IDD_PB_SET,

                    pWinData->pRecord == NULL

                      ? /* No records has been found for selected IRQ. */
                        ( sShowName != 0 ) || ( pcBuf != NULL )

                      : /* Compare user settings with current record. */
                        ( pWinData->pRecord->ulIRQLevel != ulIRQLevel ) ||
                        ( (pWinData->pRecord->pszShowName ==
                           pWinData->pszShowName[0] ? 0 : 1) != sShowName ) ||
                        ( STRCMP( pWinData->pRecord->pszName, pcBuf ) != 0 ) );
}

/* The user has changed the spin button control value (IRQ number). */
static VOID _pg2SBIRQChanged(HWND hwnd)
{
  PPG2WINDATA          pWinData;
  HWND                 hwndCnt = WinWindowFromID( hwnd, IDD_CN_NAMES );
  PIRQRECORD           pRecord;
  ULONG                ulIRQLevel;

  pWinData = (PPG2WINDATA)WinQueryWindowPtr( hwnd, QWL_USER );
  if ( pWinData == NULL || pWinData->fIgnoreSBIRQChanges )
    return;

  if ( !(BOOL)WinSendDlgItemMsg( hwnd, IDD_SB_IRQ, SPBM_QUERYVALUE,
                     MPFROMP(&ulIRQLevel), MPFROM2SHORT(0,SPBQ_DONOTUPDATE) ) )
  {
    debugCP( "Can not query a current spin button value" );
    return;
  }

  // Looking for a record of the selected IRQ in the container

  pRecord = (PIRQRECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD, 0,
                                    MPFROM2SHORT(CMA_FIRST,CMA_ITEMORDER) );
  if ( pRecord == ((PIRQRECORD)-1) )
    pRecord = NULL;

  while( pRecord != NULL )
  {
    if ( pRecord->ulIRQLevel == ulIRQLevel )
    {
      // Record for selected IRQ exists. Move the cursor to it.
      pWinData->fIgnoreSBIRQChanges = TRUE;
      WinSendMsg( hwndCnt, CM_SETRECORDEMPHASIS, MPFROMP(pRecord),
                  MPFROM2SHORT(TRUE,CRA_CURSORED) );
      pWinData->fIgnoreSBIRQChanges = FALSE;
      break;
    }

    pRecord = (PIRQRECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD, MPFROMP(pRecord),
                                      MPFROM2SHORT(CMA_NEXT,CMA_ITEMORDER) );
  }

  pWinData->pRecord = pRecord;
  _pg2checkChanges( hwnd );
}

/* "Set" button is pressed. */
static VOID _pg2SetBtn(HWND hwnd)
{
  HWND                 hwndCnt = WinWindowFromID( hwnd, IDD_CN_NAMES );
  ULONG                ulIRQLevel;
  SHORT                sShowName;
  CHAR                 acBuf[MAX_IRQNAME_LEN + 1];
  PCHAR                pcBuf;
  PPG2WINDATA          pWinData;

  pWinData = (PPG2WINDATA)WinQueryWindowPtr( hwnd, QWL_USER );
  if ( pWinData == NULL )
    return;

  // Request data from controls

  if ( !(BOOL)WinSendDlgItemMsg( hwnd, IDD_SB_IRQ, SPBM_QUERYVALUE,
                     MPFROMP(&ulIRQLevel), MPFROM2SHORT(0,SPBQ_DONOTUPDATE) ) )
  {
    debugCP( "Can not query a current spin button value" );
    return;
  }

  sShowName = (SHORT)WinSendDlgItemMsg( hwnd, IDD_CB_SHOW_NAME,
                                        LM_QUERYSELECTION, MPFROMSHORT(0), 0 );
  if ( sShowName == LIT_NONE )
  {
    debugCP( "Can not query selected \"Show name\" value" );
    return;
  }

  pcBuf = _queryEFText( hwnd, IDD_EF_USER_DEF_NAME, sizeof(acBuf), acBuf );

  // Making appropriate changes to the container records

  if ( ( sShowName == 0 ) && ( pcBuf == NULL ) )
  {
    /* Show auto-detected name, user is not given a name - this is the default
       state; no entry in the container is needed.  */
    if ( pWinData->pRecord != NULL )
    {
      PIRQRECORD       pRecord = pWinData->pRecord;
      
      pWinData->pRecord = NULL;
      if ( pRecord->pszName != NULL )
        free( pRecord->pszName );

      WinSendMsg( hwndCnt, CM_REMOVERECORD, MPFROMP(&pRecord),
                  MPFROM2SHORT(1,CMA_FREE | CMA_INVALIDATE) );
    }
    return;
  }

  if ( pWinData->pRecord != NULL )
  {
    /* The IRQ number specified in the form corresponds to the record in the
       container. We are making changes to this record.  */

    if ( pWinData->pRecord->pszName != NULL )
      free( pWinData->pRecord->pszName );

    pWinData->pRecord->pszName = STRDUP(pcBuf);
    pWinData->pRecord->pszShowName = pWinData->pszShowName[sShowName];

    WinSendMsg( hwndCnt, CM_INVALIDATERECORD, MPFROMP(&pWinData->pRecord),
                MPFROM2SHORT(1,CMA_TEXTCHANGED) );

    return;
  }

  /* There is no record in the container corresponding to the IRQ number
     specified in the form. Create a new record.  */

  pWinData->pRecord = _pg2CnrInsert( hwndCnt, ulIRQLevel,
                                     pWinData->pszShowName[sShowName],
                                     pcBuf, TRUE );

  // Select a created record
  pWinData->fIgnoreSBIRQChanges = TRUE;
  WinSendMsg( hwndCnt, CM_SETRECORDEMPHASIS, MPFROMP(pWinData->pRecord),
              MPFROM2SHORT(TRUE,CRA_CURSORED) );
  pWinData->fIgnoreSBIRQChanges = FALSE;
}


static MRESULT EXPENTRY pg2DialogProc(HWND hwnd, ULONG msg,
                                      MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _pg2WMInitDlg( hwnd, msg, mp1, mp2 );
      _pg2undo( hwnd );
      break;

    case WM_DESTROY:
      _pg2WMDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_SET:
          _pg2SetBtn( hwnd );
          break;

        case IDD_PB_UNDO:
          _pg2undo( hwnd );
          break;
      }
      return (MRESULT)FALSE;

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_CN_NAMES:
          if ( SHORT2FROMMP( mp1 ) == CN_EMPHASIS )
            _pg2CNNamesEmphasis( hwnd, (PNOTIFYRECORDEMPHASIS)mp2 );
          break;

        case IDD_SB_IRQ:
          if ( SHORT2FROMMP( mp1 ) == SPBN_CHANGE )
            _pg2SBIRQChanged( hwnd );
          break;

        case IDD_CB_SHOW_NAME:
          if ( SHORT2FROMMP( mp1 ) == CBN_LBSELECT )
            _pg2checkChanges( hwnd );
          break;

        case IDD_EF_USER_DEF_NAME:
          if ( SHORT2FROMMP( mp1 ) == EN_CHANGE )
            _pg2checkChanges( hwnd );
          break;
      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


DSEXPORT HWND APIENTRY dsLoadDlg(HWND hwndOwner, ULONG ulPage,
                                 ULONG cbTitle, PCHAR acTitle)
{
  // We have two property pages for this module
  struct {
    ULONG    ulTitleStrId;
    ULONG    ulDlgId;
    PFNWP    winProc;
  } aPages[2] = { { IDS_PROP_OPTIONS, IDD_IRQ1, pg1DialogProc },
                  { IDS_PROP_NAMES, IDD_IRQ2, pg2DialogProc } };

  WinLoadString( NULLHANDLE, hDSModule, aPages[ulPage].ulTitleStrId,
                 cbTitle, acTitle );
  
  return WinLoadDlg( hwndOwner, hwndOwner, aPages[ulPage].winProc,
                     hDSModule, aPages[ulPage].ulDlgId, NULL );
}
