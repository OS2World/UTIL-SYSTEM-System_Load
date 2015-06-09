/*
  Interface functions of plugin.
*/

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#define INCL_DOSERRORS
#define INCL_DOSMISC		/* DOS Miscellaneous values  */
#include <ds.h>
#include <sl.h>
#include <debug.h>
#include "traffic.h"
#include "iptrace.h"
#include "strutils.h"

HMODULE			hDSModule;	// Module handle.
HAB			hab;

// AUTOSAVE_PERIOD - save statistics every 2 minutes.
#define AUTOSAVE_PERIOD		(1000 * 60 * 2)

#define IDM_COMPACT		IDM_DSCMD_FIRST_ID
#define IDM_DETAILED		IDM_DSCMD_FIRST_ID + 1
#define IDM_EXPAND_ALL		IDM_DSCMD_FIRST_ID + 2
#define IDM_COLLAPSE_ALL	IDM_DSCMD_FIRST_ID + 3
#define IDM_FILTER		IDM_DSCMD_FIRST_ID + 4
#define IDM_TRACE		IDM_DSCMD_FIRST_ID + 5

#define WC_DS_TRACE		"DSTrafficNodeTrace"

#define STO_GROUP	0
#define STO_ITEM	1

ULONG		ulTimeWindow;
ULONG		ulUpdateInterval;
ULONG		ulGroupRates;

// grValToStr() callback fn. for ordinate minimum and maximum captions.
static LONG grValToStr(ULONG ulVal, PCHAR pcBuf, ULONG cbBuf);

GRVALPARAM	aGrParamVal[2] =
{
//  clrGraph,   ulLineWidth, ulPlygonBright.
  { DEF_TXCOLOR,          1,             80 },	// Sent
  { DEF_RXCOLOR,          1,              0 }	// Received
};

GRPARAM		stGrParam =	// Graph paramethers for lines.
{
  GRPF_MIN_LABEL | GRPF_MAX_LABEL | GRPF_TIME_LABEL | GRPF_LEFT_TOP_CAPTION,	// ulFlags
  NULL,						// pszAbscissaTopCaption
  NULL,						// pszAbscissaBottomCaption
  NULL,						// pszOrdinateCaption
  grValToStr,					// fnValToStr
  1,						// ulBorderCX
  1,						// ulBorderCY
  DEF_GRBORDERCOLOR,				// clrBorder
  DEF_GRBGCOLOR,				// clrBackground
  ARRAYSIZE( aGrParamVal ),			// cParamVal
  aGrParamVal					// pParamVal
};

static ULONG		aulFields[] = {
  IDS_NODE_NAME, IDS_SENT_BYTES, IDS_SEND_RATIO, IDS_RECV_BYTES, IDS_RECV_RATIO
};
#define FIELDS_NUMB	ARRAYSIZE(aulFields)

// Data source information.
static DSINFO stDSInfo =
{
  sizeof(DSINFO),		// Size, in bytes, of this data structure.
  (PSZ)IDS_DS_TITLE,		// Data source module's title (resource string
                    		// ID, see flag DS_FL_RES_MENU_TITLE).
  FIELDS_NUMB,			// Number of "sort-by" strings.
  (PSZ *)&aulFields,		// "Sort-by" strings (array of resource string
                    		// IDs, see flag DS_FL_RES_SORT_BY).
  DS_FL_ITEM_BG_LIST |		// Flags DS_FL_*
  DS_FL_SELITEM_BG_LIST |
  DS_FL_RES_MENU_TITLE |
  DS_FL_RES_SORT_BY,
  40,				// Items horisontal space, %Em
  0,				// Items vertical space, %Em
  20801				// Help main panel index.
};

LINKSEQ			lsStObjGroups;

static ULONG		ulSortBy;
static BOOL		fSortDesc;
static PIPTSNAPSHOT	pSnapshot;
static ULONG		cbSnapshot;
static LONG		lSelItemBgColor;
static LONG		lSelItemFgColor;
static LONG		lItemFgColor;
static PSIZEL		psizeEm;
static FONTMETRICS	stFontMetrics;

static PSTOBJ		*papStObj;
static ULONG		cStObj;
static ULONG		ulMaxStObj;
static ULONG		ulSelectedItem;
static ULONG		ulCmdOnItem;
static HBITMAP		hbmMinus;
static HBITMAP		hbmPlus;
static SIZEL		sizeBMExpand;
static ULONG		ulItemLPad = 20;
static ULONG		ulItemDataHOffs = 10;
static ULONG		ulLineHeight = 20;
static ULONG		ulXOffsCounterSp;
static ULONG		ulXOffsSpeedSp;
static ULONG		ulDataStrWidth;
static BOOL		fCompactItems;
static ULONG		ulLastStoreTime;

static LONG		lLedSendOn;
static LONG		lLedSendOff;
static LONG		lLedRecvOn;
static LONG		lLedRecvOff;


// Pointers to helper routines.

PHiniWriteULong			piniWriteULong;
PHiniReadULong			piniReadULong;
PHctrlDlgCenterOwner		pctrlDlgCenterOwner;
PHctrlQueryText			pctrlQueryText;
PHgrInit			pgrInit;
PHgrDone			pgrDone;
PHgrSetTimeScale		pgrSetTimeScale;
PHgrNewTimestamp		pgrNewTimestamp;
PHgrGetTimestamp		pgrGetTimestamp;
PHgrInitVal			pgrInitVal;
PHgrDoneVal			pgrDoneVal;
PHgrSetValue			pgrSetValue;
PHgrGetValue			pgrGetValue;
PHgrDraw			pgrDraw;
PHutilGetTextSize		putilGetTextSize;
PHutilBox			putilBox;
PHutilMixRGB			putilMixRGB;
PHutilGetColor			putilGetColor;
PHutilWriteResStr		putilWriteResStr;
PHutilLoadInsertStr		putilLoadInsertStr;
PHutilQueryProgPath		putilQueryProgPath;
PHutil3DFrame			putil3DFrame;
PHupdateLock			pupdateLock;
PHupdateUnlock			pupdateUnlock;
PHupdateForce			pupdateForce;
PHstrFromSec			pstrFromSec;
PHstrFromBytes			pstrFromBytes;
PHstrLoad			pstrLoad;
PHstrFromULL			pstrFromULL;
PHctrlStaticToColorRect		pctrlStaticToColorRect;
PHhelpShow			phelpShow;

// showFilter(), filter.c.
extern VOID showFilter(HWND hwndOwner, PVOID pSCNodeUser);
// traceStart(), tracewin.c
extern VOID traceStart(HWND hwndOwner, PSCNODE pNode);
// deffltCreate(), defflt.c
extern VOID deffltCreate();

extern HWND		hwndTrace;		// tracewin.c


static VOID _filterStore()
{
  CHAR		szFile[CCHMAXPATH - 12];
  ULONG		cbPath = putilQueryProgPath( sizeof(szFile), &szFile );
  PULONG	pulExt = (PULONG)&szFile[cbPath + 8];
  ULONG		ulRC;

  strcpy( &szFile[cbPath], "traffic.$$$" );

  if ( !iptStoreFile( &szFile ) )
    debug( "Filter was not stored" );
  else
  {
    *pulExt = (ULONG)'\0kab';
    DosDelete( &szFile );

    *pulExt = (ULONG)'\0tad';
    ulRC = DosMove( &szFile, "traffic.bak" );
    if ( ulRC != NO_ERROR )
      debug( "Rename %s to *.bak, rc = %u", &szFile, ulRC );

    *pulExt = (ULONG)'\0$$$';
    ulRC = DosMove( &szFile, "traffic.dat" );
    if ( ulRC != NO_ERROR )
      debug( "Rename %s to *.dat, rc = %u", &szFile, ulRC );
  }
}

