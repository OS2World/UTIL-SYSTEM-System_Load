#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <ds.h>
#include <sl.h>
#include <debug.h>
#include "process.h"
#include "sysstate.h"

#define UPDATE_INTERVAL		1000

#define FIELDS			3
#define FLD_TYPE		0
#define FLD_NAME		1
#define FLD_PID			2

#define INIT_BUF_SIZE		(1024 * 60)
#define INCR_BUF_SIZE		(1024 * 10)

typedef struct _PROCITEM {
  PSZ		pszName;
  ULONG		cbName;
  PSZ		pszPathName;
  PVOID		pPRec;
  ULONG		ulUpdateStamp;
  SSPROCESS	stProcess;
} PROCITEM, *PPROCITEM;

typedef struct _PROCITEMLIST {
  ULONG		ulCount;
  ULONG		ulMax;
  PPROCITEM	*ppItems;
} PROCITEMLIST, *PPROCITEMLIST;

// Module information

static PSZ		 apszFields[FIELDS] = { NULL };

static DSINFO stDSInfo = {
  sizeof(DSINFO),			// Size of this data structure.
  "~Processes",				// Menu item title
  FIELDS,				// Number of "sort-by" strings
  &apszFields,				// "Sort-by" strings.
  DS_FL_ITEM_BG_LIST |			// Flags DS_FL_*
  DS_FL_SELITEM_BG_LIST |
  DS_FL_NO_BG_UPDATES,
  80,					// Items horisontal space, %Em
  50,					// Items vertical space, %Em
  0					// Help main panel index.
};

static ULONG		aulFldStrId[FIELDS] = {
  IDS_FLD_TYPE,		// 0 - FLD_TYPE
  IDS_FLD_NAME,		// 1 - FLD_NAME
  IDS_FLD_PID		// 2 - FLD_PID
};

static ULONG		aulProcTypesStrId[8] = {
  IDS_PROC_TYPE_FS,	// 0 - SSPROC_TYPE_FS
  IDS_PROC_TYPE_DOS,	// 1 - SSPROC_TYPE_DOS
  IDS_PROC_TYPE_VIO,	// 2 - SSPROC_TYPE_VIO
  IDS_PROC_TYPE_PM,	// 3 - SSPROC_TYPE_PM
  IDS_PROC_TYPE_DET,	// 4 - SSPROC_TYPE_DET
  0,			// 5 - not used
  0,			// 6 - not used
  IDS_PROC_TYPE_WINVDM  // 7 - SSPROC_TYPE_WINVDM
};
static PSZ		apszProcTypes[ARRAYSIZE(aulProcTypesStrId)];

static ULONG		aulDetailsStrId[] = {
  IDS_DETAILS_FILE,
  IDS_FLD_PID,
  IDS_FLD_TYPE,
  IDS_DETAILS_PARENT,
  IDS_DETAILS_THREADS
};

HMODULE			hDSModule;	// Module handle
HAB			hab;
PID			pidSelect;	// Selected process id
PVOID			pPtrRec;
BOOL			fConfirmClose;
BOOL			fConfirmKill;

static ULONG		cbPtrRec;
static PROCITEMLIST	stList;
static ULONG		ulSortFld;
static BOOL		fSortDesc;
static PSIZEL		psizeEm;
static PID		pidMenu;
static PSWBLOCK		pstSwBlock;
static ULONG		cbSwBlock;
static LONG		lSelItemBgColor;
static ULONG		cMenuHWnd;
static PHWND		paMenuHWnd;


// Pointers to helper routines.

PHiniWriteULong			piniWriteULong;
PHiniWriteLong			piniWriteLong;
PHiniReadULong			piniReadULong;
PHiniReadLong			piniReadLong;
PHutilGetTextSize		putilGetTextSize;
PHutilBox			putilBox;
PHutilCharStringRect		putilCharStringRect;
PHutilMixRGB			putilMixRGB;
PHutilGetColor			putilGetColor;
PHutilLoadInsertStr		putilLoadInsertStr;
PHupdateLock			pupdateLock;
PHupdateUnlock			pupdateUnlock;
PHstrFromBytes			pstrFromBytes;
PHstrLoad			pstrLoad;
PHctrlDlgCenterOwner		pctrlDlgCenterOwner;
PHctrlSubclassNotebook		pctrlSubclassNotebook;


// showExplorer(), explorer.c.
extern VOID showExplorer(HWND hwndOwner, PID pid);


