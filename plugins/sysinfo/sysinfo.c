#define INCL_DOSMISC
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
#include "sysinfo.h"

// Data for this module and properties dialog.

SYSVAR                 aSysVar[SYSVAR_COUNT] =
{
  { 1, "QSV_MAX_PATH_LENGTH",          // [bytes]
    "Maximum length of a path name.",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 2, "QSV_MAX_TEXT_SESSIONS",        // [numeric]
    "Maximum number of text sessions.",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 3, "QSV_MAX_PM_SESSIONS",          // [numeric]
    "Maximum number of PM sessions.",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 4, "QSV_MAX_VDM_SESSIONS",         // [numeric]
    "Maximum number of DOS sessions.",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 5, "QSV_BOOT_DRIVE",               // 1 - A, 2 - B, ...
    "Drive from which the system was started.",
    { SI_CONV_DRIVE, SI_CONV_DEFAULT, -1, -1, -1, -1 } },
  { 6, "QSV_DYN_PRI_VARIATION",        // 0 - absolute, 1 - dynamic priority.
    "Dynamic priority variation.",
    { SI_CONV_DYNPRIORITY, SI_CONV_DEFAULT, -1, -1, -1, -1 } },
  { 7, "QSV_MAX_WAIT",                 // [seconds]
    "Maximum wait.",
    { SI_CONV_SECHMS, SI_CONV_DEFAULT, -1, -1, -1, -1 } },
  { 8, "QSV_MIN_SLICE",                // [milliseconds]
    "Minimum time slice [milliseconds].",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 9, "QSV_MAX_SLICE",                // [milliseconds]
    "Maximum time slice [milliseconds].",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 10, "QSV_PAGE_SIZE", "Memory page size.",
    { SI_CONV_BYTESAUTO, SI_CONV_BYTES, SI_CONV_DEFAULT, -1, -1, -1 } },
  { 11, "QSV_VERSION_xxxxx",           // 11 - major, 12 - minor, 13 - revision
    "Version number.",
    { SI_CONV_VERNAME, SI_CONV_VERNUM, -1, -1, -1, -1 } },
  { 14, "QSV_MS_COUNT",                // [millisecond]
    "Millisecond counter.",
    { SI_CONV_MSECHMS, SI_CONV_DEFAULT, -1, -1, -1, -1 } },
  { 15, "QSV_TIME_xxxxx",              // [seconds] 15 - low, 16 - high
    "Time since January 1, 1970 at 0:00:00.",
    { SI_CONV_SECHMS, SI_CONV_DEFAULTLL, -1, -1, -1, -1 } },
  { 17, "QSV_TOTPHYSMEM",              // [bytes]
    "Physical memory in the system.",
    { SI_CONV_BYTESAUTO, SI_CONV_MB, SI_CONV_KB, SI_CONV_BYTES, SI_CONV_DEFAULT, -1 } },
  { 18, "QSV_TOTRESMEM",               // [bytes]
    "Resident memory in the system.",
    { SI_CONV_BYTESAUTO, SI_CONV_MB, SI_CONV_KB, SI_CONV_BYTES, SI_CONV_DEFAULT, -1 } },
  { 19, "QSV_TOTAVAILMEM",             // [bytes]
    "Maximum memory that can be allocated by all processes.",
    { SI_CONV_BYTESAUTO, SI_CONV_MB, SI_CONV_KB, SI_CONV_BYTES, SI_CONV_DEFAULT, -1 } },
  { 20, "QSV_MAXPRMEM ",               // [bytes]
    "Maximum memory that this process can allocate in its private arena.",
    { SI_CONV_BYTESAUTO, SI_CONV_MB, SI_CONV_KB, SI_CONV_BYTES, SI_CONV_DEFAULT, -1 } },
  { 21, "QSV_MAXSHMEM",                // [bytes]
    "Maximum memory that a process can allocate in the shared arena.",
    { SI_CONV_BYTESAUTO, SI_CONV_MB, SI_CONV_KB, SI_CONV_BYTES, SI_CONV_DEFAULT, -1 } },
  { 22, "QSV_TIMER_INTERVAL",          // [tenths of a millisecond]
    "Timer interval in tenths of a millisecond.",
    { SI_CONV_TNTHS_MSEC, SI_CONV_DEFAULT, -1, -1, -1, -1 } },
  { 23, "QSV_MAX_COMP_LENGTH",         // [bytes]
    "Maximum length of one component in a path name.",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 24, "QSV_FOREGROUND_FS_SESSION",
    "Session ID of the current foreground full-screen session.",
    { SI_CONV_HEX, SI_CONV_DEFAULT, -1, -1, -1, -1 } },
  { 25, "QSV_FOREGROUND_PROCESS",
    "Process ID of the current foreground process.",
    { SI_CONV_HEX, SI_CONV_DEFAULT, -1, -1, -1, -1 } },
  { 27, "QSV_MAXHPRMEM",
    "Maximum amount of free space in process's high private arena.",
    { SI_CONV_BYTESAUTO, SI_CONV_MB, SI_CONV_KB, SI_CONV_BYTES, SI_CONV_DEFAULT, -1 } },
  { 28, "QSV_MAXHSHMEM",
    "Maximum amount of free space in process's high shared arena.",
    { SI_CONV_BYTESAUTO, SI_CONV_MB, SI_CONV_KB, SI_CONV_BYTES, SI_CONV_DEFAULT, -1 } },
  { 29, "QSV_MAXPROCESSES",
    "Maximum number of concurrent processes supported.",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } },
  { 30, "QSV_VIRTUALADDRESSLIMIT", "Size of the user's address space.",
    { SI_CONV_VALAUTO, SI_CONV_VALMB, SI_CONV_DEFAULT, -1, -1, -1 } },
  { 33, "OS/4 Version", "OS/4 kernel svn revision.",
    { SI_CONV_DEFAULT, -1, -1, -1, -1, -1 } }
};

