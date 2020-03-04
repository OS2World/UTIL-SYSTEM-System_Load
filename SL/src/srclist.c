#define INCL_WIN
#define INCL_GPI
#define INCL_DOSFILEMGR   /* File Manager values */
#define INCL_DOSERRORS    /* DOS error values */
#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC		/* DOS Miscellaneous values  */
#include <os2.h>
#include "sl.h"
#include "srclist.h"
#include "utils.h"
#include "ctrl.h"
#include <stdlib.h>
#include <string.h>
#include "debug.h"     // Must be the last.

extern HINI            hIni;	// sl.c
extern HAB             hab;	// sl.c

static PDATASOURCE     *ppDataSrc = NULL;
static ULONG           cDataSrc = 0;
static ULONG           ulMaxDataSrc = 0;

// Mutex sem. hmtxUpdate for locking/unlocking update thread in items.c from
// datasoures via helpers functions
extern HMTX            hmtxUpdate;

BOOL dshIniWriteLong(HMODULE hMod, PSZ pszKey, LONG lData);
BOOL dshIniWriteULong(HMODULE hMod, PSZ pszKey, ULONG ulData);
BOOL dshIniWriteStr(HMODULE hMod, PSZ pszKey, PSZ pszData);
BOOL dshIniWriteData(HMODULE hMod, PSZ pszKey, PVOID pBuf, ULONG cbBuf);
LONG dshIniReadLong(HMODULE hMod, PSZ pszKey, LONG lDefault);
ULONG dshIniReadULong(HMODULE hMod, PSZ pszKey, ULONG ulDefault);
ULONG dshIniReadStr(HMODULE hMod, PSZ pszKey, PCHAR pcBuf, ULONG cbBuf,
                    PSZ pszDefault);
BOOL dshIniReadData(HMODULE hMod, PSZ pszKey, PVOID pBuf, PULONG pcbBuf);
BOOL dshIniGetSize(HMODULE hMod, PSZ pszKey, PULONG pcbData);
VOID dshUpdateLock();
VOID dshUpdateUnlock();
VOID dshUpdateForce(HMODULE hMod);
PSIZEL dshGetEmSize(HMODULE hMod);
LONG dshGetColor(HMODULE hMod, ULONG ulColor);
VOID dshHelpShow(HMODULE hMod, ULONG ulIndex);

static PFN slQueryHelper(PSZ pszFunc);

typedef struct _HELPERITEM {
  PSZ		pszName;
  PFN		pfnFunc;
} HELPERITEM, *PHELPERITEM;

// System Load information for data sources.

static SLINFO		stSlInfo =
{
  VER_MAJOR,		// ulVerMajor
  VER_MINOR,		// ulVerMinor
  VER_REVISION,		// ulVerRevison
  NULLHANDLE,		// HAB ( will be set in srclstInit() )
  slQueryHelper		// slQueryHelper
};

// Names and pointers to helper routines. This will scan by function
// slQueryHelper() called from data sources.

