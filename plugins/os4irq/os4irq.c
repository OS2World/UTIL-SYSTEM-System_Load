#define INCL_DOSMISC		/* DOS Miscellaneous values */
#define INCL_DOSERRORS		/* DOS error values */
#define INCL_WINPOINTERS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ds.h>
#include <sl.h>
#include "os4irq.h"
#include "debug.h"

APIENTRY DosSysCtl(ULONG func, PULONG p);

#define IRQ_BOX_HPAD		5
#define IRQ_BOX_VPAD		4
#define IRQ_BOX_FLD_PAD		5

#define SYS_GETIRQSTATISTIC	36

// Resource strings IDs for fields names.
static ULONG aulFldStrId[] = {
  IDS_FLD_NUM,		// 0 - FLD_NUM
  IDS_FLD_COUNTER,	// 1 - FLD_COUNTER
  IDS_FLD_LOAD,		// 2 - FLD_LOAD
  IDS_FLD_NAME,		// 3 - FLD_NAME
  IDS_FLD_LED,		// 4 - FLD_LED
  IDS_FLD_INCREMENT,	// 5 - FLD_INCREMENT
};

// Module information

PSZ			apszFields[FIELDS] = { NULL };

static DSINFO stDSInfo = {
  sizeof(DSINFO),	// Size of this data structure.
  "~IRQ (OS/4)",	// Menu item title.
  FIELDS - 2,           // Number of "sort-by" strings (without fields "Led"
                        // and "Increment").
  &apszFields,		// "Sort-by" strings.
  0,			// Flags DS_FL_*
  70,			// Items horisontal space, %Em
  50,			// Items vertical space, %Em
  0			// Help main panel index.
};


static GRVALPARAM	stGrValParam =
{
  DEF_LINECOLOR,	// clrGraph
  2,			// lLineWidth
  0			// ulPlygonBright
};

GRPARAM			stGrParam =
{
  GRPF_MIN_LABEL | GRPF_MAX_LABEL | GRPF_TIME_LABEL/* | GRPF_LEFT_TOP_CAPTION*/,
  NULL,			// pszAbscissaTopCaption
  NULL,			// pszAbscissaBottomCaption
  NULL,			// pszOrdinateCaption
  NULL,			// fnValToStr
  1,			// ulBorderCX
  1,			// ulBorderCY
  DEF_GRBORDERCOLOR,	// clrBorder
  DEF_GRBGCOLOR,	// clrBackground
  1,			// cParamVal
  &stGrValParam		// pParamVal
};

HMODULE			hDSModule;
GRAPH			stGraph;
ULONG			ulNumSelected = (ULONG)(-1);
PIRQ			*paIRQList = NULL;
static ULONG		cIRQList = 0;
static HPOINTER		hIconOn = NULLHANDLE;
static HPOINTER		hIconOff = NULLHANDLE;
static ULONG		ulIconSize;
static ULONG		ulTextHeight;
static ULONG		ulNameWidth;
static ULONG		ulNoWidth;
static ULONG		ulCounterWidth;
static ULONG		ulLoadWidth;
static ULONG		ulIconY;
static ULONG		ulTextY;
static BOOL		fItemsFixWidth;

// Default properties

IRQPROPERTIES		stIRQProperties = {
  DEF_INTERVAL,			// ulUpdateInterval (in milliseconds)
  DEF_TIMEWIN,			// ulTimeWindow (in seconds)
  FLD_NUM,			// ulSort
  FALSE,			// fSortDesc
  { DEF_ITEMFLD_0,		// aulFields
    DEF_ITEMFLD_1,
    DEF_ITEMFLD_2,
    DEF_ITEMFLD_3,
    DEF_ITEMFLD_4,
    DEF_ITEMFLD_5 },
  DEF_ITEMNAMEFIXWIDTH,		// fNameFixWidth
  { 0 }				// apszNames
};

// Pointers to helper routines.