static BOOL _stObjUpdateStr(PSTSTR pstStr, ULONG cbStr, PCHAR pcStr)
{
  PSZ		pszStr;

  if ( pstStr->cbStr == cbStr )
  {
    memcpy( pstStr->pszStr, pcStr, cbStr );
    return TRUE;
  }

  if ( ( cbStr == 0 ) || ( pcStr == NULL ) )
  {
    pszStr = NULL;
    cbStr = 0;
  }
  else
  {
    pszStr = debugMAlloc( cbStr + 1 );
    if ( pszStr == NULL )
    {
      debug( "Not enough memory" );
      return FALSE;
    }
    memcpy( pszStr, pcStr, cbStr );
    pszStr[cbStr] = '\0';
  }

  if ( pstStr->pszStr != NULL )
    debugFree( pstStr->pszStr );

  pstStr->pszStr = pszStr;
  pstStr->cbStr = cbStr;

  return TRUE;
}

static PSTOBJITEM _stObjItemNew(PSTOBJGROUP pStObjGroup,
                                ULONG cbName, PCHAR pcName)
{
  PSTOBJITEM	pStObjItem = debugCAlloc( 1, sizeof(STOBJITEM) );

  if ( pStObjItem == NULL )
  {
    debug( "Not enough memory" );
    return NULL;
  }

  pStObjItem->stObj.ulType = STO_ITEM;
  if ( !_stObjUpdateStr( &pStObjItem->stObj.stName, cbName, pcName ) )
  {
    debugFree( pStObjItem );
    return NULL;
  }

  pgrInitVal( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValSent );
  pgrInitVal( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValRecv );
  pStObjItem->pStObjGroup = pStObjGroup;

  return pStObjItem;
}

static VOID _stObjItemFree(PSTOBJITEM pStObjItem)
{
  if ( pStObjItem->_stObj_pszName != NULL )
    debugFree( pStObjItem->_stObj_pszName );

  if ( pStObjItem->stComment.pszStr != NULL )
    debugFree( pStObjItem->stComment.pszStr );

  pgrDoneVal( &pStObjItem->stObj.stGrValSent );
  pgrDoneVal( &pStObjItem->stObj.stGrValRecv );
  debugFree( pStObjItem );
}

static PSTOBJGROUP _stObjGroupNew(PCHAR pcName, ULONG cbName)
{
  PSTOBJGROUP	pStObjGroup = debugCAlloc( 1, sizeof(STOBJGROUP) );
  ULONG		ulTime;
  PSTOBJGROUP	pScan;

  if ( pStObjGroup == NULL )
  {
    debug( "Not enough memory" );
    return NULL;
  }

  pStObjGroup->stObj.ulType = STO_GROUP;
  if ( !_stObjUpdateStr( &pStObjGroup->stObj.stName, cbName, pcName ) )
  {
    debugFree( pStObjGroup );
    return NULL;
  }

  pStObjGroup->fExpanded = TRUE;
  lnkseqInit( &pStObjGroup->lsItems );

  ulTime = ulTimeWindow * 1000;
  if ( !pgrInit( &pStObjGroup->stGraph, ulTime / ulUpdateInterval, ulTime,
                 0, 1024 ) )
  {
    debug( "grInit() fail" );
    if ( pStObjGroup->_stObj_pszName != NULL )
      debugFree( pStObjGroup->_stObj_pszName );
    debugFree( pStObjGroup );
    return NULL;
  }

  pgrInitVal( &pStObjGroup->stGraph, &pStObjGroup->stObj.stGrValSent );
  pgrInitVal( &pStObjGroup->stGraph, &pStObjGroup->stObj.stGrValRecv );

  pScan = (PSTOBJGROUP)lnkseqGetFirst( &lsStObjGroups );
  while( pScan != NULL )
  {
    if ( _STR_stricmp( pScan->_stObj_pszName, pStObjGroup->_stObj_pszName ) > 0 )
      break;
    pScan = (PSTOBJGROUP)lnkseqGetNext( pScan );
  }

  if ( pScan == NULL )
    lnkseqAdd( &lsStObjGroups, pStObjGroup );
  else
    lnkseqAddBefore( &lsStObjGroups, pScan, pStObjGroup );

  return pStObjGroup;
}

static VOID _stObjGroupFree(PSTOBJGROUP pStObjGroup)
{
  pgrDoneVal( &pStObjGroup->stObj.stGrValSent );
  pgrDoneVal( &pStObjGroup->stObj.stGrValRecv );

  if ( pStObjGroup->_stObj_pszName != NULL )
    debugFree( pStObjGroup->_stObj_pszName );

  lnkseqFree( &pStObjGroup->lsItems, PSTOBJITEM, _stObjItemFree );

  pgrDone( &pStObjGroup->stGraph );
  debugFree( pStObjGroup );
}

static PSTOBJGROUP _stObjGroupFind(PCHAR pcName, ULONG cbName)
{
  PSTOBJGROUP	pStObjGroup = (PSTOBJGROUP)lnkseqGetFirst( &lsStObjGroups );

  while( pStObjGroup != NULL )
  {
    if ( ( pStObjGroup->_stObj_cbName == cbName ) &&
         ( memicmp( pStObjGroup->_stObj_pszName, pcName, cbName ) == 0 ) )
      break;
    pStObjGroup = (PSTOBJGROUP)lnkseqGetNext( pStObjGroup );
  }

  return pStObjGroup;
}

static VOID __stObjListAdd(PSTOBJ pStObj)
{
  if ( cStObj == ulMaxStObj )
  {
    ULONG	ulMaxNew = ulMaxStObj + 64;
    PSTOBJ	*papStObjNew = debugReAlloc( papStObj,
                                             sizeof(PSTOBJ) * ulMaxNew );
    if ( papStObjNew == NULL )
      return;

    papStObj = papStObjNew;
    ulMaxStObj = ulMaxNew;
  }

  papStObj[cStObj++] = pStObj;
}


static int _sortComp(const void *p1, const void *p2)
{
  PSTOBJITEM	pItem1 = *(PSTOBJITEM *)p1;
  PSTOBJITEM	pItem2 = *(PSTOBJITEM *)p2;
  int		iRes = 0;
  ULONG		ulVal1, ulVal2;

  switch( ulSortBy )
  {
    case 1:
      if ( pItem1->stObj.ullSent > pItem2->stObj.ullSent )
        iRes = 1;
      else if ( pItem1->stObj.ullSent < pItem2->stObj.ullSent )
        iRes = -1;
      break;

    case 2:
      if ( pgrGetValue( &pItem1->pStObjGroup->stGraph, &pItem1->stObj.stGrValSent,
                        &ulVal1 )
         &&
           pgrGetValue( &pItem2->pStObjGroup->stGraph, &pItem2->stObj.stGrValSent,
                        &ulVal2 ) )
      {
        iRes = ulVal1 - ulVal2;
      }
      break;

    case 3:
      if ( pItem1->stObj.ullRecv > pItem2->stObj.ullRecv )
        iRes = 1;
      else if ( pItem1->stObj.ullRecv < pItem2->stObj.ullRecv )
        iRes = -1;
      break;

    case 4:
      if ( pgrGetValue( &pItem1->pStObjGroup->stGraph, &pItem1->stObj.stGrValRecv,
                        &ulVal1 )
         &&
           pgrGetValue( &pItem2->pStObjGroup->stGraph, &pItem2->stObj.stGrValRecv,
                        &ulVal2 ) )
      {
        iRes = ulVal1 - ulVal2;
      }
      break;
  }

  if ( iRes == 0 )
  {
    switch( WinCompareStrings( NULLHANDLE, 0, 0, pItem1->_stObj_pszName,
                               pItem2->_stObj_pszName, 0 ) )
    {
      case WCS_LT:
        iRes = -1;
        break;
      case WCS_GT:
        iRes = 1;
        break;
      default: // WCS_EQ:
        iRes = 0;
    }
  }

  return fSortDesc ? -iRes : iRes;
}