static HELPERITEM	aHelperList[] =
{
  // INI-file

  { "iniWriteLong", (PFN)dshIniWriteLong },			// srclist.c
  { "iniWriteULong", (PFN)dshIniWriteULong },			// srclist.c
  { "iniWriteStr", (PFN)dshIniWriteStr },			// srclist.c
  { "iniWriteData", (PFN)dshIniWriteData },			// srclist.c
  { "iniReadLong", (PFN)dshIniReadLong },			// srclist.c
  { "iniReadULong", (PFN)dshIniReadULong },			// srclist.c
  { "iniReadStr", (PFN)dshIniReadStr },				// srclist.c
  { "iniReadData", (PFN)dshIniReadData },			// srclist.c
  { "iniGetSize", (PFN)dshIniGetSize },				// srclist.c

  // Graph

  { "grInit", (PFN)grInit },					// graph.c
  { "grDone", (PFN)grDone },					// graph.c
  { "grSetTimeScale", (PFN)grSetTimeScale },			// graph.c
  { "grNewTimestamp", (PFN)grNewTimestamp },			// graph.c
  { "grGetTimestamp", (PFN)grGetTimestamp },			// graph.c
  { "grInitVal", (PFN)grInitVal },				// graph.c
  { "grDoneVal", (PFN)grDoneVal },				// graph.c
  { "grResetVal", (PFN)grResetVal },				// graph.c
  { "grSetValue", (PFN)grSetValue },				// graph.c
  { "grGetValue", (PFN)grGetValue },				// graph.c
  { "grDraw", (PFN)grDraw },					// graph.c

  // Utils

  { "utilGetTextSize", (PFN)utilGetTextSize },			// utils.c
  { "util3DFrame", (PFN)util3DFrame },				// utils.c
  { "utilBox", (PFN)utilBox },					// utils.c
  { "utilWriteResStr", (PFN)utilWriteResStr },			// utils.c
  { "utilWriteResStrAt", (PFN)utilWriteResStrAt },		// utils.c
  { "utilCharStringRect", (PFN)utilCharStringRect },		// utils.c
  { "utilSetFontFromPS", (PFN)utilSetFontFromPS },		// utils.c
  { "utilMixRGB", (PFN)utilMixRGB },				// utils.c
  { "utilGetEmSize", (PFN)dshGetEmSize },			// srclist.c
  { "utilGetColor", (PFN)dshGetColor },				// srclist.c
  { "utilLoadInsertStr", (PFN)utilLoadInsertStr },		// utils.c
  { "utilQueryProgPath", (PFN)utilQueryProgPath },		// utils.c

  // Lock/unlock updates

  { "updateLock", (PFN)dshUpdateLock },				// srclist.c
  { "updateUnlock", (PFN)dshUpdateUnlock },			// srclist.c
  { "updateForce", (PFN)dshUpdateForce },			// srclist.c

  // Strings

  { "strFromSec", (PFN)strFromSec },				// utils.c
  { "strFromBits", (PFN)strFromBits },				// utils.c
  { "strFromBytes", (PFN)strFromBytes },			// utils.c
  { "strLoad", (PFN)strLoad },					// utils.c
  { "strLoad2", (PFN)strLoad2 },				// utils.c
  { "strFromULL", (PFN)strFromULL },				// utils.c

  // Controls

  { "ctrlStaticToColorRect", (PFN)ctrlStaticToColorRect },	// ctrl.c
  { "ctrlDlgCenterOwner", (PFN)ctrlDlgCenterOwner },		// ctrl.c
  { "ctrlQueryText", (PFN)ctrlQueryText },			// ctrl.c
  { "ctrlSetDefaultFont", (PFN)ctrlSetDefaultFont },            // ctrl.c

  // Help

  { "helpShow", (PFN)dshHelpShow }				// srclist.c
};


static PFN slQueryHelper(PSZ pszFunc)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < sizeof(aHelperList) / sizeof(HELPERITEM); ulIdx++ )
    if ( stricmp( aHelperList[ulIdx].pszName, pszFunc ) == 0 )
      return aHelperList[ulIdx].pfnFunc;

  return NULL;
}