BOOL querySysState(PULONG pcbPtrRec, PVOID *ppPtrRec)
{
  ULONG		ulRC;

  if ( ( *ppPtrRec == NULL ) || ( *pcbPtrRec == 0 ) )
  {
    *pcbPtrRec = 0;
    goto l00;
  }

  do
  {
    ulRC = ssQueryData( *ppPtrRec, *pcbPtrRec,
                        SS_PROCESS | SS_FILESYS | SS_MTE );
    if ( ulRC == NO_ERROR )
      break;

    if ( *ppPtrRec != NULL )
      debugFree( *ppPtrRec );
    *ppPtrRec = NULL;

    if ( ulRC != ERROR_BUFFER_OVERFLOW )
    {
      debug( "ssQueryData(), rc = %u", ulRC );
      return FALSE;
    }

l00:
    *pcbPtrRec += INCR_BUF_SIZE;
    *ppPtrRec = debugMAlloc( *pcbPtrRec );
    if ( *ppPtrRec == NULL )
    {
      debug( "Not enough memory" );
      return FALSE;
    }
    debug( "Buffer size expanded to %u bytes", *pcbPtrRec );
  }
  while( TRUE );

  return TRUE;
}

static int _sortComp(const void *p1, const void *p2)
{
  PPROCITEM	pItem1 = *(PPROCITEM *)p1;
  PPROCITEM	pItem2 = *(PPROCITEM *)p2;
  int		iRes;

  switch( ulSortFld )
  {
    case FLD_TYPE:
      iRes = pItem1->stProcess.ulType - pItem2->stProcess.ulType;
      if ( iRes != 0 )
        break;
      // equal types - go to sort by FLD_NAME

    case FLD_NAME:
      iRes = WinCompareStrings( hab, 0, 0, pItem1->pszName,
                                 pItem2->pszName, 0 );
      if ( iRes != WCS_EQ )
      {
        iRes = iRes == WCS_LT ? -1 : 1;
        break;
      }
      // equal names - go to sort by IDS_FLD_PID

    default: // IDS_FLD_PID
      iRes = pItem1->stProcess.pid - pItem2->stProcess.pid;
      break;
  }

  return fSortDesc ? -iRes : iRes;
}

static BOOL _insertItem(PPROCITEMLIST pstList, ULONG ulIndex, PPROCITEM pstItem)
{
  PPROCITEM	*ppNewItems;
  
  // Expand list of items if necessary.
  if ( pstList->ulCount == pstList->ulMax )
  {
    pstList->ulMax += 16;
    ppNewItems = debugReAlloc( pstList->ppItems,
                               pstList->ulMax * sizeof(PPROCITEM) );
    if ( ppNewItems == NULL )
    {
      debug( "Not enough memory" );
      return FALSE;
    }
    pstList->ppItems = ppNewItems;
  }

  if ( ulIndex >= pstList->ulCount )
    ulIndex = pstList->ulCount;
  else
  {
    memmove( &pstList->ppItems[ulIndex + 1], &pstList->ppItems[ulIndex],
             (pstList->ulCount - ulIndex) * sizeof(PPROCITEM) );
  }
     
  pstList->ppItems[ulIndex] = pstItem;
  pstList->ulCount++;
  return TRUE;
}

static ULONG _findItem(PPROCITEMLIST pstList, PID pid, PPROCITEM *ppItem)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < stList.ulCount; ulIdx++ )
  {
    if ( stList.ppItems[ulIdx]->stProcess.pid == pid )
    {
      if ( ppItem != NULL )
        *ppItem = stList.ppItems[ulIdx];
      return ulIdx;
    }
  }

  return DS_SEL_NONE;
}

static VOID _freeList(PPROCITEMLIST pstList)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < pstList->ulCount; ulIdx++ )
  {
    if ( pstList->ppItems[ulIdx] != NULL )
      debugFree( pstList->ppItems[ulIdx] );
  }

  if ( pstList->ppItems != NULL )
    debugFree( pstList->ppItems );

  pstList->ulCount = 0;
  pstList->ulMax = 0;
  pstList->ppItems = 0;
}

static VOID _switchTo(HWND hwnd)
{
  SWP		swp;

  if ( !WinQueryWindowPos( hwnd, &swp ) )
  {
    debug( "WinQueryWindowPos() failed" );
  }
  else if ( (swp.fl & (SWP_HIDE | SWP_MINIMIZE)) != 0 )
  {
    HSWITCH	hsw = WinQuerySwitchHandle( hwnd, 0 );

    if ( (swp.fl & SWP_HIDE) != 0 )
      WinSetWindowPos( hwnd, 0, 0, 0, 0, 0, SWP_SHOW );

    if ( WinSwitchToProgram( hsw ) == 0 )
      WinSetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0,
                       SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER );
  }
  else if ( !WinSetActiveWindow( HWND_DESKTOP, hwnd ) )
    debug( "WinSetActiveWindow() failed" );
}