// VOID _listStatObj()
//
// List all Groups/Items objects maked by _makeStatObj() in array papStObj[].

static VOID _listStatObj()
{
  PSTOBJGROUP	pStObjGroup;
  PSTOBJITEM	pStObjItem;
  ULONG		ulFirst;

  cStObj = 0;

  // List all objects STOBJ* in array.
  for( pStObjGroup = (PSTOBJGROUP)lnkseqGetFirst( &lsStObjGroups );
       pStObjGroup != NULL;
       pStObjGroup = (PSTOBJGROUP)lnkseqGetNext( pStObjGroup ) )
  {
    if ( pStObjGroup->_stObj_cbName != 0 )
      __stObjListAdd( (PSTOBJ)pStObjGroup );

    if ( pStObjGroup->fExpanded )
    {
      ulFirst = cStObj;

      for( pStObjItem = (PSTOBJITEM)lnkseqGetFirst( &pStObjGroup->lsItems );
           pStObjItem != NULL;
           pStObjItem = (PSTOBJITEM)lnkseqGetNext( pStObjItem ) )
      {
        __stObjListAdd( (PSTOBJ)pStObjItem );
      }

      // Sort items in group.
      qsort( &papStObj[ulFirst], cStObj - ulFirst, sizeof(PSTOBJ), _sortComp );
    }
  }
}

// VOID _makeStatObj()
//
// Build Groups/Items objects from iptrace's snapshot for filter nodes. Then
// calls _listStatObj() to list all objects in array papStObj[].

static VOID _makeStatObj()
{
  ULONG		ulRC;
  LONG		cbActual;
  PIPTSNAPSHOT	pSSScan;
  PCHAR		pcItem, pcItemEnd;
  PCHAR		pcGroup;
  ULONG		cbItem, cbGroup;
  PSTOBJGROUP	pStObjGroup, pStObjGroupNext;
  PSTOBJITEM	*ppStObjItem;
  PSTOBJITEM	pStObjItem, pStObjItemNext;
  static ULONG	ulMakeStamp = 0;

  ulRC = iptSnapshot( pSnapshot, cbSnapshot, (PULONG)&cbActual );
  if ( ulRC == IPT_OVERFLOW )
  {
    if ( pSnapshot != NULL )
      debugFree( pSnapshot );
    pSnapshot = debugMAlloc( cbActual );
    if ( pSnapshot == NULL )
    {
      debug( "Not enough memory" );
      return;
    }
    cbSnapshot = cbActual;

    ulRC = iptSnapshot( pSnapshot, cbSnapshot, (PULONG)&cbActual );
  }

  if ( ulRC != IPT_OK )
  {
    debug( "Cannot get snapshot. iptSnapshot(), rc = %u", ulRC );
    return;
  }

  // Scan snapshot records.

  ulMakeStamp++;
  for( pSSScan = pSnapshot; cbActual > 0;
       pSSScan++, cbActual -= sizeof(IPTSNAPSHOT) )
  {
    // Parse node name: "group:item".

    pcItem = strchr( pSSScan->pNode->pszName, ':' );
    pcItemEnd = strchr( pSSScan->pNode->pszName, '\0' );
    if ( pcItem == NULL )
    {
      pcItem = pSSScan->pNode->pszName;
      pcGroup = NULL;
      cbGroup = 0;
    }
    else
    {
      PCHAR	pcGroupEnd = pcItem;

      pcGroup = pSSScan->pNode->pszName;
      _STR_skipSpaces( pcGroup );
      _STR_skipRSpaces( pcGroup, pcGroupEnd );
      cbGroup = pcGroupEnd - pcGroup;

      pcItem++;
    }

    _STR_skipSpaces( pcItem );
    _STR_skipRSpaces( pcItem, pcItemEnd );
    cbItem = pcItemEnd - pcItem;
    if ( cbItem == 0 )
    {
      pSSScan->pNode->pUser = NULL;
      continue;
    }
    // pcGroup, cbGroup - group name
    // pcItem, cbItem - item name

    pStObjGroup = _stObjGroupFind( pcGroup, cbGroup );
    ppStObjItem = (PSTOBJITEM *)(&pSSScan->pNode->pUser);

    if ( ( *ppStObjItem != NULL ) &&
         ( (*ppStObjItem)->pStObjGroup != pStObjGroup ) )
    {
      // Group for item changed - destroy item object.
      lnkseqRemove( &(*ppStObjItem)->pStObjGroup->lsItems, *ppStObjItem );
      _stObjItemFree( *ppStObjItem );
      *ppStObjItem = NULL;
    }

    // Create group if it not exists.
    if ( pStObjGroup == NULL )
    {
      pStObjGroup = _stObjGroupNew( pcGroup, cbGroup );
      if ( pStObjGroup == NULL )
        continue;
    }
    pStObjGroup->stObj.ulMakeStamp = ulMakeStamp;

    // Crete item object STOBJITEM for node. Add it to the group.
    if ( *ppStObjItem == NULL )
    {
      *ppStObjItem = _stObjItemNew( pStObjGroup, cbItem, pcItem );
      if ( *ppStObjItem == NULL )
        continue;
      (*ppStObjItem)->stObj.ullSent = pSSScan->ullSent;
      (*ppStObjItem)->stObj.ullRecv = pSSScan->ullRecv;
      lnkseqAdd( &pStObjGroup->lsItems, *ppStObjItem );
    }
    else
      _stObjUpdateStr( &(*ppStObjItem)->stObj.stName, cbItem, pcItem );

    _stObjUpdateStr( &(*ppStObjItem)->stComment,
                     _STR_strlen( pSSScan->pNode->pszComment ),
                     pSSScan->pNode->pszComment );
    (*ppStObjItem)->stObj.ulMakeStamp = ulMakeStamp;
  }

  // Remove and destroy not used groups and items.
  pStObjGroup = (PSTOBJGROUP)lnkseqGetFirst( &lsStObjGroups );
  while( pStObjGroup != NULL )
  {
    pStObjGroupNext = (PSTOBJGROUP)lnkseqGetNext( pStObjGroup );

    if ( pStObjGroup->stObj.ulMakeStamp != ulMakeStamp )
    {
      lnkseqRemove( &lsStObjGroups, pStObjGroup );
      _stObjGroupFree( pStObjGroup );
    }
    else
    {
      pStObjItem = (PSTOBJITEM)lnkseqGetFirst( &pStObjGroup->lsItems );
      while( pStObjItem != NULL )
      {
        pStObjItemNext = (PSTOBJITEM)lnkseqGetNext( pStObjItem );

        if ( pStObjItem->stObj.ulMakeStamp != ulMakeStamp )
        {
          lnkseqRemove( &pStObjGroup->lsItems, pStObjItem );
          _stObjItemFree( pStObjItem );
        }

        pStObjItem = pStObjItemNext;
      }
    }

    pStObjGroup = pStObjGroupNext;
  }

  _listStatObj();
}

static VOID _expandGroups(BOOL fExpand)
{
  PSTOBJGROUP	pStObjGroup = (PSTOBJGROUP)lnkseqGetFirst( &lsStObjGroups );

  pupdateLock();
  while( pStObjGroup != NULL )
  {
    pStObjGroup->fExpanded = fExpand;
    pStObjGroup = (PSTOBJGROUP)lnkseqGetNext( pStObjGroup );
  }

  _listStatObj();
  pupdateUnlock();
}