PHiniWriteULong			piniWriteULong;
PHiniWriteStr			piniWriteStr;
PHiniWriteData			piniWriteData;
PHiniReadULong			piniReadULong;
PHiniReadStr			piniReadStr;
PHiniReadData			piniReadData;
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
PHupdateLock			pupdateLock;
PHupdateUnlock			pupdateUnlock;
PHstrFromSec			pstrFromSec;
PHstrLoad			pstrLoad;
PHctrlStaticToColorRect		pctrlStaticToColorRect;
PHctrlDlgCenterOwner		pctrlDlgCenterOwner;
PHctrlQueryText			pctrlQueryText;


static PIRQ _irqFind(ULONG ulNum)
{
  ULONG		ulIdx;
  PIRQ		*ppIRQ = paIRQList;

  for( ulIdx = 0; ulIdx < cIRQList; ulIdx++, ppIRQ++ )
  {
    if ( (*ppIRQ)->ulNum == ulNum )
      return *ppIRQ;
  }

  return NULL;
}

static int _irqStrCmp(PSZ pszS1, PSZ pszS2)
{
  PSZ		pszSf1 = pszS1 == NULL ? "" : pszS1;
  PSZ		pszSf2 = pszS2 == NULL ? "" : pszS2;
  ULONG		ulRC = WinCompareStrings( NULLHANDLE, 0, 0, pszSf1, pszSf2, 0 );

  switch( ulRC )
  {
    case WCS_EQ:
      return 0;

    case WCS_LT:
      return -1;

    case WCS_GT:
      return 1;
  }
  // ulRC == WCS_ERROR - use stricmp
  debug( "Warning! WinCompareStrings() fail" );

  return stricmp( pszSf1, pszSf2 );
}

static int _irqSortComp(const void *p1, const void *p2)
{
  PIRQ		pIRQ1 = *(PIRQ *)p1;
  PIRQ		pIRQ2 = *(PIRQ *)p2;
  int		iRes;
  ULONG		ulLoad1, ulLoad2;

  switch( stIRQProperties.ulSort )
  {
    case FLD_COUNTER:
      iRes = pIRQ1->ulCounter > pIRQ2->ulCounter ? 1 : -1;
      break;

    case FLD_LOAD:
      if ( pgrGetValue( &stGraph, &pIRQ1->stGrVal, &ulLoad1 ) &&
           pgrGetValue( &stGraph, &pIRQ2->stGrVal, &ulLoad2 ) )
      {
        iRes = ulLoad1 - ulLoad2;
        break;
      }

    case FLD_NUM:
      iRes = pIRQ1->ulNum - pIRQ2->ulNum;
      break;

    default:	// FLD_NAME
      iRes = _irqStrCmp( stIRQProperties.apszNames[ pIRQ1->ulNum ],
                         stIRQProperties.apszNames[ pIRQ2->ulNum ] );
  }

  return stIRQProperties.fSortDesc ? -iRes : iRes;
}

static BOOL _irqGetIRQStat(ULONG ulTime, BOOL fInitial)
{
  ULONG			ulRC;
  ULONG			ulIdx;
  PIRQ			pIRQ;
  BOOL			fInterval;
  ULONG			ulInterval;
  ULONG			aulValues[IRQ_MAX] = { 0 };

  // Request system IRQ counters
  ulRC = DosSysCtl( SYS_GETIRQSTATISTIC, &aulValues );
  if ( ulRC != 0 )
    return FALSE;

  if ( fInitial )
    fInterval = FALSE;
  else
  {
    fInterval = pgrGetTimestamp( &stGraph, &ulInterval );
    if ( fInterval )
    {
      ulInterval = ulTime - ulInterval;
      if ( ulInterval < 100 )
        // This can happen when user switches between enable/disable states
        // of data source. Avoid division by zero and inaccurate result.
        return FALSE;
    }
    pgrNewTimestamp( &stGraph, ulTime );
  }

  // Create/update IRQ objects
  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
  {
    if ( aulValues[ulIdx] != 0 )
    {
      pIRQ = _irqFind( ulIdx );

      if ( pIRQ == NULL )
      {
        pIRQ = debugMAlloc( sizeof(IRQ) );
        if ( pIRQ == NULL )
          continue;
        pIRQ->ulNum = ulIdx;
        pIRQ->ulIncrement = 0;
        pgrInitVal( &stGraph, &pIRQ->stGrVal );
        paIRQList[cIRQList++] = pIRQ;
      }
      else if ( fInterval )
      {
        pIRQ->ulIncrement = aulValues[ulIdx] - pIRQ->ulCounter;

        // Store updates per second to graph's values storage.
        pgrSetValue( &stGraph, &pIRQ->stGrVal,
          (aulValues[ulIdx] - pIRQ->ulCounter) * 1000 / ulInterval );
      }

      pIRQ->ulCounter = aulValues[ulIdx];
    }
  }

  // Sort IRQ objects
  qsort( paIRQList, cIRQList, sizeof(PIRQ), _irqSortComp );

  return TRUE;
}