static VOID _closeWindow(HWND hwnd, HWND hwndOwner, PSZ pszConfirmTile)
{
  if ( fConfirmClose )
  {
    ULONG	ulIdx;
    BOOL	fFound = FALSE;
    PSZ		pszVal;
    CHAR	szBuf[256];
    ULONG	cbBuf;

    for( ulIdx = 0; ulIdx < pstSwBlock->cswentry; ulIdx++ )
    {
      if ( pstSwBlock->aswentry[ulIdx].swctl.hwnd == hwnd )
      {
        fFound = TRUE;
        break;
      }
    }

    if ( !fFound )
      return;

    pszVal = &pstSwBlock->aswentry[ulIdx].swctl.szSwtitle;
    cbBuf = putilLoadInsertStr( hDSModule, FALSE, IDM_CLOSE_ASK,
                                1, &pszVal, sizeof(szBuf) - 1, &szBuf );
    if ( cbBuf != 0 )
    {
      szBuf[cbBuf] = '\0';
      if ( WinMessageBox( HWND_DESKTOP, hwndOwner, &szBuf, pszConfirmTile,
                          0, MB_MOVEABLE | MB_ICONQUESTION | MB_YESNO ) !=
           MBID_YES )
        return;
    }
  }

  WinPostMsg( hwnd, WM_SYSCOMMAND, (MPARAM)SC_CLOSE,
              MPFROM2SHORT( CMDSRC_OTHER, TRUE ) );
}

static VOID _killProcess(PID pid, HWND hwndOwner)
{
  ULONG			ulRC;
  PPROCITEM		pstItem;
  CHAR			szPID[16];
  CHAR			szErrCode[16];
  PSZ			apszVal[3];
  CHAR			szBuf[256];
  ULONG			cbBuf;

  if ( _findItem( &stList, pid, &pstItem ) == DS_SEL_NONE )
    return;

  if ( fConfirmKill )
  {
    apszVal[0] = pstItem->pszName;
    _snprintf( szPID, sizeof(szPID), "0x%X", pid );
    apszVal[1] = &szPID;
    cbBuf = putilLoadInsertStr( hDSModule, FALSE, IDM_KILL_ASK,
                                2, &apszVal, sizeof(szBuf) - 1, &szBuf );
    if ( cbBuf != 0 )
    {
      szBuf[cbBuf] = '\0';
      if ( WinMessageBox( HWND_DESKTOP, hwndOwner, &szBuf, pstItem->pszName,
                          0, MB_MOVEABLE | MB_ICONQUESTION | MB_YESNO ) !=
           MBID_YES )
        return;
    }
  }

  ulRC = DosKillProcess( DKP_PROCESS, pid );
  if ( ulRC != NO_ERROR )
  {
    ultoa( ulRC, szErrCode, 10 );
    apszVal[2] = &szErrCode;
    cbBuf = putilLoadInsertStr( hDSModule, FALSE,
                                ( ulRC == ERROR_ZOMBIE_PROCESS ?
                                   IDM_KILL_FAIL_ZOMBIE : IDM_KILL_FAIL ),
                                2, &apszVal, sizeof(szBuf) - 1, &szBuf );
    if ( cbBuf != 0 )
    {
      szBuf[cbBuf] = '\0';
      WinMessageBox( HWND_DESKTOP, hwndOwner, &szBuf, pstItem->pszName, 0,
                     MB_MOVEABLE | MB_ICONHAND | MB_CANCEL );
    }
  } 
}


// Interface routines of data source
// ---------------------------------

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime);

DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  PHutilGetEmSize	putilGetEmSize;
  ULONG			ulIdx;
  CHAR			szBuf[128];

  debugInit();

  // Store module handle of data source.
  hDSModule = hMod;
  // Store anchor block handle.
  hab = pSLInfo->hab;

  // Query pointers to helper routines.

  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniWriteLong = (PHiniWriteLong)pSLInfo->slQueryHelper( "iniWriteLong" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  piniReadLong = (PHiniReadLong)pSLInfo->slQueryHelper( "iniReadLong" );
  putilGetTextSize = (PHutilGetTextSize)pSLInfo->slQueryHelper( "utilGetTextSize" );
  putilBox = (PHutilBox)pSLInfo->slQueryHelper( "utilBox" );
  putilCharStringRect = (PHutilCharStringRect)pSLInfo->slQueryHelper( "utilCharStringRect" );
  putilMixRGB = (PHutilMixRGB)pSLInfo->slQueryHelper( "utilMixRGB" );
  putilGetColor = (PHutilGetColor)pSLInfo->slQueryHelper( "utilGetColor" );
  putilLoadInsertStr = (PHutilLoadInsertStr)pSLInfo->slQueryHelper( "utilLoadInsertStr" );
  pupdateLock = (PHupdateLock)pSLInfo->slQueryHelper( "updateLock" );
  pupdateUnlock = (PHupdateUnlock)pSLInfo->slQueryHelper( "updateUnlock" );
  pstrFromBytes = (PHstrFromBytes)pSLInfo->slQueryHelper( "strFromBytes" );
  pstrLoad = (PHstrLoad)pSLInfo->slQueryHelper( "strLoad" );
  pctrlDlgCenterOwner = (PHctrlDlgCenterOwner)pSLInfo->slQueryHelper( "ctrlDlgCenterOwner" );
  pctrlSubclassNotebook = (PHctrlSubclassNotebook)pSLInfo->slQueryHelper( "ctrlSubclassNotebook" );

  putilGetEmSize = (PHutilGetEmSize)pSLInfo->slQueryHelper( "utilGetEmSize" );
  psizeEm = putilGetEmSize( hMod );

  // Load names of fields for sorting.
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
  {
    if ( apszFields[ulIdx] != NULL )
      debugFree( apszFields[ulIdx] );
  }

  debugDone();
}