// grValToStr() callback fn. for pgrDraw(). This function should
// return the string at pcBuf for ordinate minimum and maximum captions.
static LONG grValToStr(ULONG ulVal, PCHAR pcBuf, ULONG cbBuf)
{
  return pstrFromBytes( ulVal, cbBuf, pcBuf, TRUE );
}

static VOID _drawLed(HPS hps, POINTL pt, LONG lColor)
{
  RECTL		rect;
  HRGN		hrgn;
  LONG		lSaveColor;
  SIZEL		size = { 1, 1 };

  rect.xLeft   = pt.x;
  rect.yBottom = pt.y + ( psizeEm->cy >> 2 );
  rect.xRight  = rect.xLeft + (ulItemDataHOffs >> 1);
  rect.yTop    = rect.yBottom + (psizeEm->cy >> 1);
  hrgn = GpiCreateRegion( hps, 1, &rect );

  lSaveColor = GpiQueryColor( hps );
  GpiSetColor( hps, lColor );
  GpiPaintRegion( hps, hrgn );
  GpiSetColor( hps, ( lColor >> 1 ) & 0x007F7F7F );
  GpiFrameRegion( hps, hrgn, &size );
  GpiSetColor( hps, lSaveColor );
  GpiDestroyRegion( hps, hrgn );

  pt.x += ulItemDataHOffs;
  GpiMove( hps, &pt );
}


// Interface routines of data source
// ---------------------------------


DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  PHutilGetEmSize	putilGetEmSize;

  debugInit();

  // Store module handle of data source.
  hDSModule = hMod;
  // Store anchor block handle.
  hab = pSLInfo->hab;

  // Query pointers to helper routines.

  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  pctrlDlgCenterOwner = (PHctrlDlgCenterOwner)pSLInfo->slQueryHelper( "ctrlDlgCenterOwner" );
  pctrlQueryText = (PHctrlQueryText)pSLInfo->slQueryHelper( "ctrlQueryText" );
  pgrInit = (PHgrInit)pSLInfo->slQueryHelper( "grInit" );
  pgrDone = (PHgrDone)pSLInfo->slQueryHelper( "grDone" );
  pgrSetTimeScale = (PHgrSetTimeScale)pSLInfo->slQueryHelper( "grSetTimeScale" );
  pgrNewTimestamp = (PHgrNewTimestamp)pSLInfo->slQueryHelper( "grNewTimestamp" );
  pgrGetTimestamp = (PHgrGetTimestamp)pSLInfo->slQueryHelper( "grGetTimestamp" );
  pgrInitVal = (PHgrInitVal)pSLInfo->slQueryHelper( "grInitVal" );
  pgrDoneVal = (PHgrDoneVal)pSLInfo->slQueryHelper( "grDoneVal" );
  pgrSetValue = (PHgrSetValue)pSLInfo->slQueryHelper( "grSetValue" );
  pgrGetValue = (PHgrGetValue)pSLInfo->slQueryHelper( "grGetValue" );
  pgrDraw = (PHgrDraw)pSLInfo->slQueryHelper( "grDraw" );
  putilGetTextSize = (PHutilGetTextSize)pSLInfo->slQueryHelper( "utilGetTextSize" );
  putilBox = (PHutilBox)pSLInfo->slQueryHelper( "utilBox" );
  putilMixRGB = (PHutilMixRGB)pSLInfo->slQueryHelper( "utilMixRGB" );
  putilGetColor = (PHutilGetColor)pSLInfo->slQueryHelper( "utilGetColor" );
  putilWriteResStr = (PHutilWriteResStr)pSLInfo->slQueryHelper( "utilWriteResStr" );
  putilLoadInsertStr = (PHutilLoadInsertStr)pSLInfo->slQueryHelper( "utilLoadInsertStr" );
  putilQueryProgPath = (PHutilQueryProgPath)pSLInfo->slQueryHelper( "utilQueryProgPath" );
  putil3DFrame = (PHutil3DFrame)pSLInfo->slQueryHelper( "util3DFrame" );
  pupdateLock = (PHupdateLock)pSLInfo->slQueryHelper( "updateLock" );
  pupdateUnlock = (PHupdateUnlock)pSLInfo->slQueryHelper( "updateUnlock" );
  pupdateForce = (PHupdateForce)pSLInfo->slQueryHelper( "updateForce" );
  pstrFromSec = (PHstrFromSec)pSLInfo->slQueryHelper( "strFromSec" );
  pstrFromBytes = (PHstrFromBytes)pSLInfo->slQueryHelper( "strFromBytes" );
  pstrLoad = (PHstrLoad)pSLInfo->slQueryHelper( "strLoad" );
  pstrFromULL = (PHstrFromULL)pSLInfo->slQueryHelper( "strFromULL" );
  pctrlStaticToColorRect = (PHctrlStaticToColorRect)pSLInfo->slQueryHelper( "ctrlStaticToColorRect" );
  phelpShow = (PHhelpShow)pSLInfo->slQueryHelper( "helpShow" );

  putilGetEmSize = (PHutilGetEmSize)pSLInfo->slQueryHelper( "utilGetEmSize" );
  psizeEm = putilGetEmSize( hMod );

  return &stDSInfo;
}

DSEXPORT VOID APIENTRY dsUninstall()
{
  debugDone();
}

DSEXPORT BOOL APIENTRY dsInit()
{
  CHAR			szFile[CCHMAXPATH - 12];
  ULONG			cbPath;
  BITMAPINFOHEADER	stBmInfoHdr;

  pSnapshot = NULL;
  cbSnapshot = 0;
  lnkseqInit( &lsStObjGroups );
  papStObj = NULL;
  cStObj = 0;
  ulMaxStObj = 0;
  ulSelectedItem = 0;

  if ( !iptInit() )
    return FALSE;

  cbPath = putilQueryProgPath( sizeof(szFile), &szFile );
  strcpy( &szFile[cbPath], "traffic.dat" );

  if ( !iptLoadFile( &szFile ) )
  {
    debug( "Cannot load filter from %s", &szFile );

    strcpy( &szFile[cbPath], "traffic.bak" );
    if ( !iptLoadFile( &szFile ) )
    {
      debug( "Cannot load filter from %s", &szFile );
      deffltCreate();
    }
    else
      debug( "Filter loaded from %s", &szFile );
  }

  hbmPlus = WinGetSysBitmap( HWND_DESKTOP, SBMP_TREEPLUS );
  hbmMinus = WinGetSysBitmap( HWND_DESKTOP, SBMP_TREEMINUS );

  stBmInfoHdr.cbFix = sizeof( BITMAPINFOHEADER );
  GpiQueryBitmapParameters( hbmMinus, &stBmInfoHdr );
  sizeBMExpand.cx = stBmInfoHdr.cx;
  sizeBMExpand.cy = stBmInfoHdr.cy;
  ulItemLPad = stBmInfoHdr.cx << 1;

  ulSortBy = piniReadULong( hDSModule, "SortBy", 0 );
  fSortDesc = (BOOL)piniReadULong( hDSModule, "SortDesc", 0 );
  fCompactItems = (BOOL)piniReadULong( hDSModule, "Compact", FALSE );
  ulGroupRates = piniReadULong( hDSModule, "GroupRates", DEF_GROUPRATES );

  ulUpdateInterval = piniReadULong( hDSModule, "UpdateInterval", DEF_INTERVAL );
  if ( ulUpdateInterval < 500 || ulUpdateInterval > 30000 ||
       (ulUpdateInterval % 100) != 0 )
    ulUpdateInterval = DEF_INTERVAL;

  ulTimeWindow = piniReadULong( hDSModule, "TimeWindow", DEF_TIMEWIN );
  if ( ulTimeWindow < 60 || ulTimeWindow > (60 * 60) )
    ulUpdateInterval = DEF_TIMEWIN;

  stGrParam.clrBackground = piniReadULong( hDSModule, "GrBgColor",
                                           DEF_GRBGCOLOR );
  stGrParam.clrBorder = piniReadULong( hDSModule, "GrBorderColor",
                                       DEF_GRBORDERCOLOR );
  stGrParam.pParamVal[0].clrGraph = piniReadULong( hDSModule, "TXColor",
                                                   DEF_TXCOLOR );
  stGrParam.pParamVal[1].clrGraph = piniReadULong( hDSModule, "RXColor",
                                                   DEF_RXCOLOR );

  _makeStatObj();

  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulLastStoreTime, sizeof(ULONG) );

  return TRUE;
}