// Interface routines of data source
// ---------------------------------

DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  ULONG		ulIdx;
  CHAR		szBuf[128];

  debugInit();

  // Store module handle of data source
  hDSModule = hMod;

  // Query pointers to helper routines.

  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniWriteStr = (PHiniWriteStr)pSLInfo->slQueryHelper( "iniWriteStr" );
  piniWriteData = (PHiniWriteData)pSLInfo->slQueryHelper( "iniWriteData" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  piniReadStr = (PHiniReadStr)pSLInfo->slQueryHelper( "iniReadStr" );
  piniReadData = (PHiniReadData)pSLInfo->slQueryHelper( "iniReadData" );
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
  pupdateLock = (PHupdateLock)pSLInfo->slQueryHelper( "updateLock" );
  pupdateUnlock = (PHupdateUnlock)pSLInfo->slQueryHelper( "updateUnlock" );
  pstrFromSec = (PHstrFromSec)pSLInfo->slQueryHelper( "strFromSec" );
  pstrLoad = (PHstrLoad)pSLInfo->slQueryHelper( "strLoad" );
  pctrlStaticToColorRect = (PHctrlStaticToColorRect)pSLInfo->slQueryHelper( "ctrlStaticToColorRect" );
  pctrlDlgCenterOwner = (PHctrlDlgCenterOwner)pSLInfo->slQueryHelper( "ctrlDlgCenterOwner" );
  pctrlQueryText = (PHctrlQueryText)pSLInfo->slQueryHelper( "ctrlQueryText" );

  // Load names of fields
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    pstrLoad( hMod, aulFldStrId[ulIdx], sizeof(szBuf), &szBuf );
    apszFields[ulIdx] = debugStrDup( &szBuf );
  }

  // Return data source information for main program
  return &stDSInfo;
}

DSEXPORT VOID APIENTRY dsUninstall()
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
    if ( apszFields[ulIdx] != NULL )
      debugFree( apszFields[ulIdx] );

  debugDone();
}

DSEXPORT VOID APIENTRY dsDone()
{
  ULONG		ulIdx;
  PIRQ		pIRQ;

  // Destroy objects

  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
  {
    if ( stIRQProperties.apszNames[ulIdx] != NULL )
      debugFree( stIRQProperties.apszNames[ulIdx] );
  }

  for( ulIdx = 0; ulIdx < cIRQList; ulIdx++ )
  {
    pIRQ = paIRQList[ulIdx];
    if ( pIRQ == NULL )
      continue;

    pgrDoneVal( &pIRQ->stGrVal );
    debugFree( pIRQ );
  }

  debugFree( paIRQList );
  pgrDone( &stGraph );

  WinDestroyPointer( hIconOn );
  WinDestroyPointer( hIconOff );

  if ( stGrParam.pszOrdinateCaption != NULL )
    debugFree( stGrParam.pszOrdinateCaption );
}