DSEXPORT BOOL APIENTRY dsInit()
{
  ULONG		ulIdx;
  CHAR		szBuf[128];

  memset( &stList, '\0', sizeof(PROCITEMLIST) );
  pidSelect = (PID)(-1);
  pstSwBlock = NULL;
  cbSwBlock = 0;
  cMenuHWnd = 0;
  paMenuHWnd = NULL;

  // Load names of process types.
  for( ulIdx = 0; ulIdx < ARRAYSIZE(apszProcTypes); ulIdx++ )
  {
    if ( aulProcTypesStrId[ulIdx] == 0 )
      apszProcTypes[ulIdx] = NULL;
    else
    {
      pstrLoad( hDSModule, aulProcTypesStrId[ulIdx], sizeof(szBuf), &szBuf );
      apszProcTypes[ulIdx] = debugStrDup( &szBuf );
    }
  }

  // Allocate buffer to query system information.
  pPtrRec = debugMAlloc( INIT_BUF_SIZE );
  if ( pPtrRec == NULL )
  {
    debug( "Not enough memory" );
    return FALSE;
  }
  cbPtrRec = INIT_BUF_SIZE;

  ulSortFld = piniReadULong( hDSModule, "Sort", FLD_TYPE );
  fSortDesc = (BOOL)piniReadULong( hDSModule, "SortDesc", FALSE );
  fConfirmClose = (BOOL)piniReadULong( hDSModule, "confirmClose", TRUE );
  fConfirmKill = (BOOL)piniReadULong( hDSModule, "confirmKill", TRUE );

  dsUpdate( 0 );

  return stList.ulCount != 0;
}

DSEXPORT VOID APIENTRY dsDone()
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < ARRAYSIZE(apszProcTypes); ulIdx++ )
  {
    if ( apszProcTypes[ulIdx] != NULL )
      debugFree( apszProcTypes[ulIdx] );
  }

  if ( pPtrRec != NULL )
    debugFree( pPtrRec );

  if ( pstSwBlock != NULL )
    debugFree( pstSwBlock );

  if ( paMenuHWnd != NULL )
    debugFree( paMenuHWnd );

  _freeList( &stList );
}

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return stList.ulCount;
}

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return UPDATE_INTERVAL;
}

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  PVOID		pPRec;
  PPROCITEM	pstItem;
  BOOL		fListChanged = FALSE;
  ULONG		ulIdx;
  SSPROCESS	stProcess;
  BOOL		fFound;
  ULONG		cSwEntries;
  PVOID		pLRec;
  static ULONG	ulUpdateStamp = 0;

  if ( !querySysState( &cbPtrRec, &pPtrRec ) )
    return DS_UPD_NONE;

  ulUpdateStamp++;
  for( pPRec = ssFirstProcess( pPtrRec ); pPRec != NULL;
       pPRec = ssNextProcess( pPRec ) )
  {
    ssGetProcess( pPRec, &stProcess );
    fFound = _findItem( &stList, stProcess.pid, &pstItem ) != DS_SEL_NONE;

    if ( !fFound )
    {
      pstItem = debugMAlloc( sizeof(PROCITEM) );
      if ( pstItem == NULL )
        break;

      if ( !_insertItem( &stList, ~0, pstItem ) )
      {
        debugFree( pstItem );
        break;
      }
      fListChanged = TRUE;
    }

    // Fill item.

    pstItem->pPRec = pPRec;
    pstItem->ulUpdateStamp = ulUpdateStamp;
    pstItem->stProcess = stProcess;
    pLRec = ssFindModule( pPtrRec, pstItem->stProcess.hModule );
    if ( pLRec != NULL )
    {
      SSMODULE		stModule;
      PCHAR		pcSlash;

      ssGetModule( pLRec, &stModule );
      if ( ( pstItem->stProcess.ulType == SSPROC_TYPE_DOS ) &&  // Kernel bug,
           ( strcmp( stModule.pszName, "SYSINIT" ) == 0 ) )     // must be VDM?
      {
        pstItem->pszPathName = "VDM";
        pstItem->pszName = "VDM";
      }
      else
      {
        pstItem->pszPathName = stModule.pszName;
        pcSlash = strrchr( stModule.pszName, '\\' );
        pstItem->pszName = pcSlash == NULL ? stModule.pszName : (pcSlash + 1);
      }
    }
    else
    {
      debugCP( "Should not occur" );
      pstItem->pszName = "???";
      pstItem->pszPathName = "???";
    }
    pstItem->cbName = strlen( pstItem->pszName );
  }

  ulIdx = 0;
  while( ulIdx < stList.ulCount )
  {
    pstItem = stList.ppItems[ulIdx];

    if ( pstItem->ulUpdateStamp != ulUpdateStamp )
    {
      if ( pstItem->pPRec != NULL )
        fListChanged = TRUE;

      debugFree( pstItem );
      stList.ulCount--;
      stList.ppItems[ulIdx] = stList.ppItems[stList.ulCount];
    }
    else
      ulIdx++;
  }

  // Sort items.
  qsort( stList.ppItems, stList.ulCount, sizeof(PPROCITEM), _sortComp );

  // Insert items-separators.
  if ( ( ulSortFld == FLD_TYPE ) && ( stList.ulCount > 1 ) )
  {
    ULONG	ulType = ~0;

    for( ulIdx = 0; ulIdx < stList.ulCount - 1; ulIdx++ )
    {
      if ( stList.ppItems[ulIdx]->stProcess.ulType != ulType )
      {
        ulType = stList.ppItems[ulIdx]->stProcess.ulType;

        pstItem = debugCAlloc( 1, sizeof(PROCITEM) );
        if ( pstItem == NULL )
          break;

        if ( !_insertItem( &stList, ulIdx, pstItem ) )
        {
          debugFree( pstItem );
          break;
        }

        if ( ( ulType < ARRAYSIZE(aulProcTypesStrId) ) &&
             ( apszProcTypes[ulType] != NULL ) )
        {
          pstItem->pszName = apszProcTypes[ulType];
          pstItem->cbName = strlen( apszProcTypes[ulType] );
        }
        pstItem->stProcess.ulType = ulType;
        pstItem->ulUpdateStamp = ulUpdateStamp;
        ulIdx++;
      }
    }
  }

  // Query window list.

  cSwEntries = WinQuerySwitchList( hab, pstSwBlock, cbSwBlock );
  if ( cSwEntries == 0 )
  {
    debugCP( "WinQuerySwitchList() failed" );
  }
  else if ( pstSwBlock == NULL || ( cSwEntries > pstSwBlock->cswentry ) )
  {
    // Expand buffer for window list.
    ULONG	cbNewSwBlock =
                           sizeof(HSWITCH) + ( cSwEntries * sizeof(SWENTRY) );
    PSWBLOCK	pstNewSwBlock = (PSWBLOCK)debugMAlloc( cbNewSwBlock );

    if ( pstNewSwBlock != NULL )
    {
      if ( pstSwBlock != NULL )
        debugFree( pstSwBlock );

      pstSwBlock = pstNewSwBlock;
      cbSwBlock = cbNewSwBlock;

      // Query data to the expanded buffer.
      if ( WinQuerySwitchList( hab, pstSwBlock, cbSwBlock ) == 0 )
        debugCP( "WinQuerySwitchList() failed" );
    }
  }

  return fListChanged ? DS_UPD_LIST : DS_UPD_DATA;
}