static VOID _initModule(PSZ pszFile, PSZ pszDefaultDSFont)
{
  CHAR		szBuf[256];
  ULONG		ulRC;
  PDATASOURCE	pDataSrc = calloc( 1, sizeof(DATASOURCE) );
  PCHAR		pCh, pcExt;
  PSZ		pszModule;
  HELPINIT	hInit = { 0 };
  FILESTATUS3L 	sInfo;

  if ( pDataSrc == NULL )
  {
    debugCP( "Not enough memory" );
    return;
  }

  debug( "Load module: %s", pszFile );
  ulRC = DosLoadModule( &szBuf, sizeof(szBuf), pszFile, &pDataSrc->hModule );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosLoadModule(,,\"%s\",), rc = %u", pszFile, ulRC );
    free( pDataSrc );
    return;
  }

  // Get name without path and extension
  pcExt = strrchr( pszFile, '\\' );
  pszModule = pcExt != NULL ? ( pcExt + 1 ) : pszFile;
  pcExt = strrchr( pszModule, '.' );
  if ( pcExt == NULL )
  {
    debugCP( "Given name has no extension" );
    DosFreeModule( pDataSrc->hModule );
    free( pDataSrc );
    return;
  }
  ulRC = min( pcExt - pszModule, sizeof(pDataSrc->szModule) - 1 );
  memcpy( &pDataSrc->szModule, pszModule, ulRC );
  pDataSrc->szModule[ulRC] = '\0';

  // Required entries

  if ( DosQueryProcAddr( pDataSrc->hModule, 0, "dsInstall",
                         (PFN *)&pDataSrc->fnInstall ) != NO_ERROR ||
       DosQueryProcAddr( pDataSrc->hModule, 0, "dsInit",
                         (PFN *)&pDataSrc->fnInit ) != NO_ERROR ||
       DosQueryProcAddr( pDataSrc->hModule, 0, "dsDone",
                         (PFN *)&pDataSrc->fnDone ) != NO_ERROR ||
       DosQueryProcAddr( pDataSrc->hModule, 0, "dsGetCount",
                         (PFN *)&pDataSrc->fnGetCount ) != NO_ERROR ||
       DosQueryProcAddr( pDataSrc->hModule, 0, "dsPaintItem",
                         (PFN *)&pDataSrc->fnPaintItem ) != NO_ERROR )
  {
    debug( "Module %s is not valid data source.", pszFile );
    DosFreeModule( pDataSrc->hModule );
    free( pDataSrc );
    return;
  }

  // Optional entries

  DosQueryProcAddr( pDataSrc->hModule, 0, "dsUninstall",
                    (PFN *)&pDataSrc->fnUninstall );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsGetUpdateDelay",
                    (PFN *)&pDataSrc->fnGetUpdateDelay );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsUpdate",
                    (PFN *)&pDataSrc->fnUpdate );

  DosQueryProcAddr( pDataSrc->hModule, 0, "dsSetSel",
                    (PFN *)&pDataSrc->fnSetSel );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsGetSel",
                    (PFN *)&pDataSrc->fnGetSel );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsIsSelected",
                    (PFN *)&pDataSrc->fnIsSelected );

  DosQueryProcAddr( pDataSrc->hModule, 0, "dsSetWndStart",
                    (PFN *)&pDataSrc->fnSetWndStart );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsSetWnd",
                    (PFN *)&pDataSrc->fnSetWnd );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsPaintDetails",
                    (PFN *)&pDataSrc->fnPaintDetails );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsGetHintText",
                    (PFN *)&pDataSrc->fnGetHintText );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsSortBy",
                    (PFN *)&pDataSrc->fnSortBy );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsLoadDlg",
                    (PFN *)&pDataSrc->fnLoadDlg );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsFillMenu",
                    (PFN *)&pDataSrc->fnFillMenu );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsCommand",
                    (PFN *)&pDataSrc->fnCommand );
  DosQueryProcAddr( pDataSrc->hModule, 0, "dsEnter",
                    (PFN *)&pDataSrc->fnEnter );


  // Add new data source to the list.
  // Add data source to list now to be able to call helpers from fnInstall().

  if ( cDataSrc == ulMaxDataSrc )
  {
    ULONG		ulMax = ulMaxDataSrc + 8;
    PDATASOURCE		*ppNewDataSrc = realloc( ppDataSrc,
                                                 ulMax * sizeof(PDATASOURCE) );

    if ( ppNewDataSrc == NULL )
    {
      debugCP( "Not enough memory" );
      DosFreeModule( pDataSrc->hModule );
      free( pDataSrc );
      return;
    }
    ppDataSrc = ppNewDataSrc;
    ulMaxDataSrc = ulMax;
  }

  ppDataSrc[cDataSrc] = pDataSrc;
  cDataSrc++;

  // Installation

  pDataSrc->pDSInfo = pDataSrc->fnInstall( pDataSrc->hModule, &stSlInfo );
  if ( ( pDataSrc->pDSInfo == NULL ) ||
       ( pDataSrc->pDSInfo->cbDSInfo < sizeof(DSINFO) ) )
  {
    debug( "Data source %s not installed", pDataSrc->szModule );
    cDataSrc--;		// "Remove" data source from list.
    DosFreeModule( pDataSrc->hModule );
    free( pDataSrc );
    return;
  }

  // Set module title.

  do
  {
    if ( (pDataSrc->pDSInfo->ulFlags & DS_FL_RES_MENU_TITLE) != 0 )
    {
      if ( WinLoadString( hab, pDataSrc->hModule,
                          pDataSrc->pDSInfo->_title.ulResId,
                          sizeof(pDataSrc->szTitle), &pDataSrc->szTitle ) != 0 )
        break;

      debug( "Cannot load title string for module %s from resource (id: %u)",
             &pDataSrc->szModule, pDataSrc->pDSInfo->_title.ulResId );
      pCh = &pDataSrc->szModule;
    }
    else
      pCh = pDataSrc->pDSInfo->_title.pszStr;

    strlcpy( &pDataSrc->szTitle, pCh, sizeof(pDataSrc->szTitle) );
  }
  while( FALSE );

  // Initialization

  pDataSrc->fDisable = utilQueryProfileLong( hIni, &pDataSrc->szModule,
                                             INI_DSKEY_DISABLE, 0 );

  pDataSrc->fInitialized = !pDataSrc->fDisable && pDataSrc->fnInit();

  debug( "Data source %s and %sinitialized (hdl %u): %s",
         pDataSrc->fDisable ? "disabled" : "enabled",
         pDataSrc->fInitialized ? "" : "not ",
         pDataSrc->hModule, pDataSrc->szModule );

  // Default sort index for new (not ordered) data source is number of loaded
  // modules & 0x00010000. May be helpful to identify new data sources in
  // future: ulIndex & 0x00010000 != 0.
  pDataSrc->ulIndex = utilQueryProfileLong( hIni, &pDataSrc->szModule,
                                   INI_DSKEY_INDEX, 0x00010000 + cDataSrc );

  utilQueryProfileStr( hIni, &pDataSrc->szModule, INI_DSKEY_FONT,
                       pszDefaultDSFont, szBuf, sizeof(szBuf) );
  pDataSrc->pszFont = strdup( szBuf );

  pDataSrc->lListBackCol = utilQueryProfileLong( hIni, &pDataSrc->szModule,
                             INI_DSKEY_LISTBACKCOL, DEF_DSLISTBACKCOL );
  pDataSrc->lItemForeCol = utilQueryProfileLong( hIni, &pDataSrc->szModule,
                             INI_DSKEY_ITEMFORECOL, DEF_DSITEMFORECOL );
  pDataSrc->lItemBackCol = utilQueryProfileLong( hIni, &pDataSrc->szModule,
                             INI_DSKEY_ITEMBACKCOL, DEF_DSITEMBACKCOL );
  pDataSrc->lItemHlForeCol = utilQueryProfileLong( hIni, &pDataSrc->szModule,
                               INI_DSKEY_ITEMHLFORECOL, DEF_DSITEMHLFORECOL );
  pDataSrc->lItemHlBackCol = utilQueryProfileLong( hIni, &pDataSrc->szModule,
                               INI_DSKEY_ITEMHLBACKCOL, DEF_DSITEMHLBACKCOL );

  // Set help instance for data source (if file <module>.hlp exists).

  strcpy( pcExt, ".hlp" );
  ulRC = DosQueryPathInfo( pszFile, FIL_STANDARDL, &sInfo, sizeof(FILESTATUS3L) );
  if ( ulRC == NO_ERROR &&
       (sInfo.attrFile & (FILE_HIDDEN | FILE_DIRECTORY)) == 0 )
  {
    // Using data source's title for help window's title (w/o '~').
    strRemoveMnemonic( sizeof(szBuf), &szBuf, &pDataSrc->szTitle );

    hInit.cb = sizeof( hInit );
    hInit.pszHelpWindowTitle = &szBuf;
    hInit.fShowPanelId = CMIC_HIDE_PANEL_ID;
    hInit.pszHelpLibraryName = pszFile;
    pDataSrc->hwndHelp = WinCreateHelpInstance( hab, &hInit );
    debug( "Help instance for %s%s created", pDataSrc->szModule,
           pDataSrc->hwndHelp == NULLHANDLE ? " WAS NOT" : "" );
  }
}