DSEXPORT BOOL APIENTRY dsInit()
{
  ULONG		ulValWindow;
  ULONG		ulTime;
  ULONG		ulIdx;
  CHAR		szIRQNameKey[8];
  CHAR		szBuf[MAX_IRQNAME_LEN + 1];

  ulNumSelected = (ULONG)(-1);
  paIRQList = NULL;
  cIRQList = 0;

  // Load graph ordinate's caption (IPS)
  pstrLoad( hDSModule, IDS_IPS, sizeof(szBuf), &szBuf );
  stGrParam.pszOrdinateCaption = debugStrDup( &szBuf );

  // Read properties

  for( ulIdx = 0; ulIdx < IRQ_MAX; ulIdx++ )
  {
    sprintf( &szIRQNameKey, "IRQ%u", ulIdx );

    stIRQProperties.apszNames[ulIdx] =
      piniReadStr( hDSModule, &szIRQNameKey, &szBuf,
                              sizeof(szBuf), NULL ) != 0 ?
        debugStrDup( &szBuf ) : NULL;
  }

  stGrParam.clrBackground =
    piniReadULong( hDSModule, "GrBgColor", DEF_GRBGCOLOR );
  stGrParam.clrBorder =
    piniReadULong( hDSModule, "GrBorderColor", DEF_GRBORDERCOLOR );
  stGrParam.pParamVal[0].clrGraph =
    piniReadULong( hDSModule, "LineColor", DEF_LINECOLOR );

  stIRQProperties.ulUpdateInterval = piniReadULong( hDSModule,
                                       "UpdateInterval", DEF_INTERVAL );
  if ( stIRQProperties.ulUpdateInterval < 100 ||
       stIRQProperties.ulUpdateInterval > 2000 ||
       stIRQProperties.ulUpdateInterval % 100 != 0 )
    stIRQProperties.ulUpdateInterval = DEF_INTERVAL;

  stIRQProperties.ulTimeWindow = piniReadULong( hDSModule,
                                    "TimeWindow", DEF_TIMEWIN );
  if ( stIRQProperties.ulTimeWindow < 60 ||
       stIRQProperties.ulTimeWindow > (60 * 5) )
    stIRQProperties.ulTimeWindow = DEF_TIMEWIN;

  ulIdx = FIELDS * sizeof(ULONG);
  piniReadData( hDSModule, "Fields", &stIRQProperties.aulFields,
                           &ulIdx );
  if ( ulIdx != FIELDS * sizeof(ULONG) )
  {
    stIRQProperties.aulFields[0] = DEF_ITEMFLD_0;
    stIRQProperties.aulFields[1] = DEF_ITEMFLD_1;
    stIRQProperties.aulFields[2] = DEF_ITEMFLD_2;
    stIRQProperties.aulFields[3] = DEF_ITEMFLD_3;
    stIRQProperties.aulFields[4] = DEF_ITEMFLD_4;
    stIRQProperties.aulFields[5] = DEF_ITEMFLD_5;
  }
  stIRQProperties.fNameFixWidth = piniReadULong( hDSModule,
                                 "NameFixWidth", (ULONG)DEF_ITEMNAMEFIXWIDTH );

  stIRQProperties.ulSort = piniReadULong( hDSModule, "Sort",
                                                     FLD_NUM );
  if ( stIRQProperties.ulSort > FLD_LOAD )
    stIRQProperties.ulSort = FLD_NUM;
  stIRQProperties.fSortDesc = piniReadULong( hDSModule, "SortDesc",
                                                        FALSE );

  // Allocate IRQ objects list

  paIRQList = debugMAlloc( IRQ_MAX * sizeof(PIRQ) );
  if ( paIRQList == NULL )
    return FALSE;

  // Graph data initialization

  ulTime = stIRQProperties.ulTimeWindow * 1000;
  ulValWindow = ulTime / stIRQProperties.ulUpdateInterval;

  // Minimum and maximum is zero - allow to determine the scale of ordinates
  // automatically.
  if ( !pgrInit( &stGraph, ulValWindow, ulTime, 0, 0 ) )
//  if ( !pgrInit( &stGraph, ulValWindow, ulTime, 0, 10 ) )
  {
    dsDone();
    return FALSE;
  }

  // Load led icons
  hIconOn = WinLoadPointer( HWND_DESKTOP, hDSModule, ID_ICON_ON );
  hIconOff = WinLoadPointer( HWND_DESKTOP, hDSModule, ID_ICON_OFF );

  // Query statistic
  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulTime, sizeof(ULONG) );
  if ( !_irqGetIRQStat( ulTime, TRUE ) )
  {
    dsDone();
    return FALSE;
  }

  // Select first item
  if ( cIRQList > 0 && ulNumSelected == ((ULONG)(-1)) )
    ulNumSelected = paIRQList[0]->ulNum;

  return TRUE;
}

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return stIRQProperties.ulUpdateInterval;
}

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return cIRQList;
}

DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  SIZEL		sizeText;
  ULONG		ulIdx;
  BOOL		fNeedCalcNameWidth = FALSE;

  // Calc parts of item sizes.

  if ( WinQuerySysValue( HWND_DESKTOP, SV_CXICON ) == 40 )
    ulIconSize = 16;
  else
    ulIconSize = 13;

  if ( cIRQList < 100 )
    putilGetTextSize( hps, 7, "IRQ 000", &sizeText );
  else
    putilGetTextSize( hps, 6, "IRQ 00", &sizeText );
  ulNoWidth = sizeText.cx;
  ulTextHeight = sizeText.cy;

  putilGetTextSize( hps, 10, "0000000000", &sizeText );
  ulCounterWidth = sizeText.cx;

  putilGetTextSize( hps, 6, "000000", &sizeText );
  ulLoadWidth = sizeText.cx;

  ulTextY = IRQ_BOX_VPAD; // Vertical offset for text in window

  // Calc item's window size. Determine whether the object will be a fixed size.

  pSize->cx = 0;
  pSize->cy = ulTextHeight;
  fItemsFixWidth = TRUE; // Global fixed size of windows flag.
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    switch( stIRQProperties.aulFields[ulIdx] )
    {
      case FLD_NUM:
        pSize->cx += ulNoWidth + IRQ_BOX_FLD_PAD;
        break;

      case FLD_COUNTER:
        pSize->cx += ulCounterWidth + IRQ_BOX_FLD_PAD;
        break;

      case FLD_LOAD:
      case FLD_INCREMENT:
        pSize->cx += ulLoadWidth + IRQ_BOX_FLD_PAD;
        break;

      case FLD_NAME:
        fNeedCalcNameWidth = stIRQProperties.fNameFixWidth;
        fItemsFixWidth = stIRQProperties.fNameFixWidth;
        break;

      case FLD_LED:
        pSize->cx += ulIconSize + IRQ_BOX_FLD_PAD;

        if ( ulTextHeight < ulIconSize )
        {
          // Led icon's height > text's height - calc centric text offset
          ulIconY = IRQ_BOX_VPAD;
          ulTextY = IRQ_BOX_VPAD + (ulIconSize / 2) - (ulTextHeight / 2);
          // Set window height to icon size.
          pSize->cy = ulIconSize;
        }
        else
          // Led icon's height < text's height - calc centric icon offset
          ulIconY = IRQ_BOX_VPAD + (ulTextHeight / 2) - (ulIconSize / 2);
        break;
    }
  }

  if ( fNeedCalcNameWidth )
  {
    // Have visible field Name and global flag stIRQProperties.fNameFixWidth
    // specified - calculate maximum width of IRQ names.
    PSZ		pszName;

    ulNameWidth = 0;
    for( ulIdx = 0; ulIdx < cIRQList; ulIdx++ )
    {
      pszName = stIRQProperties.apszNames[ paIRQList[ulIdx]->ulNum ];
      if ( pszName == NULL )
        continue;

      putilGetTextSize( hps, strlen( pszName ), pszName, &sizeText );
      if ( sizeText.cx > ulNameWidth )
        ulNameWidth = sizeText.cx;
    }

    if ( ulNameWidth != 0 )
      // Increase window's width by max. name width and field space.
      pSize->cx += ulNameWidth + IRQ_BOX_FLD_PAD;
  }

  // Add spaces.
  pSize->cx += ( 2 * IRQ_BOX_HPAD ) - IRQ_BOX_FLD_PAD;
  pSize->cy += 2 * IRQ_BOX_VPAD;
}