DSEXPORT ULONG APIENTRY dsSortBy(ULONG ulNewSort)
{
  ULONG		ulResult;

  if ( ulNewSort != DSSORT_QUERY )
  {
    ulSortFld = ulNewSort & DSSORT_VALUE_MASK;
    fSortDesc = (ulNewSort & DSSORT_FL_DESCENDING) != 0;
    // Update list.
    dsUpdate( 0 );
    // Store new sort type to the ini-file
    piniWriteULong( hDSModule, "Sort", ulSortFld );
    piniWriteULong( hDSModule, "SortDesc", fSortDesc );
  }

  ulResult = ulSortFld;
  if ( fSortDesc )
    ulResult |= DSSORT_FL_DESCENDING;

  return ulResult;
}

DSEXPORT VOID APIENTRY dsGetHintText(ULONG ulIndex, PCHAR pcBuf, ULONG cbBuf)
{
  PPROCITEM  	pstItem = stList.ppItems[ulIndex];
  LONG		cBytes;

  if ( pstItem->pPRec == NULL )		// Separator.
    return;

  cBytes = _bprintf( pcBuf, cbBuf, "%s\n", pstItem->pszPathName );
  if ( cBytes <= 0 )
    return;
  pcBuf += cBytes;
  cbBuf -= cBytes;
}

DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex)
{
  PPROCITEM  	pstItem = stList.ppItems[ulIndex];

  if ( pstItem->pPRec == NULL )
    return FALSE;

  pidSelect = pstItem->stProcess.pid;
  return TRUE;
}

DSEXPORT ULONG APIENTRY dsGetSel()
{
  return _findItem( &stList, pidSelect, NULL );
}

BOOL APIENTRY dsIsSelected(ULONG ulIndex)
{
  return stList.ppItems[ulIndex]->stProcess.pid == pidSelect;
}

DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  // Color may vary. Is why we take the color here, before each complete
  // redraw.
  lSelItemBgColor = putilGetColor( hDSModule, DS_COLOR_ITEMHIBACK );
}