DSEXPORT VOID APIENTRY dsDone()
{
  if ( WinDestroyWindow != NULL )
  {
    WinDestroyWindow( hwndTrace );
    hwndTrace = NULLHANDLE;
  }

  iptDone();

  if ( pSnapshot != NULL )
    debugFree( pSnapshot );

  lnkseqFree( &lsStObjGroups, PSTOBJGROUP, _stObjGroupFree );

  if ( papStObj != NULL )
    debugFree( papStObj );
}

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return ulUpdateInterval;
}

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  ULONG		cbActual;
  ULONG		ulRC = iptSnapshot( pSnapshot, cbSnapshot, &cbActual );
  PIPTSNAPSHOT	pScan;
  PSTOBJITEM	pStObjItem;
  PSTOBJGROUP	pStObjGroup;
  ULONG		ulVal;
  ULONG		ulTimeDelta;
  ULONG		ulSendSpeed;
  ULONG		ulRecvSpeed;
  static ULONG	ulTimeLast = 0;

  if ( ulRC != IPT_OK )
  {
    debug( "iptSnapshot(), rc = %u", ulRC );
    return DS_UPD_NONE;
  }

  // Set new timestamps and reset sum and maximum values for groups of items.
  for( pStObjGroup = (PSTOBJGROUP)lnkseqGetFirst( &lsStObjGroups );
       pStObjGroup != NULL;
       pStObjGroup = (PSTOBJGROUP)lnkseqGetNext( pStObjGroup ) )
  {
    pgrNewTimestamp( &pStObjGroup->stGraph, ulTime );
    pStObjGroup->ullNewSumSent = 0;
    pStObjGroup->ullNewSumRecv = 0;
    pStObjGroup->ulMaxSendSpeed = 0;
    pStObjGroup->ulMaxRecvSpeed = 0;
  }

  ulTimeDelta = ulTime - ulTimeLast;
  for( pScan = pSnapshot; cbActual >= sizeof(IPTSNAPSHOT);
       cbActual -= sizeof(IPTSNAPSHOT), pScan++ )
  {
    pStObjItem = (PSTOBJITEM)pScan->pNode->pUser;
    if ( pStObjItem == NULL )
      continue;
    pStObjGroup = pStObjItem->pStObjGroup;

    if ( pgrGetValue( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValSent, &ulVal ) )
    {
      ulSendSpeed = ((pScan->ullSent - pStObjItem->stObj.ullSent) * 1000) /
                    ulTimeDelta;
      ulRecvSpeed = ((pScan->ullRecv - pStObjItem->stObj.ullRecv) * 1000) /
                    ulTimeDelta;

      if ( pStObjGroup->ulMaxSendSpeed < ulSendSpeed )
        pStObjGroup->ulMaxSendSpeed = ulSendSpeed;
      if ( pStObjGroup->ulMaxRecvSpeed < ulRecvSpeed )
        pStObjGroup->ulMaxRecvSpeed = ulRecvSpeed;

      pgrSetValue( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValSent,
                   ulSendSpeed );
      pgrSetValue( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValRecv,
                   ulRecvSpeed );
    }
    else
    {
      pgrSetValue( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValSent, 0 );
      pgrSetValue( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValRecv, 0 );
    }

    pStObjItem->stObj.ullSent = pScan->ullSent;
    pStObjItem->stObj.ullRecv = pScan->ullRecv;

    pStObjGroup->ullNewSumSent += pScan->ullSent;
    pStObjGroup->ullNewSumRecv += pScan->ullRecv;
  }

  for( pStObjGroup = (PSTOBJGROUP)lnkseqGetFirst( &lsStObjGroups );
       pStObjGroup != NULL;
       pStObjGroup = (PSTOBJGROUP)lnkseqGetNext( pStObjGroup ) )
  {
    if ( pgrGetValue( &pStObjGroup->stGraph, &pStObjGroup->stObj.stGrValSent, &ulVal ) )
    {
      if ( ulGroupRates == GROUPRATE_MAX )
      {
        ulSendSpeed = pStObjGroup->ulMaxSendSpeed;
        ulRecvSpeed = pStObjGroup->ulMaxRecvSpeed;
      }
      else
      {
        ulSendSpeed = ((pStObjGroup->ullNewSumSent - pStObjGroup->stObj.ullSent)
                       * 1000) / ulTimeDelta;
        ulRecvSpeed = ((pStObjGroup->ullNewSumRecv - pStObjGroup->stObj.ullRecv)
                       * 1000) / ulTimeDelta;

        if ( ulGroupRates == GROUPRATE_AVERAGE )
        {
          ulSendSpeed /= lnkseqGetCount( &pStObjGroup->lsItems );
          ulRecvSpeed /= lnkseqGetCount( &pStObjGroup->lsItems );
        } // else - GRSPD_SUM
      }

      pgrSetValue( &pStObjGroup->stGraph, &pStObjGroup->stObj.stGrValSent,
                   ulSendSpeed );
      pgrSetValue( &pStObjGroup->stGraph, &pStObjGroup->stObj.stGrValRecv,
                   ulRecvSpeed );
    }
    else
    {
      pgrSetValue( &pStObjGroup->stGraph, &pStObjGroup->stObj.stGrValSent, 0 );
      pgrSetValue( &pStObjGroup->stGraph, &pStObjGroup->stObj.stGrValRecv, 0 );
    }

    pStObjGroup->stObj.ullSent = pStObjGroup->ullNewSumSent;
    pStObjGroup->stObj.ullRecv = pStObjGroup->ullNewSumRecv;
  }

  ulTimeLast = ulTime;

  // Call _listStatObj() to sort items in groups.
  _listStatObj();

  // Periodic saving filter. We do not have to completely lose the statistics
  // when the program or system fails: power failure, system hangs,
  // SystemLoad process kill, e.t.c.
  if ( ( ulTime - ulLastStoreTime ) >= AUTOSAVE_PERIOD )
  {
    _filterStore();
    ulLastStoreTime = ulTime;
  }

  return DS_UPD_DATA;
}

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return cStObj;
}

DSEXPORT ULONG APIENTRY dsSortBy(ULONG ulNewSort)
{
  ULONG		ulResult;

  if ( ulNewSort != DSSORT_QUERY )
  {
    ulSortBy = ulNewSort & DSSORT_VALUE_MASK;
    fSortDesc = (ulNewSort & DSSORT_FL_DESCENDING) != 0;
    // Call _listStatObj() to sort items in groups.
    _listStatObj();
    // Store new sort type to the ini-file
    piniWriteULong( hDSModule, "SortBy", ulSortBy );
    piniWriteULong( hDSModule, "SortDesc", fSortDesc );
  }

  ulResult = ulSortBy;
  if ( fSortDesc )
    ulResult |= DSSORT_FL_DESCENDING;

  return ulResult;
}

DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex)
{
  ulSelectedItem = ulIndex;
  return TRUE;
}