static int _dsSortComp(const void *p1, const void *p2)
{
  PDATASOURCE	pDataSrc1 = *(PDATASOURCE *)p1;
  PDATASOURCE	pDataSrc2 = *(PDATASOURCE *)p2;

  return pDataSrc1->ulIndex - pDataSrc2->ulIndex;
}


PDATASOURCE _srclstGetByModHdl(HMODULE hModule)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cDataSrc; ulIdx++ )
  {
    if ( ppDataSrc[ulIdx]->hModule == hModule )
      return ppDataSrc[ulIdx];
  }

  return NULL;
}


VOID srclstInit(HAB hab)
{
  CHAR			szBuf[CCHMAXPATHCOMP + 64];
  ULONG			ulRC;
  HDIR			hDir = HDIR_CREATE;
  FILEFINDBUF3		stFind;
  ULONG			cFind = 1;
  ULONG			cbPath;
  PSZ			pszDefaultDSFont = utilGetDefaultFont( NULL );

  stSlInfo.hab = hab;

  cbPath = utilQueryProgPath( sizeof(szBuf), &szBuf );
  strcpy( &szBuf[cbPath], "*.dll" );

  // Find first *.DLL file
  debug( "Search plugins: %s", &szBuf );
  ulRC = DosFindFirst( &szBuf, &hDir, FILE_ARCHIVED | FILE_SYSTEM |
                       FILE_READONLY, &stFind, sizeof(stFind), &cFind,
                       FIL_STANDARD );
  if ( ulRC != NO_ERROR && ulRC != ERROR_NO_MORE_FILES )
  {
    debug( "DosFindFirst(), rc = %u", ulRC );
    return;
  }

  while( ulRC == NO_ERROR )
  {
    strcpy( &szBuf[cbPath], &stFind.achName );
    // Try to load module
    _initModule( &szBuf, pszDefaultDSFont );

    // Find next *.DLL file
    cFind = 1;
    ulRC = DosFindNext( hDir, &stFind, sizeof(stFind), &cFind );
  }
  if ( ulRC != ERROR_NO_MORE_FILES )
    debug( "DosFindNext(), rc = %u", ulRC );

  // Close handle to a find request
  DosFindClose( hDir );

  qsort( ppDataSrc, cDataSrc, sizeof(PDATASOURCE), _dsSortComp );
}