DSEXPORT VOID APIENTRY dsSetWnd(ULONG ulIndex, HWND hwnd, HPS hps)
{
  SIZEL		sizeText;
  PSZ		pszName;
  SWP		swp;

  if ( fItemsFixWidth )
    // Use calculated by dsSetWndStart() size.
    return;

  pszName = stIRQProperties.apszNames[ paIRQList[ulIndex]->ulNum ];
  if ( pszName == NULL )
    // Use calculated by dsSetWndStart() size.
    return;

  // Increase window's width on IRQ name text width.
  putilGetTextSize( hps, strlen( pszName ), pszName, &sizeText );
  WinQueryWindowPos( hwnd, &swp );
  WinSetWindowPos( hwnd, HWND_TOP, 0, 0,
                   swp.cx + sizeText.cx + IRQ_BOX_FLD_PAD, swp.cy, SWP_SIZE );
}

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  ULONG		cOldIRQList = cIRQList;
  PIRQ		*paOldIRQList;
  BOOL		fMayBeDynamic = ( stIRQProperties.ulSort == FLD_COUNTER ||
                                  stIRQProperties.ulSort == FLD_LOAD ) &&
                                !fItemsFixWidth;

  if ( fMayBeDynamic )
  {
    // Store current list of pointers to PIRQs. We'll need it to check
    // reordering of items.
    paOldIRQList = alloca( sizeof(IRQ) * cIRQList );
    if ( paOldIRQList != NULL )
      memcpy( paOldIRQList, paIRQList, sizeof(IRQ) * cIRQList );
  }

  // Query statistics.
  if ( !_irqGetIRQStat( ulTime, FALSE ) )
    return DS_UPD_NONE;

  if ( cOldIRQList != cIRQList )
    // Count of items is changed - list CHANGED.
    return DS_UPD_LIST;

  if ( fMayBeDynamic )
  {
    // Sorting FLD_COUNTER and FLD_LOAD thegas dynamic reordering of items.

    if ( paOldIRQList == NULL ||
         memcmp( paOldIRQList, paIRQList, sizeof(IRQ) * cIRQList ) != 0 )
      // The order of existing items has been changed - list CHANGED.
      return DS_UPD_LIST;
  }

  // Only existing items data have been updated - items UPDATED.
  return DS_UPD_DATA;
}

DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex)
{
  if ( ulIndex >= cIRQList )
    return FALSE;

  ulNumSelected = paIRQList[ulIndex]->ulNum;
  return TRUE;
}

DSEXPORT ULONG APIENTRY dsGetSel()
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cIRQList; ulIdx++ )
  {
    if ( paIRQList[ulIdx]->ulNum == ulNumSelected )
      return ulIdx;
  }

  return DS_SEL_NONE;
}