DSEXPORT ULONG APIENTRY dsGetSel()
{
  return ulSelectedItem;
}

BOOL APIENTRY dsIsSelected(ULONG ulIndex)
{
  return ulIndex == ulSelectedItem;
}

// SystemLoad calls dsGetHintText() to query text for hint-window.
// We will show text "TX: xxxx, RX: xxxx" only for items in compact view.
DSEXPORT VOID APIENTRY dsGetHintText(ULONG ulIndex, PCHAR pcBuf, ULONG cbBuf)
{
  ULONG		cBytes;
  PSTOBJ	pStObj = papStObj[ulIndex];

  if ( !fCompactItems || ( pStObj->ulType == STO_GROUP ) ||
       ( cbBuf <= ( 2 * sizeof(ULONG) ) ) )
    return;

  *((PULONG)pcBuf) = (ULONG)' :XT';
  pcBuf += sizeof(ULONG);
  cbBuf -= sizeof(ULONG);
  cBytes = pstrFromBytes( pStObj->ullSent, cbBuf, pcBuf, FALSE );
  pcBuf += cBytes;
  cbBuf -= cBytes;
  if ( cbBuf <= ( 2 * sizeof(ULONG) ) )
    return;
  *((PULONG)pcBuf) = (ULONG)'\0\0 ,';
  pcBuf += 2;
  cbBuf -= 2;
  *((PULONG)pcBuf) = (ULONG)' :XR';
  pcBuf += sizeof(ULONG);
  cbBuf -= sizeof(ULONG);
  pstrFromBytes( pStObj->ullRecv, cbBuf, pcBuf, FALSE );
}


DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  SIZEL		sizeText;
  LONG		lBackColor = GpiQueryBackColor( hps );

  stGrParam.clrBackground = GpiQueryRGBColor( hps, 0, SYSCLR_WINDOW );

  // Color may vary. Is why we take the color here, before each complete
  // redraw.
  lSelItemFgColor = putilGetColor( hDSModule, DS_COLOR_ITEMHIFORE );
  lSelItemBgColor = putilGetColor( hDSModule, DS_COLOR_ITEMHIBACK );
  lItemFgColor =
    GpiQueryRGBColor( hps, 0,  putilGetColor( hDSModule, DS_COLOR_ITEMFORE ) );

  lLedSendOn = aGrParamVal[0].clrGraph;
  lLedSendOff = putilMixRGB( lLedSendOn, lBackColor, 192 );
  lLedRecvOn = aGrParamVal[1].clrGraph;
  lLedRecvOff = putilMixRGB( lLedRecvOn, lBackColor, 192 );

  GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &stFontMetrics );
  ulLineHeight = stFontMetrics.lMaxAscender + stFontMetrics.lMaxDescender;

  putilGetTextSize( hps, 8,  "RX  9999", &sizeText );
  ulXOffsCounterSp = ulItemDataHOffs + sizeText.cx;

  putilGetTextSize( hps, 20, "RX  9999 Bytes  9999", &sizeText );
  ulXOffsSpeedSp = ulItemDataHOffs + sizeText.cx;

  putilGetTextSize( hps, 31, "RX  9999 Bytes  9999 Bytes/sec.", &sizeText );
  ulDataStrWidth = ulItemDataHOffs + sizeText.cx;
}

DSEXPORT VOID APIENTRY dsSetWnd(ULONG ulIndex, HWND hwnd, HPS hps)
{
  PSTOBJ	pStObj = papStObj[ulIndex];
  SIZEL		sizeText;

  putilGetTextSize( hps, pStObj->stName.cbStr, pStObj->stName.pszStr, &sizeText );
  sizeText.cx += psizeEm->cx >> 1;

  if ( pStObj->ulType == STO_GROUP )
  {
    sizeText.cx += sizeBMExpand.cx + (sizeBMExpand.cx >> 2);
    sizeText.cy = ulLineHeight + (ulLineHeight >> 2);

    if ( sizeText.cy < sizeBMExpand.cy )
      sizeText.cy = sizeBMExpand.cy;
    WinSetWindowPos( hwnd, HWND_TOP, 0, 0, sizeText.cx, sizeText.cy, SWP_SIZE );
    return;
  }

  if ( fCompactItems )
    sizeText.cy = ulLineHeight + (ulLineHeight >> 2);
  else
  {
    if ( sizeText.cx < ulDataStrWidth )
      sizeText.cx = ulDataStrWidth;
    sizeText.cy = (ulLineHeight << 2) - (ulLineHeight >> 1);
  }

  if ( ((PSTOBJITEM)pStObj)->pStObjGroup->_stObj_cbName != 0 )
    sizeText.cx += ulItemLPad;

  WinSetWindowPos( hwnd, HWND_TOP, 0, 0, sizeText.cx, sizeText.cy, SWP_SIZE );
}

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  PSTOBJ	pStObj = papStObj[ulIndex];
  POINTL	pt;
  CHAR		acBuf[64];
  LONG		cbBuf;
  SIZEL		sizeText;
  PSTOBJITEM	pStObjItem;
  PSTOBJGROUP	pStObjGroup;
  ULONG		ulVal;
  ULONG		ulXData;
  ULONG		ulXCounterSp;
  ULONG		ulXSpeedSp;
  RECTL		rect;
  LONG		lBackColor = GpiQueryBackColor( hps );

  if ( pStObj->stName.pszStr == NULL )
    return;

  GpiSetColor( hps, lItemFgColor );

  if ( pStObj->ulType == STO_GROUP )
  {
    pt.x = 0;
    if ( sizeBMExpand.cy < ulLineHeight )
      pt.y = ( ulLineHeight - sizeBMExpand.cy ) >> 1;
    else
      pt.y = 0;
    WinDrawBitmap( hps, ((PSTOBJGROUP)pStObj)->fExpanded ? hbmMinus : hbmPlus,
                   NULL, &pt, 0, 0, DBM_IMAGEATTRS );
    pt.x = sizeBMExpand.cx + (sizeBMExpand.cx >> 2);

    if ( ulIndex == ulSelectedItem )
    {
      rect.xLeft = pt.x;
      rect.yBottom = 0;
      rect.xRight = psizeWnd->cx;
      rect.yTop = ulLineHeight;
      putilBox( hps, &rect, lSelItemBgColor );
      GpiSetColor( hps, lSelItemFgColor );
      GpiSetBackColor( hps, lSelItemBgColor );
    }

    pt.x += psizeEm->cx >> 2;
    pt.y = ( ulLineHeight - (stFontMetrics.lMaxAscender - stFontMetrics.lInternalLeading) ) / 2;
    GpiCharStringAt( hps, &pt, pStObj->stName.cbStr, pStObj->stName.pszStr );
    return;
  }

  pStObjItem = (PSTOBJITEM)pStObj;
  pStObjGroup = pStObjItem->pStObjGroup;

  if ( pStObjGroup->_stObj_cbName == 0 )
    pt.x = 0;
  else
  {
    pt.x = sizeBMExpand.cx >> 1;
    pt.y = psizeWnd->cy;
    GpiMove( hps, &pt );
    pt.y -= (ulLineHeight >> 2) + (ulLineHeight >> 1);
    GpiLine( hps, &pt );
    pt.x = ulItemLPad;
    GpiLine( hps, &pt );

    if ( ( ulIndex < (cStObj - 1) ) &&
         ( ((PSTOBJ)papStObj[ulIndex + 1])->ulType != STO_GROUP ) )
    {
      // Not last item in group.
      pt.x = sizeBMExpand.cx >> 1;
      GpiMove( hps, &pt );
      pt.y = 0;
      GpiLine( hps, &pt );
    }

    pt.x = ulItemLPad;
  }

  pt.y = psizeWnd->cy - ulLineHeight - (ulLineHeight >> 2);

  putilGetTextSize( hps, pStObj->stName.cbStr, pStObj->stName.pszStr, &sizeText );
  rect.xLeft = pt.x;
  rect.yBottom = pt.y;
  rect.xRight = rect.xLeft + sizeText.cx + (psizeEm->cx >> 1);
  rect.yTop = rect.yBottom + ulLineHeight;

  if ( ulIndex == ulSelectedItem )
  {
    GpiSetColor( hps, lSelItemFgColor );
    GpiSetBackColor( hps, lSelItemBgColor );
  }

  pt.x += psizeEm->cx >> 2;
  pt.y += ( ulLineHeight -
            (stFontMetrics.lMaxAscender - stFontMetrics.lInternalLeading) ) / 2;
  GpiCharStringPosAt( hps, &pt, &rect, CHS_CLIP | CHS_OPAQUE,
                      pStObj->stName.cbStr, pStObj->stName.pszStr, NULL );

  GpiSetBackColor( hps, lBackColor );

  if ( fCompactItems )
    return;

  GpiSetColor( hps, putilMixRGB( lItemFgColor, lBackColor, 65 ) );

  pt.y -= ulLineHeight;

  ulXData = pt.x;
  ulXCounterSp = pt.x + ulXOffsCounterSp;
  ulXSpeedSp = pt.x + ulXOffsSpeedSp;

  if ( !pgrGetValue( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValSent, &ulVal ) )
    ulVal = 0;

  _drawLed( hps, pt, ulVal > 0 ? lLedSendOn : lLedSendOff );
  GpiCharString( hps, 2, "TX" );

  cbBuf = pstrFromBytes( pStObjItem->stObj.ullSent, sizeof(acBuf), &acBuf, FALSE );
  if ( cbBuf > 0 )
  {
    putilGetTextSize( hps, strchr( &acBuf, ' ' ) - &acBuf, &acBuf, &sizeText );
    pt.x = ulXCounterSp - sizeText.cx;
    GpiCharStringAt( hps, &pt, cbBuf, &acBuf );
  }

  cbBuf = pstrFromBytes( ulVal, sizeof(acBuf), &acBuf, TRUE );
  if ( cbBuf > 0 )
  {
    putilGetTextSize( hps, strchr( &acBuf, ' ' ) - &acBuf, &acBuf, &sizeText );
    pt.x = ulXSpeedSp - sizeText.cx;
    GpiCharStringAt( hps, &pt, cbBuf, &acBuf );
  }

  pt.y -= ulLineHeight;

  if ( !pgrGetValue( &pStObjGroup->stGraph, &pStObjItem->stObj.stGrValRecv, &ulVal ) )
    ulVal = 0;

  pt.x = ulXData;
  _drawLed( hps, pt, ulVal > 0 ? lLedRecvOn : lLedRecvOff );
  GpiCharString( hps, 2, "RX" );

  cbBuf = pstrFromBytes( pStObjItem->stObj.ullRecv, sizeof(acBuf), &acBuf, FALSE );
  if ( cbBuf > 0 )
  {
    putilGetTextSize( hps, strchr( &acBuf, ' ' ) - &acBuf, &acBuf, &sizeText );
    pt.x = ulXCounterSp - sizeText.cx;
    GpiCharStringAt( hps, &pt, cbBuf, &acBuf );
  }

  cbBuf = pstrFromBytes( ulVal, sizeof(acBuf), &acBuf, TRUE );
  if ( cbBuf > 0 )
  {
    putilGetTextSize( hps, strchr( &acBuf, ' ' ) - &acBuf, &acBuf, &sizeText );
    pt.x = ulXSpeedSp - sizeText.cx;
    GpiCharStringAt( hps, &pt, cbBuf, &acBuf );
  }
}

DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  POINTL		pt, ptCur;
  PSTOBJ		pStObj;
  PSTOBJGROUP		pStObjGroup;
  RECTL			rect;
  PGRVAL		apGrVal[2];
  SIZEL			sizeEm;
  SIZEL			sizeText;
  ULONG			ulIdx;
  CHAR			acBuf[32];
  ULONG			cbBuf;
  ULONG			ulX;
  ULONG			ulVal;
  ULONG			ulLineHeight;
  FONTMETRICS		stFontMetrics;
  PCHAR			pcBuf;
  LONG			lColor = GpiQueryColor( hps );

  if ( ulSelectedItem >= cStObj )
    return;

  pStObj = papStObj[ulSelectedItem];
  pStObjGroup = pStObj->ulType == STO_ITEM ?
                  ((PSTOBJITEM)pStObj)->pStObjGroup : (PSTOBJGROUP)pStObj;


  putilGetTextSize( hps, 2, "Em", &sizeEm );
  ulLineHeight = sizeEm.cy << 1;

  // Node title "Group: Node".

  pt.x = sizeEm.cx;
  pt.y = psizeWnd->cy - ulLineHeight;
  GpiMove( hps, &pt );

  if ( pStObj->ulType == STO_ITEM )
  {
    GpiCharString( hps, pStObjGroup->stObj.stName.cbStr,
                   pStObjGroup->stObj.stName.pszStr );
    GpiCharString( hps, 2, ": " );
  }

  GpiCharString( hps, pStObj->stName.cbStr, pStObj->stName.pszStr );
  pt.y -= ulLineHeight;

  // Titles of information lines.

  ulX = 0;
  for( ulIdx = 1; ulIdx < FIELDS_NUMB; ulIdx++ )
  {
    cbBuf = pstrLoad( hDSModule, aulFields[ulIdx], sizeof(acBuf), &acBuf );
    GpiCharStringAt( hps, &pt, cbBuf, &acBuf );
    GpiCharString( hps, 1, ":" );
    GpiQueryCurrentPosition( hps, &ptCur );
    if ( ulX < ptCur.x )
      ulX = ptCur.x;

    pt.y -= ulLineHeight;
  }

  // Values of information lines.

  putilGetTextSize( hps, 25, "2 203 222 870 279 061 503", &sizeText );
  ulX += sizeText.cx;

  pt.y = psizeWnd->cy - (ulLineHeight << 1);
  GpiSetColor( hps, 0x00000000 );

  for( ulIdx = 1; ulIdx < FIELDS_NUMB; ulIdx++ )
  {
    switch( ulIdx )
    {
      case 1:		// Bytes sent value
      case 3:		// Bytes received value
        cbBuf = pstrFromULL( &acBuf, sizeof(acBuf),
                             ulIdx == 1 ? pStObj->ullSent : pStObj->ullRecv );
        putilGetTextSize( hps, cbBuf, &acBuf, &sizeText );
        pt.x = ulX - sizeText.cx;
        GpiCharStringAt( hps, &pt, cbBuf, &acBuf );

        pcBuf = &acBuf;
        cbBuf = WinLoadString( hab, 0, IDS_BYTES, sizeof(acBuf), &acBuf );
        break;

      case 2:		// Send speed value
      case 4:		// Receive speed value
        if ( pgrGetValue( &pStObjGroup->stGraph,
                          ulIdx == 2 ? &pStObj->stGrValSent :
                                       &pStObj->stGrValRecv,
                          &ulVal ) )
        {
          cbBuf = pstrFromBytes( ulVal, sizeof(acBuf), &acBuf, TRUE );
          if ( cbBuf > 0 )
          {
            pcBuf = strchr( &acBuf, ' ' );
            ulVal = pcBuf - &acBuf;
            putilGetTextSize( hps, ulVal, &acBuf, &sizeText );
            pt.x = ulX - sizeText.cx;
            GpiCharStringAt( hps, &pt, ulVal, &acBuf );

            pcBuf++;
            cbBuf -= (ulVal + 1);
          }
        }
        break;
    }

    // Measurement units.
    pt.x = ulX + ( sizeEm.cx >> 1 );
    GpiCharStringAt( hps, &pt, cbBuf, pcBuf );

    pt.y -= ulLineHeight;
  }

  // Horisontal separators.

  rect.xLeft = sizeEm.cx;
  rect.xRight = ulX + (sizeEm.cx << 2);
  rect.yBottom = psizeWnd->cy - ulLineHeight - ((ulLineHeight - sizeEm.cy) >> 1);
  rect.yTop = rect.yBottom + 2;
  putil3DFrame( hps, &rect, SYSCLR_TITLEBOTTOM, SYSCLR_WINDOW );
  rect.yBottom = pt.y + sizeEm.cy;
  rect.yTop = rect.yBottom + 2;
  putil3DFrame( hps, &rect, SYSCLR_TITLEBOTTOM, SYSCLR_WINDOW );

  // Filter node comments.

  if ( ( pStObj->ulType == STO_ITEM ) &&
       ( ((PSTOBJITEM)pStObj)->stComment.pszStr != NULL ) )
  {
    rect.xLeft = sizeEm.cx;
    rect.yTop = pt.y + sizeEm.cy;
    rect.yBottom = 0;

    pcBuf = ((PSTOBJITEM)pStObj)->stComment.pszStr;
    cbBuf = ((PSTOBJITEM)pStObj)->stComment.cbStr;
    GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &stFontMetrics );
    ulLineHeight = stFontMetrics.lMaxBaselineExt + stFontMetrics.lExternalLeading;

    while( cbBuf > 0 )
    {
      ulVal = WinDrawText( hps, cbBuf, pcBuf, &rect, 0, 0,
                           DT_LEFT | DT_TOP | DT_WORDBREAK | DT_TEXTATTRS );
      if ( ulVal == 0 )
        break;
      pcBuf += ulVal;
      cbBuf -= ulVal;
      rect.yTop -= ulLineHeight;
    }
  }

  // Graph.

  GpiSetColor( hps, lColor );

  rect.xLeft = rect.xRight + (sizeEm.cx << 3);
  rect.xRight = psizeWnd->cx - 50;
  rect.yBottom = sizeEm.cy + (sizeEm.cy << 1);
  rect.yTop = psizeWnd->cy - rect.yBottom;

  if ( pstrFromSec( ulTimeWindow, sizeof(acBuf), &acBuf ) > 0 )
    stGrParam.pszAbscissaBottomCaption = &acBuf;
  else
    stGrParam.pszAbscissaBottomCaption = NULL;

  // Fill list of data storages for graphs
  apGrVal[0] = &pStObj->stGrValSent;
  apGrVal[1] = &pStObj->stGrValRecv; 

  pgrDraw( &pStObjGroup->stGraph, hps, &rect, ARRAYSIZE( apGrVal ), &apGrVal,
           &stGrParam );
}