VOID srclstDone()
{
  ULONG			ulIdx;
  PDATASOURCE		pDataSrc;

  debug( "Unload %u modules.", cDataSrc );
  for( ulIdx = 0; ulIdx < cDataSrc; ulIdx++ )
  {
    pDataSrc = ppDataSrc[ulIdx];

    if ( pDataSrc->fInitialized )
    {
      debug( "Finalize %s...", &pDataSrc->szModule );
      pDataSrc->fnDone();
    }

    if ( pDataSrc->hwndHelp != NULLHANDLE )
      WinDestroyHelpInstance( pDataSrc->hwndHelp );

    if ( pDataSrc->pszFont != NULL )
      free( pDataSrc->pszFont );

    if ( pDataSrc->fnUninstall != NULL )
    {
      debug( "Uninstall %s...", &pDataSrc->szModule );
      pDataSrc->fnUninstall();
    }

    debug( "Free module %s...", &pDataSrc->szModule );
    DosFreeModule( pDataSrc->hModule );
    free( pDataSrc );
  }
  free( ppDataSrc );
  cDataSrc = 0;
  debugCP( "Done" );
}

PDATASOURCE srclstGetByMenuItemId(ULONG ulMenuItemId)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cDataSrc; ulIdx++ )
  {
    if ( ppDataSrc[ulIdx]->ulMenuItemId == ulMenuItemId )
      return ppDataSrc[ulIdx];
  }

  return NULL;
}

ULONG srclstGetCount()
{
  return cDataSrc;
}

PDATASOURCE srclstGetByIndex(ULONG ulIndex)
{
  return ulIndex >= cDataSrc ? NULL : ppDataSrc[ulIndex];
}