DSEXPORT VOID APIENTRY dsSetWnd(ULONG ulIndex, HWND hwnd, HPS hps)
{
  PPROCITEM  	pstItem = stList.ppItems[ulIndex];
  SIZEL		sizeText;

  putilGetTextSize( hps, pstItem->cbName, pstItem->pszName, &sizeText );

  if ( ( pstItem->pPRec == NULL ) && ( pstItem->pszName != NULL ) )
  {
    // Separator.

    sizeText.cy = psizeEm->cy + (psizeEm->cy >> 0);
  }
  else
  {
    if ( ulSortFld == FLD_TYPE )
      sizeText.cx += psizeEm->cx;

    sizeText.cy = psizeEm->cy + (psizeEm->cy >> 2);
  }
  sizeText.cx += psizeEm->cx >> 2;

  WinSetWindowPos( hwnd, HWND_TOP, 0, 0, sizeText.cx, sizeText.cy, SWP_SIZE );
}

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  PPROCITEM  	pstItem = stList.ppItems[ulIndex];
  POINTL	pt;
  RECTL		rect;
  BOOL		fTypeSortItem = ( ulSortFld == FLD_TYPE ) &&
                                ( pstItem->pPRec != NULL );
  LONG		lColor = GpiQueryColor( hps );
  LONG		lBackColor = GpiQueryBackColor( hps );

  if ( pstItem->pszName == NULL )
    return;

  if ( stList.ppItems[ulIndex]->stProcess.pid == pidSelect )
  {
    rect.xLeft = fTypeSortItem ? psizeEm->cx : 0;
    rect.yBottom = 0;
    rect.xRight = psizeWnd->cx;
    rect.yTop = psizeWnd->cy;
    putilBox( hps, &rect, lSelItemBgColor );
    GpiSetBackColor( hps, lSelItemBgColor );
  }

  pt.x = (psizeEm->cx >> 3) +
         ( fTypeSortItem ? psizeEm->cx : 0 );
  pt.y = 1 + ( psizeWnd->cy - psizeEm->cy ) >> 1;

  if ( pstItem->pPRec == NULL )
  {
    // Gradient line under separator's text.
    POINTL	ptL;
    ULONG	ulStep = psizeWnd->cx / 8;
    ULONG	ulColorMix = 0;

    ptL.x = 0;
    ptL.y = 0;
    GpiMove( hps, &ptL );
    do
    {
      GpiSetColor( hps, putilMixRGB( lColor, lBackColor, ulColorMix ) );
      ulColorMix += 23;

      ptL.x += ulStep;
      GpiLine( hps, &ptL );
    }
    while( ptL.x < psizeWnd->cx );
    GpiSetColor( hps, lColor );
  }
  else
    GpiSetColor( hps, putilMixRGB( lColor, lBackColor, 65 ) );

  GpiCharStringAt( hps, &pt, pstItem->cbName, pstItem->pszName );
}

DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  PPROCITEM	pstItem;
  PPROCITEM	pstParentItem;
  CHAR		szBuf[128];
  ULONG		cbBuf;
  ULONG		ulIdx;
  POINTL	pt1, pt2;
  SIZEL		sizeEm;
  ULONG		ulValXPos = 0;
  ULONG		ulLineHeight;
  PSWENTRY	pSwEntry;
  RECTL		rect;
  BOOL		fTitles = FALSE;
  ULONG		ulIconSize = WinQuerySysValue( HWND_DESKTOP, SV_CXICON );
  ULONG		ulIconMiniSize = ulIconSize >> 1;
  ULONG		ulLeftColXRight = psizeWnd->cx;
  HPOINTER	hptrIcon = NULLHANDLE;

  if ( ( pidSelect == (PID)(-1) ) ||
       ( _findItem( &stList, pidSelect, &pstItem ) == DS_SEL_NONE ) )
    return;

  putilGetTextSize( hps, 2, "Em", &sizeEm );

  // Right column: Windows titles

  ulLineHeight = max( (sizeEm.cy << 1), (ulIconMiniSize + 1) );
  rect.xLeft = (psizeWnd->cx >> 1) + sizeEm.cx + ulIconMiniSize;
  rect.yBottom = psizeWnd->cy - ulLineHeight - (sizeEm.cy >> 1);
  rect.xRight = psizeWnd->cx;
  rect.yTop = rect.yBottom + ulLineHeight;

  for( ulIdx = 0; ulIdx < pstSwBlock->cswentry; ulIdx++ )
  {
    pSwEntry = &pstSwBlock->aswentry[ulIdx];
    if ( pSwEntry->swctl.idProcess != pstItem->stProcess.pid )
      continue;

    // If not found windows to list we will use this icon to display in
    // upper right corner.
    hptrIcon = pSwEntry->swctl.hwndIcon;
    if ( pSwEntry->swctl.uchVisibility != SWL_VISIBLE )
      continue;

    fTitles = TRUE;

    if ( hptrIcon == NULLHANDLE )
      hptrIcon = (HPOINTER)WinSendMsg( pSwEntry->swctl.hwnd,
                                                       WM_QUERYICON, 0, 0 );

    if ( hptrIcon != NULLHANDLE )
      WinDrawPointer( hps, rect.xLeft - ulIconMiniSize - (sizeEm.cx >> 1),
                      rect.yBottom + ( ulLineHeight - ulIconMiniSize ) / 2,
                      hptrIcon, DP_MINI );

    putilCharStringRect( hps, &rect, strlen( &pSwEntry->swctl.szSwtitle ),
                         &pSwEntry->swctl.szSwtitle,
                         SL_CSR_CUTFADE | SL_CSR_VCENTER );
    WinOffsetRect( hab, &rect, 0, -ulLineHeight );

    pt1.y -= ulLineHeight;
    if ( pt1.y < 0 )
      break;
  }

  if ( fTitles )
  {
    // Have windows list for process - right border for left column is half
    // of details window's width.
    ulLeftColXRight = psizeWnd->cx >> 1;
  }
  else if ( hptrIcon != NULLHANDLE )
  {
    // Have no list of windows for process but have icon - draw it at upper
    // right corner.
    ulLeftColXRight = psizeWnd->cx - ulIconSize;
    WinDrawPointer( hps, ulLeftColXRight, rect.yTop - ulIconSize,
                    hptrIcon, DP_NORMAL );
  }

  // Left column: Fields captions

  ulLineHeight = sizeEm.cy << 1;
  pt1.x = sizeEm.cx;
  pt1.y = psizeWnd->cy - (sizeEm.cy << 1);
  for( ulIdx = 0; ulIdx < ARRAYSIZE(aulDetailsStrId); ulIdx++ )
  {
    cbBuf = pstrLoad( hDSModule, aulDetailsStrId[ulIdx], sizeof(szBuf) - 1,
                      &szBuf );
    szBuf[cbBuf++] = ':';

    GpiCharStringAt( hps, &pt1, cbBuf, &szBuf );
    GpiQueryCurrentPosition( hps, &pt2 );
    if ( ulValXPos < pt2.x )
      ulValXPos = pt2.x;

    pt1.y -= ulLineHeight;
  }

  GpiSetColor( hps, 0x00000000 );

  rect.xLeft = ulValXPos + sizeEm.cx;
  rect.yBottom = psizeWnd->cy - (sizeEm.cy << 1);
  rect.xRight = ulLeftColXRight;
  rect.yTop = rect.yBottom + ulLineHeight;

  // File
  putilCharStringRect( hps, &rect, strlen( pstItem->pszPathName ),
                       pstItem->pszPathName, SL_CSR_CUTFADE );
  WinOffsetRect( hab, &rect, 0, -ulLineHeight );

  // PID
  cbBuf = sprintf( &szBuf, "0x%X (%u)", pstItem->stProcess.pid,
                   pstItem->stProcess.pid );
  putilCharStringRect( hps, &rect, cbBuf, &szBuf, SL_CSR_CUTFADE );
  WinOffsetRect( hab, &rect, 0, -ulLineHeight );

  // Type
  if ( ( pstItem->stProcess.ulType < ARRAYSIZE(aulProcTypesStrId) ) &&
       ( apszProcTypes[pstItem->stProcess.ulType] != NULL ) )
  {
    putilCharStringRect( hps, &rect,
                         strlen( apszProcTypes[pstItem->stProcess.ulType] ),
                         apszProcTypes[pstItem->stProcess.ulType],
                         SL_CSR_CUTFADE );
  }
  WinOffsetRect( hab, &rect, 0, -ulLineHeight );

  // Parent
  if ( pstItem->stProcess.pidParent != 0 )
  {
    if ( ( _findItem( &stList, pstItem->stProcess.pidParent, &pstParentItem )
           != DS_SEL_NONE ) )
    {
      cbBuf = sprintf( &szBuf, "%s (PID: 0x%X)", pstParentItem->pszName,
                       pstParentItem->stProcess.pid );
    }
    else
      cbBuf = sprintf( &szBuf, "PID: 0x%X", pstItem->stProcess.pidParent );

    putilCharStringRect( hps, &rect, cbBuf, &szBuf, SL_CSR_CUTFADE );
  }
  WinOffsetRect( hab, &rect, 0, -ulLineHeight );

  // Threads
  ultoa( pstItem->stProcess.cThreads, &szBuf, 10 );
  putilCharStringRect( hps, &rect, strlen( &szBuf ), &szBuf, SL_CSR_CUTFADE );
}