DSEXPORT VOID APIENTRY dsFillMenu(HWND hwndMenu, ULONG ulIndex)
{
  MENUITEM		stMINew = { 0 };
  CHAR			szBuf[256];
  ULONG			cbBuf;

  ulCmdOnItem = ulIndex;

  // Separator
  stMINew.iPosition = MIT_END;
  stMINew.afStyle = MIS_SEPARATOR;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );
  stMINew.afStyle = MIS_TEXT;

  stMINew.id = IDM_COMPACT;
  stMINew.afAttribute = fCompactItems ? MIA_CHECKED : 0;
  cbBuf = pstrLoad( hDSModule, IDS_MI_COMPACT, sizeof(szBuf), &szBuf );
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  stMINew.id = IDM_DETAILED;
  stMINew.afAttribute = fCompactItems ? 0 : MIA_CHECKED;
  cbBuf = pstrLoad( hDSModule, IDS_MI_DETAILED, sizeof(szBuf), &szBuf );
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  stMINew.iPosition = MIT_END;
  stMINew.afStyle = MIS_SEPARATOR;
  stMINew.afAttribute = 0;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );
  stMINew.afStyle = MIS_TEXT;

  stMINew.id = IDM_EXPAND_ALL;
  cbBuf = pstrLoad( hDSModule, IDS_MI_EXPAND_ALL, sizeof(szBuf), &szBuf );
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  stMINew.id = IDM_COLLAPSE_ALL;
  cbBuf = pstrLoad( hDSModule, IDS_MI_COLLAPSE_ALL, sizeof(szBuf), &szBuf );
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  stMINew.id = IDM_FILTER;
  cbBuf = pstrLoad( hDSModule, IDS_MI_FILTER, sizeof(szBuf), &szBuf );
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  if ( ulIndex == DS_SEL_NONE )
    return;

  stMINew.id = IDM_TRACE;
  cbBuf = pstrLoad( hDSModule, IDS_MI_TRACE, sizeof(szBuf), &szBuf );
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
}

DSEXPORT ULONG APIENTRY dsCommand(HWND hwndOwner, USHORT usCommand)
{
  PSTOBJ	pStObj = ulCmdOnItem < cStObj ? papStObj[ulCmdOnItem] : NULL;

  switch( usCommand )
  {
    case IDM_EXPAND_ALL:
      _expandGroups( TRUE );
      return DS_UPD_LIST;

    case IDM_COLLAPSE_ALL:
      _expandGroups( FALSE );
      return DS_UPD_LIST;

    case IDM_DETAILED:
      if ( !fCompactItems )
        return DS_UPD_NONE;
      fCompactItems = FALSE;
      break;

    case IDM_COMPACT:
      if ( fCompactItems )
        return DS_UPD_NONE;
      fCompactItems = TRUE;
      break;

    case IDM_FILTER:
      // Open Filter Editor dialog. We are not worried about the delete and
      // create records. Filter Editor works with objects SCNODE while the main
      // list displays the objects STOBJ. Objects STOBJ do not cretes and
      // destroys in Filter Editor.
      showFilter( hwndOwner, pStObj );
      _filterStore();

      pupdateLock();
      _makeStatObj();
      pupdateUnlock();
      return DS_UPD_LIST;

    case IDM_TRACE:
      if ( pStObj != NULL )
      {
        PSCNODE		pNode = NULL;
        PIPTSNAPSHOT	pSSScan;
        LONG		cbSSScan = cbSnapshot;

        pupdateLock();
        for( pSSScan = pSnapshot; cbSSScan > 0;
             pSSScan++, cbSSScan -= sizeof(IPTSNAPSHOT) )
        {
          if ( pSSScan->pNode->pUser == (PVOID)pStObj )
          {
            pNode = pSSScan->pNode;
            break;
          }
        }
        pupdateUnlock();

        traceStart( hwndOwner, pNode );
      }

    default:
      return DS_UPD_NONE;
  }

  piniWriteULong( hDSModule, "Compact", (ULONG)fCompactItems );
  return DS_UPD_LIST;
}

DSEXPORT ULONG APIENTRY dsEnter(HWND hwndOwner)
{
  PSTOBJ	pStObj;

  if ( ulSelectedItem >= cStObj )
    return DS_UPD_NONE;

  pStObj = papStObj[ulSelectedItem];
  if ( pStObj->ulType != STO_GROUP )
  {
    ulCmdOnItem = ulSelectedItem;
    return dsCommand( hwndOwner, IDM_FILTER );
  }
//    return DS_UPD_NONE;

  pupdateLock();
  ((PSTOBJGROUP)pStObj)->fExpanded = !((PSTOBJGROUP)pStObj)->fExpanded;
  _listStatObj();
  pupdateUnlock();
  return DS_UPD_LIST;
}