DSEXPORT BOOL APIENTRY dsIsSelected(ULONG ulIndex)
{
  return ulIndex < cIRQList ?
           paIRQList[ulIndex]->ulNum == ulNumSelected : FALSE;
}

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  POINTL	pt;
  ULONG		ulLoad = 0;
  SIZEL		sizeText;
  ULONG		ulLen, ulX;
  CHAR		szBuf[128];
  BOOL		fLoad;
  PSZ		pszName;
  ULONG		ulIdx;
  ULONG		ulField;

  if ( ulIndex >= cIRQList )
    return;

  fLoad = pgrGetValue( &stGraph, &paIRQList[ulIndex]->stGrVal, &ulLoad );
  pszName = stIRQProperties.apszNames[ paIRQList[ulIndex]->ulNum ];

  pt.x = IRQ_BOX_HPAD;
  pt.y = ulTextY;

  // Draw all fields without flag "hidden".
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    ulField = stIRQProperties.aulFields[ulIdx];
    switch( ulField )
    {
      case FLD_NUM:
        ulLen = sprintf( &szBuf, "IRQ %u", paIRQList[ulIndex]->ulNum );
        GpiCharStringAt( hps, &pt, ulLen, &szBuf );
        pt.x += ulNoWidth + IRQ_BOX_FLD_PAD;
        break;

      case FLD_COUNTER:
        ulX = pt.x + ulCounterWidth;
        ulLen = sprintf( &szBuf, "%u", paIRQList[ulIndex]->ulCounter );
        putilGetTextSize( hps, ulLen, &szBuf, &sizeText );
        pt.x = ulX - sizeText.cx;
        GpiCharStringAt( hps, &pt, ulLen, &szBuf );
        pt.x = ulX + IRQ_BOX_FLD_PAD;
        break;

      case FLD_LOAD:
      case FLD_INCREMENT:
        ulX = pt.x + ulLoadWidth;
        if ( fLoad )
        {
          ulLen = sprintf( &szBuf, "%u",
                           ulField == FLD_LOAD ?
                             ulLoad : paIRQList[ulIndex]->ulIncrement );
          putilGetTextSize( hps, ulLen, &szBuf, &sizeText );
          pt.x = ulX - sizeText.cx;
          GpiCharStringAt( hps, &pt, ulLen, &szBuf );
        }
        pt.x = ulX + IRQ_BOX_FLD_PAD;
        break;

      case FLD_NAME:
        if ( pszName != NULL )
        {
          GpiCharStringAt( hps, &pt, strlen( pszName ), pszName );

          if ( !stIRQProperties.fNameFixWidth )
          {
            GpiQueryCurrentPosition( hps, &pt );
            pt.x += IRQ_BOX_FLD_PAD;
            pt.y = ulTextY;
            break;
          }
        }

        if ( stIRQProperties.fNameFixWidth && ulNameWidth != 0 )
          pt.x += ulNameWidth + IRQ_BOX_FLD_PAD;
        break;

      case FLD_LED:
        WinDrawPointer( hps, pt.x, ulIconY,
                        fLoad && ulLoad > 0 ? hIconOn : hIconOff, DP_MINI );
        pt.x += ulIconSize + IRQ_BOX_FLD_PAD;
        break;
    }
  }
}

DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  PIRQ			pIRQ;
  CHAR			szAbscissaTopCaption[512];
  CHAR			szAbscissaBottomCaption[64];
  LONG			cBytes;
  PGRVAL		pGrVal;
  RECTL			rclGraph;
  SIZEL			sizeText;

  if ( ulNumSelected == (ULONG)(-1) )
    return;

  pIRQ = _irqFind( ulNumSelected );
  if ( pIRQ != NULL )
  {
    cBytes = sprintf( &szAbscissaTopCaption, "IRQ %u", pIRQ->ulNum );
    stGrParam.pszAbscissaTopCaption = &szAbscissaTopCaption;

    if ( stIRQProperties.apszNames[pIRQ->ulNum] != NULL )
      _bprintf( &szAbscissaTopCaption[cBytes],
                sizeof(szAbscissaTopCaption) - cBytes, ": %s",
                stIRQProperties.apszNames[pIRQ->ulNum] );

    stGrParam.pszAbscissaBottomCaption =
      pstrFromSec( stIRQProperties.ulTimeWindow,
                   sizeof(szAbscissaBottomCaption),
                   &szAbscissaBottomCaption ) > 0 ?
      &szAbscissaBottomCaption : NULL;

    putilGetTextSize( hps, 8, "00000000", &sizeText );
    rclGraph.xLeft = sizeText.cx + GRAPG_ORDINATE_LEFT_PAD;
    rclGraph.xRight = psizeWnd->cx - 30;
    rclGraph.yTop = psizeWnd->cy - sizeText.cy - GRAPG_ABSCISSA_TOP_PAD - 5;
    rclGraph.yBottom = sizeText.cy + GRAPG_ABSCISSA_BOTTOM_PAD + 5;
    pGrVal = &pIRQ->stGrVal;
    pgrDraw( &stGraph, hps, &rclGraph, 1, &pGrVal, &stGrParam );
  }
}