VOID srclstSetFont(PDATASOURCE pDataSrc, PSZ pszFont)
{
  pszFont = strdup( pszFont );
  if ( pszFont == NULL )
  {
    debugCP( "Not enough memory" );
    return;
  }

  if ( pDataSrc->pszFont != NULL )
    free( pDataSrc->pszFont );

  pDataSrc->pszFont = pszFont;
  utilWriteProfileStr( hIni, pDataSrc->szModule, INI_DSKEY_FONT, pszFont );
}

VOID srclstSetColor(PDATASOURCE pDataSrc, ULONG ulColorType, LONG lColor)
{
  PSZ		pszKey;

  switch( ulColorType )
  {
    case SLCT_LISTBACK:
      pszKey = INI_DSKEY_LISTBACKCOL;
      pDataSrc->lListBackCol = lColor;
      break;

    case SLCT_ITEMFORE:
      pszKey = INI_DSKEY_ITEMFORECOL;
      pDataSrc->lItemForeCol = lColor;
      break;

    case SLCT_ITEMBACK:
      pszKey = INI_DSKEY_ITEMBACKCOL;
      pDataSrc->lItemBackCol = lColor;
      break;

    case SLCT_ITEMHLFORE:
      pszKey = INI_DSKEY_ITEMHLFORECOL;
      pDataSrc->lItemHlForeCol = lColor;
      break;

    case SLCT_ITEMHLBACK:
      pszKey = INI_DSKEY_ITEMHLBACKCOL;
      pDataSrc->lItemHlBackCol = lColor;
      break;

    default:
      debug( "Invalid ulColorType: %u", ulColorType );
      return;
  }

  utilWriteProfileLong( hIni, pDataSrc->szModule, pszKey, lColor );
}

BOOL srclstMove(ULONG ulIndex, BOOL fForward)
{
  PDATASOURCE		pDataSrc;
  ULONG			ulNewIndex;

  if ( ( ulIndex >= cDataSrc ) || ( ulIndex == 0 && !fForward ) ||
       ( ulIndex == (cDataSrc - 1) && fForward ) )
    return FALSE;

  ulNewIndex = ulIndex + ( fForward ? 1 : -1 );

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  pDataSrc = ppDataSrc[ulNewIndex];
  ppDataSrc[ulNewIndex] = ppDataSrc[ulIndex];
  ppDataSrc[ulIndex] = pDataSrc;
  DosReleaseMutexSem( hmtxUpdate );
  return TRUE;
}

VOID srclstStoreOrder()
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cDataSrc; ulIdx++ )
    utilWriteProfileULong( hIni, ppDataSrc[ulIdx]->szModule, INI_DSKEY_INDEX,
                           ulIdx );
}

VOID srclstEnable(ULONG ulIndex, BOOL fEnable)
{
  PDATASOURCE		pDataSrc;

  if ( ulIndex >= cDataSrc )
    return;

  pDataSrc = ppDataSrc[ulIndex];

  if ( pDataSrc->fDisable == !fEnable )
    return;

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );

  pDataSrc->fDisable = !fEnable;
  if ( fEnable )
  {
    if ( !pDataSrc->fInitialized )
    {
      pDataSrc->fInitialized = pDataSrc->fnInit();

      if ( pDataSrc->fInitialized )
        DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &pDataSrc->ulLastUpdate,
                         sizeof(ULONG) );
    }
  }
  else if ( pDataSrc->fInitialized )
  {
    pDataSrc->fnDone();
    pDataSrc->fInitialized = FALSE;
  }
    
  DosReleaseMutexSem( hmtxUpdate );

  utilWriteProfileLong( hIni, pDataSrc->szModule, INI_DSKEY_DISABLE,
                        (LONG)pDataSrc->fDisable );
}

BOOL srclstIsEnabled(ULONG ulIndex)
{
  if ( ulIndex >= cDataSrc )
    return FALSE;

  return !ppDataSrc[ulIndex]->fDisable;
}


// Data sources helpers for ini-file

BOOL dshIniWriteLong(HMODULE hMod, PSZ pszKey, LONG lData)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
    return FALSE;

  return utilWriteProfileLong( hIni, pDataSrc->szModule, pszKey, lData);
}