ULONG                  ulShowItems = SI_ITEMS_FULL;
ULONG                  ulInterval = 1;     // In seconds.
HMODULE                hDSModule;          // Module handle
HAB                    hab;
USERVAR                aUserVar[SYSVAR_COUNT] = { 0 };
ULONG                  cUserVar = 0;
ULONG                  aulData[QSV_MAX_OS4] = { 0 };

// Pointers to helper routines.
PHiniWriteULong        piniWriteULong;
PHiniWriteStr          piniWriteStr;
PHiniReadULong         piniReadULong;
PHiniReadStr           piniReadStr;
PHutilGetTextSize      putilGetTextSize;
PHutilCharStringRect   putilCharStringRect;
PHutil3DFrame          putil3DFrame;
PHutilMixRGB           putilMixRGB;
PHstrFromSec           pstrFromSec;
PHstrFromBytes         pstrFromBytes;
PHstrFromULL           pstrFromULL;
PHstrLoad              pstrLoad;
PHhelpShow             phelpShow;


LONG convDefault(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convMSecHMS(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convBytesAuto(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convDrive(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convDynPriority(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convSecHMS(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convVerNum(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convVerName(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convDefaultLL(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convKb(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convMb(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convTnthsMsec(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convHex(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convBytes(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convVALMb(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);
LONG convVALAuto(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);

// aValToStr[]. Index is SI_CONV_xxxxx
CONVVAL                aValToStr[CONV_COUNT] =
{
  "Numeric",      convDefault,             // SI_CONV_DEFAULT
  "Hour/Min/Sec", convMSecHMS,             // SI_CONV_MSECHMS
  "Auto",         convBytesAuto,           // SI_CONV_BYTESAUTO
  "Drive",        convDrive,               // SI_CONV_DRIVE ( 1-A, 2-B, ... )
  "Priority",     convDynPriority,         // SI_CONV_DYNPRIORITY
  "Hour/Min/Sec", convSecHMS,              // SI_CONV_SECHMS
  "Numeric",      convVerNum,              // SI_CONV_VERNUM
  "Name",         convVerName,             // SI_CONV_VERNAME
  "Numeric",      convDefaultLL,           // SI_CONV_DEFAULTLL
  "Kb",           convKb,                  // SI_CONV_KB
  "Mb",           convMb,                  // SI_CONV_MB
  "Milliseconds", convTnthsMsec,           // SI_CONV_TNTHS_MSEC
  "Hex",          convHex,                 // SI_CONV_HEX
  "Bytes",        convBytes,               // SI_CONV_BYTES
  "Mb",           convVALMb,               // SI_CONV_VALMB
  "Auto",         convVALAuto              // SI_CONV_VALAUTO
};

// Local data.

// Data source information.
static DSINFO stDSInfo =
{
  sizeof(DSINFO),                // Size, in bytes, of this data structure.
  "DosQuery~SysInfo()",          // Menu item title.
  0,                             // Number of "sort-by" strings.
  NULL,                          // "Sort-by" strings.
  DS_FL_NO_BG_UPDATES |
  DS_FL_ITEM_BG_LIST,            // Flags DS_FL_* (see ds.h)
  90,                            // Items horisontal space, %Em
  70,                            // Items vertical space, %Em
  20801                          // Help main panel index.
};

static PSIZEL   psizeEm;
static ULONG    ulCtxMenuItem;


// Convert value to string.
// ------------------------

static LONG _convBytes(ULONG ulValue, PSZ pszEU, ULONG cbBuf, PCHAR pcBuf)
{
  ULLONG     ullVal = ulValue;
  CHAR       acVal[24];

  pstrFromULL( &acVal, sizeof(acVal), ullVal );
  return _snprintf( pcBuf, cbBuf, "%s %s", &acVal, pszEU );
}

LONG convDefault(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _snprintf( pcBuf, cbBuf, "%u", *pulValue );
}

LONG convMSecHMS(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return pstrFromSec( (*pulValue) / 1000, cbBuf, pcBuf );
}

LONG convBytesAuto(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  ULLONG     ullVal = *pulValue;

  return pstrFromBytes( ullVal, cbBuf, pcBuf, FALSE );
}

LONG convDrive(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _snprintf( pcBuf, cbBuf, "%c:", 'A' - 1 + *pulValue );
}

LONG convDynPriority(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _snprintf( pcBuf, cbBuf, "%s",
                    *pulValue == 0 ? "Absolute" : "Dynamic" );
}

LONG convSecHMS(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return pstrFromSec( *pulValue, cbBuf, pcBuf );
}

LONG convVerNum(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _snprintf( pcBuf, cbBuf, "%u %u %u",
                    pulValue[0], pulValue[1], pulValue[2] );
}

LONG convVerName(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  PSZ        pszVer = NULL;

  if ( pulValue[0] == 20 && pulValue[2] == 0 )
  {
    switch( pulValue[1] )
    {
      case 00: pszVer = "2.0";  break;
      case 10: pszVer = "2.1";  break;
      case 11: pszVer = "2.11"; break;
      case 30: pszVer = "3.0";  break;
      case 40: pszVer = "4.0";  break;
      case 45: pszVer = "4.5";  break;
    }

    if ( pszVer != NULL )
      return _snprintf( pcBuf, cbBuf, "OS/2 %s", pszVer );
  }

  return convVerNum( pulValue, cbBuf, pcBuf );
}

LONG convDefaultLL(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _snprintf( pcBuf, cbBuf, "%llu", *(unsigned long long *)pulValue );
}

LONG convKb(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _convBytes( *pulValue / 1024, "Kb", cbBuf, pcBuf );
}

LONG convMb(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _convBytes( *pulValue / 1024 / 1024, "Mb", cbBuf, pcBuf );
}

LONG convTnthsMsec(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  ULONG      ulTnths = *pulValue % 10;
  ULONG      ulMsec  = *pulValue / 10;

  if ( ulTnths == 0 )
    return _snprintf( pcBuf, cbBuf, "%u msec.", ulMsec );

  return _snprintf( pcBuf, cbBuf, "%u.%u msec.", ulMsec, ulTnths );
}

LONG convHex(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _snprintf( pcBuf, cbBuf, "0x%X", *pulValue );
}

LONG convBytes(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _convBytes( *pulValue, "B", cbBuf, pcBuf );
}

LONG convVALMb(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  return _convBytes( *pulValue, "Mb", cbBuf, pcBuf );
}

LONG convVALAuto(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf)
{
  ULLONG     ullVal = *pulValue;

  ullVal *= ( 1024 * 1024 );
  return pstrFromBytes( ullVal, cbBuf, pcBuf, FALSE );
}


// Interface routines of data source.
// ----------------------------------


// PDSINFO dsInstall(HMODULE hMod, PSLINFO pSLInfo)
//
// This function is called once, immediately after module is loaded.
// hMod: Handle for the dynamic link module of data source.
// pSLInfo: Program information, see ..\..\sl\ds.h
// Returns pointer to the DSINFO structure.

DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  PHutilGetEmSize	putilGetEmSize;

  debugInit();

  // Store module handle of data source
  hDSModule = hMod;
  // Store anchor block handle.
  hab = pSLInfo->hab;

  // Query pointers to helper routines.

  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniWriteStr = (PHiniWriteStr)pSLInfo->slQueryHelper( "iniWriteStr" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  piniReadStr = (PHiniReadStr)pSLInfo->slQueryHelper( "iniReadStr" );
  putilGetTextSize = (PHutilGetTextSize)pSLInfo->slQueryHelper( "utilGetTextSize" );
  putilCharStringRect = (PHutilCharStringRect)pSLInfo->slQueryHelper( "utilCharStringRect" );
  putil3DFrame = (PHutil3DFrame)pSLInfo->slQueryHelper( "util3DFrame" );
  putilMixRGB = (PHutilMixRGB)pSLInfo->slQueryHelper( "utilMixRGB" );
  pstrFromSec = (PHstrFromSec)pSLInfo->slQueryHelper( "strFromSec" );
  pstrFromBytes = (PHstrFromBytes)pSLInfo->slQueryHelper( "strFromBytes" );
  pstrFromULL = (PHstrFromULL)pSLInfo->slQueryHelper( "strFromULL" );
  phelpShow = (PHhelpShow)pSLInfo->slQueryHelper( "helpShow" );
  pstrLoad = (PHstrLoad)pSLInfo->slQueryHelper( "strLoad" );
  putilGetEmSize = (PHutilGetEmSize)pSLInfo->slQueryHelper( "utilGetEmSize" );
  psizeEm = putilGetEmSize( hMod );

  // Return data source information to the main program.
  return &stDSInfo;
}

DSEXPORT VOID APIENTRY dsUninstall()
{
  debugDone();
}


// BOOL dsInit()
//
// This function is called after dsInstall() and every time when user turns
// data source (in "Settings").

DSEXPORT BOOL APIENTRY dsInit()
{
  ULONG      ulIdx;
  CHAR       acBuf[24];
  PCHAR      pcBuf;
  PUSERVAR   pUserVar;
  CHAR       acComment[640];

  // Read properties from the ini-file.

  cUserVar = piniReadULong( hDSModule, "VarCount", 0 );
  for( ulIdx = 0; ulIdx < cUserVar; ulIdx++ )
  {
    pUserVar = &aUserVar[ulIdx];
    pcBuf = &acBuf[ sprintf( &acBuf, "Var%u", ulIdx ) ];

    strcpy( pcBuf, "Id" );
    pUserVar->ulSysVarId = piniReadULong( hDSModule, &acBuf, 0 );

    strcpy( pcBuf, "Comment" );
    pUserVar->pszComment =
      debugStrDup( piniReadStr( hDSModule, &acBuf, &acComment,
                                           sizeof(acComment), NULL ) != 0 ?
                     &acComment : aSysVar[pUserVar->ulSysVarId].pszComment );

    strcpy( pcBuf, "Conv" );
    pUserVar->ulConvFunc = piniReadULong( hDSModule, &acBuf, 0 );
  }

  ulShowItems = piniReadULong( hDSModule, "ItemsStyle", SI_ITEMS_FULL );
  ulInterval = piniReadULong( hDSModule, "UpdateInterval", 1 );

  if ( cUserVar == 0 )
  {
    // No items - make default list.

    // QSV_MAXPRMEM
    aUserVar[0].ulSysVarId = 16;
    aUserVar[0].pszComment = debugStrDup( aSysVar[16].pszComment );
    aUserVar[0].ulConvFunc = aSysVar[16].aConv[0];
    // QSV_MAXSHMEM
    aUserVar[1].ulSysVarId = 17;
    aUserVar[1].pszComment = debugStrDup( aSysVar[17].pszComment );
    aUserVar[1].ulConvFunc = aSysVar[17].aConv[0];
    // QSV_MAXHPRMEM
    aUserVar[2].ulSysVarId = 22;
    aUserVar[2].pszComment = debugStrDup( aSysVar[22].pszComment );
    aUserVar[2].ulConvFunc = aSysVar[22].aConv[0];
    // QSV_MAXHSHMEM
    aUserVar[3].ulSysVarId = 23;
    aUserVar[3].pszComment = debugStrDup( aSysVar[23].pszComment );
    aUserVar[3].ulConvFunc = aSysVar[23].aConv[0];

    cUserVar = 4;
  }

  return TRUE;
}


// VOID dsDone()
//
// This function is called when user turns off the data source or exits the
// program.

DSEXPORT VOID APIENTRY dsDone()
{
  ULONG      ulIdx;
  PUSERVAR   pUserVar;

  for( ulIdx = 0; ulIdx < cUserVar; ulIdx++ )
  {
    pUserVar = &aUserVar[ulIdx];

    if ( pUserVar->pszComment != NULL )
      debugFree( pUserVar->pszComment );
  }
}


// ULONG dsGetCount()
//
// The program calls this function to query number of items.

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return cUserVar;
}


// ULONG dsGetUpdateDelay()
//
// The program calls this function to query time intervals for updates.
// This function must take a minimum of time. If your module uses threads
// you should use lock with helpers updateLock()/updateUnlock() to access the
// data which changes by thread and uses in this function (to avoid calls
// dsGetUpdateDelay() and dsUpdate()). Functions dsGetUpdateDelay() and
// dsUpdate() will not be called during execution of other module's interface
// functions, except dsLoadDlg(), dsEnter() and dsCommand().
// dsGetUpdateDelay() calls rather frequent, it allows you to change intervals
// dynamically.

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return ulInterval * 1000;
}


// ULONG dsUpdate(ULONG ulTime)
//
// Function will be called with an interval that is set dsGetUpdateDelay().
// This function must take a minimum of time. If your module uses threads
// you should use lock with helpers updateLock()/updateUnlock() to access the
// data which changes by thread and uses in this function (to avoid calls
// dsGetUpdateDelay() and dsUpdate()). Functions dsGetUpdateDelay() and
// dsUpdate() will not be called during execution of other module's interface
// functions, except dsLoadDlg().
// Return values:
//   DS_UPD_NONE - Displayed data is not changed.
//   DS_UPD_DATA - Displayed data is changed. Size of items windows, order and
//                 number of items have not changed. SL will update items and
//                 details.
//   DS_UPD_LIST - Size of items windows, order or number of items have been
//                 changed. SL will recreate items windows and update details.

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  ULONG      ulRC;

  ulRC = DosQuerySysInfo( 1, QSV_MAX_OS4, (PVOID)aulData, sizeof(aulData) );
  if ( ulRC != NO_ERROR )
  {
    ulRC = DosQuerySysInfo( 1, QSV_MAX, (PVOID)aulData, sizeof(aulData) );
    if ( ulRC != NO_ERROR )
      debug( "DosQuerySysInfo(), rc = %u", ulRC );
  }

  return DS_UPD_DATA;
}


// VOID dsSetWndStart(HPS hps, PSIZEL pSize)
//
// The program calls this function before creating windows for items, to query
// size for all item's windows.

DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  ULONG         ulIdx;
  SIZEL	        size;
  PUSERVAR      pUserVar = aUserVar;
  PSYSVAR       pSysVar;
  PFNVALTOSTR   pfnValToStr;
  CHAR          acBuf[32];
  ULONG         cbBuf;
  ULONG         ulHeight, ulCommentWidth = 0, ulNameWidth = 0, ulValWidth = 0;
  ULONG         aUl[3] = { ~0 };

  // Scan all items and find largest size for the item.

  for( ulIdx = 0; ulIdx < cUserVar; ulIdx++, pUserVar++ )
  {
    pSysVar  = &aSysVar[ pUserVar->ulSysVarId ];
    pfnValToStr = aValToStr[ pUserVar->ulConvFunc ].pFn;

    if ( ulShowItems != SI_ITEMS_COMMENT )
    {
      putilGetTextSize( hps, strlen( pSysVar->pszName ), pSysVar->pszName, &size );
      ulHeight = size.cy;
      if ( ulNameWidth < size.cx )
        ulNameWidth = size.cx;
    }
    else
      ulHeight = 0;

    cbBuf = pfnValToStr( (PULONG)&aUl[0] /*&aulData[ pSysVar->ulOrdinal - 1 ]*/,
                         sizeof(acBuf), &acBuf );
    putilGetTextSize( hps, cbBuf, &acBuf, &size );
    if ( ulValWidth < size.cx )
      ulValWidth = size.cx;

    if ( ulShowItems != SI_ITEMS_NAME )
    {
      putilGetTextSize( hps, strlen( pUserVar->pszComment ),
                        pUserVar->pszComment, &size );
      ulHeight += size.cy;
      if ( ulCommentWidth < size.cx )
        ulCommentWidth = size.cx;
    }

    if ( ulHeight > pSize->cy )
      pSize->cy = ulHeight;
  }

  if ( ulShowItems == SI_ITEMS_COMMENT )
    pSize->cx = ulCommentWidth + ulValWidth + psizeEm->cx;
  else
  {
    ulNameWidth += ulValWidth + psizeEm->cx;
    ulCommentWidth += psizeEm->cx / 2;
    pSize->cx = max( ulNameWidth, ulCommentWidth );
  }

  pSize->cy = max( ulShowItems == SI_ITEMS_FULL
                     ? ( psizeEm->cy * 3 ) : ( psizeEm->cy + psizeEm->cy / 2 ),
                   ulHeight );
}


// VOID dsPaintItem(ULONG ulIndex, HPS hps, SIZEL sizeWnd)
//
// This function calls when a window of item needs repainting. Module MAY not
// paint over whole area. The background is painted already in the right color
// (for selected or not selected item).
// 
// ulIndex: Index of the item to be paint.
// hps: Presentation space handle.
// sizeWnd: Size of item's window.

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  POINTL               pt = { 5, 5 };
  PUSERVAR             pUserVar = &aUserVar[ulIndex];
  PSYSVAR              pSysVar  = &aSysVar[ pUserVar->ulSysVarId ];
  PFNVALTOSTR          pfnValToStr = aValToStr[ pUserVar->ulConvFunc ].pFn;
  SIZEL                size;
  CHAR                 acBuf[32];
  RECTL                rect;
  ULONG                ulVarX;
  LONG                 lColor = GpiQueryColor( hps );
  LONG                 lBackColor = GpiQueryBackColor( hps );
  PSZ                  pszFirstLine = ulShowItems == SI_ITEMS_COMMENT &&
                                      pUserVar->pszComment != NULL &&
                                      *pUserVar->pszComment != '\0'
                                        ? pUserVar->pszComment
                                        : pSysVar->pszName;
  ULONG                cbLine = strlen( pszFirstLine );

  // First line: constant name or comment.

  putilGetTextSize( hps, cbLine, pszFirstLine, &size );
  pt.x = 0;
  pt.y = psizeWnd->cy - size.cy;
  GpiCharStringAt( hps, &pt, cbLine, pszFirstLine );

  // "....."

  // Get system variable position.
  cbLine = pfnValToStr( &aulData[ pSysVar->ulOrdinal - 1 ],
                        sizeof(acBuf), &acBuf );
  putilGetTextSize( hps, cbLine, &acBuf, &size );
  ulVarX = psizeWnd->cx - size.cx;

  // Draw dots.
  GpiSetColor( hps, putilMixRGB( lColor, lBackColor, 95 ) );
  do
  {
    GpiCharString( hps, 1, "." );
    GpiQueryCurrentPosition( hps, &pt );
  }
  while( pt.x < ( ulVarX - (psizeEm->cx / 2) ) );

  // First line: system variable value.

  GpiSetColor( hps, lColor );
  pt.x = ulVarX;
  GpiCharStringAt( hps, &pt, cbLine, &acBuf );

  // Second line: comment.

  if ( ulShowItems == SI_ITEMS_FULL )
  {
    cbLine = strlen( pUserVar->pszComment );
    putilGetTextSize( hps, cbLine, pUserVar->pszComment, &size );

    GpiSetColor( hps, putilMixRGB( lColor, lBackColor, 75 ) );
    rect.xLeft = 0;
    rect.yBottom = 0;
    rect.xRight = psizeWnd->cx;
    rect.yTop = pt.y;
    putilCharStringRect( hps, &rect, cbLine, pUserVar->pszComment,
                         SL_CSR_CUTFADE | SL_CSR_VCENTER );
    GpiSetColor( hps, lColor );
  }
}

DSEXPORT VOID APIENTRY dsFillMenu(HWND hwndMenu, ULONG ulIndex)
{
  MENUITEM   stMINew = { 0 };
  CHAR       szBuf[64];

  if ( ulIndex == DS_SEL_NONE )
    return;

  // Separator
  stMINew.iPosition = MIT_END;
  stMINew.afStyle = MIS_SEPARATOR;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );

  // Item "Copy name".
  pstrLoad( hDSModule, IDS_COPY_NAME, sizeof(szBuf), &szBuf );
  stMINew.afStyle = MIS_TEXT;
  stMINew.id = IDM_DSCMD_FIRST_ID;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  // Item "Copy value".
  pstrLoad( hDSModule, IDS_COPY_VALUE, sizeof(szBuf), &szBuf );
  stMINew.id = IDM_DSCMD_FIRST_ID + 1;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  ulCtxMenuItem = ulIndex;
}

DSEXPORT ULONG APIENTRY dsCommand(HWND hwndOwner, USHORT usCommand)
{
  PUSERVAR   pUserVar;
  PSYSVAR    pSysVar;
  PSZ        pszCopyStr;
  CHAR       acBuf[32];
  PSZ        pszClipboard;
  ULONG      ulRC;
  BOOL       fSuccess;

  // User has selected one of "Copy ???" items.

  if ( ulCtxMenuItem >= cUserVar )
    return DS_UPD_NONE;

  pUserVar = &aUserVar[ulCtxMenuItem];
  pSysVar = &aSysVar[ pUserVar->ulSysVarId ];

  // Detect source string.
  if ( usCommand == IDM_DSCMD_FIRST_ID )
  {
    // Item "Copy name" was selected.
    pszCopyStr = pSysVar->pszName;
  }
  else
  {
    // Item "Copy value" was selected.
    PFNVALTOSTR        pfnValToStr = aValToStr[ pUserVar->ulConvFunc ].pFn;

    pfnValToStr( &aulData[ pSysVar->ulOrdinal - 1 ], sizeof(acBuf), &acBuf );
    pszCopyStr = &acBuf;
  }

  // Copy string to the clipboard.

  ulRC = DosAllocSharedMem( (PPVOID)&pszClipboard, 0, strlen( pszCopyStr ) + 1,
                            PAG_COMMIT | PAG_READ | PAG_WRITE |
                            OBJ_GIVEABLE | OBJ_GETTABLE | OBJ_TILE );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosAllocSharedMem() failed, rc = %u", ulRC );
    return DS_UPD_NONE;
  }
  strcpy( pszClipboard, pszCopyStr );

  if ( !WinOpenClipbrd( hab ) )
  {
    debug( "WinOpenClipbrd() failed" );
    fSuccess = FALSE;
  }
  else
  {    
    WinEmptyClipbrd( hab );

    fSuccess = WinSetClipbrdData( hab, (ULONG)pszClipboard, CF_TEXT,
                                  CFI_POINTER );
    if ( !fSuccess )
      debug( "WinOpenClipbrd() failed" );

    WinCloseClipbrd( hab );
  }

  if ( !fSuccess )
    DosFreeMem( pszClipboard );

  return DS_UPD_NONE;
}