DSEXPORT VOID APIENTRY dsGetHintText(ULONG ulIndex, PCHAR pcBuf, ULONG cbBuf)
{
  BOOL		fNum = FALSE;
  BOOL		fCounter = FALSE;
  BOOL		fLoad = FALSE;
  BOOL		fName = FALSE;
  ULONG		ulNum, ulLoad;
  ULONG		ulIdx, cBytes;
  PSZ		pszName;
  HAB		hab;

  if ( ulIndex >= cIRQList )
    return;

  ulNum = paIRQList[ulIndex]->ulNum;

  // Seek hidden fields
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    switch( stIRQProperties.aulFields[ulIdx] )
    {
      case FLD_NUM | FLD_FL_HIDDEN:
        fNum = TRUE;
        break;
      case FLD_COUNTER | FLD_FL_HIDDEN:
        fCounter = TRUE;
        break;
      case FLD_LOAD | FLD_FL_HIDDEN:
        fLoad = pgrGetValue( &stGraph, &paIRQList[ulIndex]->stGrVal,
                                        &ulLoad );
        break;
      case FLD_NAME | FLD_FL_HIDDEN:
        pszName = stIRQProperties.apszNames[ulNum];
        fName = pszName != NULL;
        break;
    }
  }

  // Make hint text

  if ( fNum & fName )
    cBytes = _bprintf( pcBuf, cbBuf, "IRQ %u: %s\n", ulNum, pszName );
  else if ( fNum )
    cBytes = _bprintf( pcBuf, cbBuf, "IRQ %u\n", ulNum );
  else if ( fName )
    cBytes = _bprintf( pcBuf, cbBuf, "%s\n", pszName );
  else
    cBytes = 0;

  pcBuf += cBytes;
  cbBuf -= cBytes;

/*
  Presentation Manager Programming Guide and Reference:
  The operating system does not generally use the information supplied by the
  hab parameter to its calls; instead, it deduces it from the identity of the
  thread that is making the call. Thus an OS/2 application is not required to
  supply any particular value as the hab parameter. 
*/
  hab = NULLHANDLE;//WinInitialize( 0 );

  if ( fCounter )
  {
    cBytes = WinLoadString( hab, hDSModule, IDS_FLD_COUNTER, cbBuf, pcBuf );
    pcBuf += cBytes;
    cbBuf -= cBytes;
    cBytes = _bprintf( pcBuf, cbBuf, ": %u\n", paIRQList[ulIndex]->ulCounter );
    pcBuf += cBytes;
    cbBuf -= cBytes;
  }

  if ( fLoad )
  {
    cBytes = WinLoadString( hab, hDSModule, IDS_FLD_LOAD, cbBuf, pcBuf );
    pcBuf += cBytes;
    cbBuf -= cBytes;
    cBytes = _bprintf( pcBuf, cbBuf, ": %u\n", ulLoad );
    pcBuf += cBytes;
    cbBuf -= cBytes;
  }
}

DSEXPORT ULONG APIENTRY dsSortBy(ULONG ulNewSort)
{
  ULONG		ulResult;

  if ( ulNewSort != DSSORT_QUERY )
  {
    stIRQProperties.ulSort = ulNewSort & DSSORT_VALUE_MASK;
    stIRQProperties.fSortDesc = (ulNewSort & DSSORT_FL_DESCENDING) != 0;
    // Sort IRQ objects
    qsort( paIRQList, cIRQList, sizeof(PIRQ), _irqSortComp );
    // Store new sort type to the ini-file
    piniWriteULong( hDSModule, "Sort", stIRQProperties.ulSort );
    piniWriteULong( hDSModule, "SortDesc",
                                  stIRQProperties.fSortDesc );
  }

  ulResult = stIRQProperties.ulSort;
  if ( stIRQProperties.fSortDesc )
    ulResult |= DSSORT_FL_DESCENDING;

  return ulResult;
}