BOOL dshIniWriteULong(HMODULE hMod, PSZ pszKey, ULONG ulData)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
    return FALSE;

  return utilWriteProfileULong( hIni, pDataSrc->szModule, pszKey, ulData);
}

BOOL dshIniWriteStr(HMODULE hMod, PSZ pszKey, PSZ pszData)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
    return FALSE;

  return utilWriteProfileStr( hIni, pDataSrc->szModule, pszKey, pszData );
}

BOOL dshIniWriteData(HMODULE hMod, PSZ pszKey, PVOID pBuf, ULONG cbBuf)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
    return FALSE;

  return utilWriteProfileData( hIni, pDataSrc->szModule, pszKey, pBuf, cbBuf);
}

LONG dshIniReadLong(HMODULE hMod, PSZ pszKey, LONG lDefault)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
    return lDefault;

  return utilQueryProfileLong( hIni, pDataSrc->szModule, pszKey, lDefault );
}

ULONG dshIniReadULong(HMODULE hMod, PSZ pszKey, ULONG ulDefault)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
    return ulDefault;

  return utilQueryProfileULong( hIni, pDataSrc->szModule, pszKey, ulDefault );
}

ULONG dshIniReadStr(HMODULE hMod, PSZ pszKey, PCHAR pcBuf, ULONG cbBuf,
                    PSZ pszDefault)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
  {
    if ( pszDefault == NULL )
      return 0;
    return strlcpy( pcBuf, pszDefault, cbBuf );
  }

  return utilQueryProfileStr( hIni, pDataSrc->szModule, pszKey, pszDefault,
                              pcBuf, cbBuf );
}

BOOL dshIniReadData(HMODULE hMod, PSZ pszKey, PVOID pBuf, PULONG pcbBuf)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  return pDataSrc == NULL ? FALSE :
         utilQueryProfileData( hIni, pDataSrc->szModule, pszKey, pBuf, pcbBuf );
}

BOOL dshIniGetSize(HMODULE hMod, PSZ pszKey, PULONG pcbData)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  return pDataSrc == NULL ? FALSE :
         utilQueryProfileSize( hIni, pDataSrc->szModule, pszKey, pcbData );
}

VOID dshUpdateLock()
{
  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
}

VOID dshUpdateUnlock()
{
  DosReleaseMutexSem( hmtxUpdate );
}

VOID dshUpdateForce(HMODULE hMod)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc == NULL )
    return;

  DosRequestMutexSem( hmtxUpdate, SEM_INDEFINITE_WAIT );
  pDataSrc->fForceUpdate = TRUE;
  DosReleaseMutexSem( hmtxUpdate );
}

PSIZEL dshGetEmSize(HMODULE hMod)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  return pDataSrc == NULL ? NULL : &pDataSrc->sizeEm;
}

// LONG dshGetColor(HMODULE hMod, ULONG ulColor)
//   ulColor: DS_COLOR_* (see ds.h)
//   Returns RGB color for ulColor or -1 on error,

LONG dshGetColor(HMODULE hMod, ULONG ulColor)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc != NULL )
  {
    switch( ulColor )
    {
      case DS_COLOR_LISTBG:
        return pDataSrc->lListBackCol;

      case DS_COLOR_ITEMFORE:
        return pDataSrc->lItemForeCol;

      case DS_COLOR_ITEMBACK:
        return pDataSrc->lItemBackCol;

      case DS_COLOR_ITEMHIFORE:
        return pDataSrc->lItemHlForeCol;

      case DS_COLOR_ITEMHIBACK:
        return pDataSrc->lItemHlBackCol;
    }
  }

  return -1;
}

VOID dshHelpShow(HMODULE hMod, ULONG ulIndex)
{
  PDATASOURCE		pDataSrc = _srclstGetByModHdl( hMod );

  if ( pDataSrc != NULL && pDataSrc->hwndHelp != NULLHANDLE )
    WinSendMsg( pDataSrc->hwndHelp, HM_DISPLAY_HELP,
                MPFROMLONG( MAKELONG( ulIndex, NULL ) ),
                MPFROMSHORT( HM_RESOURCEID ) );
}