DSEXPORT VOID APIENTRY dsFillMenu(HWND hwndMenu, ULONG ulIndex)
{
  PPROCITEM	  	pstItem;
  MENUITEM		stMINew = { 0 };
  CHAR			szBuf[256];
  ULONG			cbBuf;
  CHAR			szPID[16];
  PSZ			apszVal[2];

  if ( ulIndex >= stList.ulCount )
    return;

  pstItem = stList.ppItems[ulIndex];
  if ( pstItem->pPRec == NULL )
    return;

  pidMenu = pstItem->stProcess.pid;

  stMINew.iPosition = MIT_END;
  stMINew.afStyle = MIS_SEPARATOR;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );
  stMINew.afStyle = MIS_TEXT;

  // Kill
  _snprintf( szPID, sizeof(szPID), "0x%X", pidMenu );
  apszVal[0] = pstItem->pszName;
  apszVal[1] = &szPID;
  cbBuf = putilLoadInsertStr( hDSModule, TRUE, IDS_MI_KILL,
                              2, &apszVal, sizeof(szBuf) - 1, &szBuf );
  if ( cbBuf != 0 )
  {
    szBuf[cbBuf] = '\0';
    stMINew.id = IDM_DSCMD_FIRST_ID;
    WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
  }

  if ( pstSwBlock != NULL )
  {
    ULONG	ulIdx;
    PTIB	ptib;
    PPIB	ppib;
    HWND	aHWnd[100];
    ULONG	cHWnd = 0;

    DosGetInfoBlocks( &ptib, &ppib );

    // Items for program's windows: "Close: <title>"

    cbBuf = pstrLoad( hDSModule, IDS_MI_CLOSE, sizeof(szBuf), &szBuf );
    stMINew.id = IDM_DSCMD_FIRST_ID + 100;

    for( ulIdx = 0; ulIdx < pstSwBlock->cswentry; ulIdx++ )
    {
      if ( ( pstSwBlock->aswentry[ulIdx].swctl.idProcess !=
             pstItem->stProcess.pid ) ||
           ( pstSwBlock->aswentry[ulIdx].swctl.uchVisibility != SWL_VISIBLE ) )
        continue;

      strlcpy( &szBuf[cbBuf], pstSwBlock->aswentry[ulIdx].swctl.szSwtitle,
               sizeof(szBuf) - cbBuf );
      WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
      stMINew.id++;

      aHWnd[cHWnd++] = pstSwBlock->aswentry[ulIdx].swctl.hwnd;
      if ( cHWnd == ARRAYSIZE(aHWnd) )
        break;
    }

    // Items for program's windows: "Switch to: <title>"

    if ( ppib->pib_ulpid != pstItem->stProcess.pid ) // is it me?
    {
      ULONG		ulItems = 0;

      cbBuf = pstrLoad( hDSModule, IDS_MI_SWITCHTO, sizeof(szBuf), &szBuf );
      stMINew.id = IDM_DSCMD_FIRST_ID + 200;

      for( ulIdx = 0; ulIdx < pstSwBlock->cswentry; ulIdx++ )
      {
        if ( ( pstSwBlock->aswentry[ulIdx].swctl.idProcess !=
               pstItem->stProcess.pid ) ||
             ( pstSwBlock->aswentry[ulIdx].swctl.uchVisibility != SWL_VISIBLE ) )
          continue;

        strlcpy( &szBuf[cbBuf], pstSwBlock->aswentry[ulIdx].swctl.szSwtitle,
                 sizeof(szBuf) - cbBuf );
        WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
        stMINew.id++;

        if ( ++ulItems == ARRAYSIZE(aHWnd) )
          break;
      }
    }

    if ( paMenuHWnd != NULL )
      debugFree( paMenuHWnd );

    cMenuHWnd = cHWnd;
    cHWnd *= sizeof(HWND);
    paMenuHWnd = debugMAlloc( cHWnd );
    memcpy( paMenuHWnd, &aHWnd, cHWnd );
  }
}

DSEXPORT ULONG APIENTRY dsCommand(HWND hwndOwner, USHORT usCommand)
{
  if ( ( usCommand >= (IDM_DSCMD_FIRST_ID + 100) ) &&
       ( usCommand < (IDM_DSCMD_FIRST_ID + 100 + cMenuHWnd) ) )
  {
    // One of items "Close: <title>" selected.

    PPROCITEM		pstItem;

    if ( _findItem( &stList, pidMenu, &pstItem ) != DS_SEL_NONE )
      _closeWindow( paMenuHWnd[usCommand - IDM_DSCMD_FIRST_ID - 100],
                    hwndOwner, pstItem->pszName );
  }
  else if ( ( usCommand >= (IDM_DSCMD_FIRST_ID + 200) ) &&
       ( usCommand < (IDM_DSCMD_FIRST_ID + 200 + cMenuHWnd) ) )
  {
    // One of items: "Switch to: <title>"

    _switchTo( paMenuHWnd[usCommand - IDM_DSCMD_FIRST_ID - 200] );
  }
  else if ( usCommand == IDM_DSCMD_FIRST_ID )
  {
    // Item "Kill"

    _killProcess( pidMenu, hwndOwner );
  }

  if ( paMenuHWnd != NULL )
    debugFree( paMenuHWnd );
  paMenuHWnd = NULL;
  cMenuHWnd = 0;

  return DS_UPD_NONE;
}

DSEXPORT ULONG APIENTRY dsEnter(HWND hwndOwner)
{
  showExplorer( hwndOwner, pidSelect );

  return DS_UPD_NONE;
}
