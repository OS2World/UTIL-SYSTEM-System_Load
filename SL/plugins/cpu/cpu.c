#define INCL_DOSMISC
#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_ERRORS
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSSPINLOCK
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ds.h>
#include <sl.h>
#include "cputemp.h"
#include "cpu.h"
#include "termzone.h"
#include <debug.h>

// CPU load bar
#define BAR_LINES		10
#define BARCLR_SHADOW		0x0030353A
#define BARCLR_LIGHT		0x00FFFFFF
#define BARCLR_BG		0x0090959A
#define BARCLR_1		0x0000E010
#define BARCLR_2		0x00E0EA00
#define BARCLR_3		0x00F00000

#define UPDATE_INTERVAL		1000		// msec.

PFNCPUTEMPQUERY         pfnCPUTempQuery = NULL; // cputemp.dll

HMODULE			hDSModule;	// Module handle.
HAB			hab;
// Default colors of CPU lines on graph.
LONG			aulDefColors[DEF_COLORS] =
  { DEF_COLOR_1, DEF_COLOR_2, DEF_COLOR_3, DEF_COLOR_4 };
// API functions (doscalls.dll).
ULONG			ulTimeWindow;	// Graph time window.
ULONG			cCPU = 0;
PCPU			pCPUList;	// CPU load data storages, last update
					// timestamps, loads values and states.
PSZ			pszTZPathname;  // ACPI pathname for Thermal Zone.
ULONG			ulTemperature;  // Current TZ t*10, (ULONG)(-1) - unknown
ULONG			ulPTMU;		// TZ_MU_CELSIUS / TZ_MU_FAHRENHEIT
BOOL			fRealFreq;	// Show real frequency.
GRAPH			stGraph;
PGRVALPARAM		pGrValParam;	// Graph paramethers for lines.
ULONG			ulSeparateRates;// Show separate rates user/IRQ loads
                                        // for CPUs: CPU_SR_*.
// ulLastSeparateRates - Value of ulSeparateRates after last update.
ULONG			ulLastSeparateRates;
static GRVAL		stGrIRQVal;	// Graph values storage for IRQ.
static HPOINTER		hIcon;		// CPU online icon handle.
static HPOINTER		hIconOff;	// CPU offline icon handle.
static PCPUCNT		pCPUCounters;	// New values of counters for update.
static ULLONG		ullCPU0Freq;	// Times per sec. detacted at last upd.
static PSZ		pszBrandStr;	// CPU brand string or vendor.
static PSIZEL		psizeEm;

static HMODULE          hCPUTemp;
static PCPUTEMP		paCPUTemp;
static ULONG		cCPUTempMax;
static ULONG		cCPUTemp;	// not 0 - CPU temperature. available.


// grValToStr() callback fn. for ordinate minimum and maximum captions.
static LONG grValToStr(ULONG ulVal, PCHAR pcBuf, ULONG cbBuf);

GRPARAM			stGrParam =	// Graph paramethers for lines.
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
  0,						// cParamVal
  NULL						// pParamVal
};

static ULONG		ulHPad;
static ULONG		ulVPad;
static ULONG		ulIconSize;
static ULONG		ulIconAreaCX;
static ULONG		ulLineHeight;
static ULONG		ulBarTextWidth;
static ULONG		ulBarWidth;
static ULONG		ulBarHeight;

// Data source information.
static DSINFO stDSInfo = {
  sizeof(DSINFO),		// Size of this data structure.
  (PSZ)IDS_DS_TITLE,		// Data source module's title (resource string
                    		// ID, see flag DS_FL_RES_MENU_TITLE).
  0,				// Number of "sort-by" strings.
  NULL,				// "Sort-by" strings.
  DS_FL_RES_MENU_TITLE,		// Flags DS_FL_*
  70,				// Items horisontal space, %Em
  70,				// Items vertical space, %Em
  20801				// Help main panel index (see cpu.ipf).
};

// Pointers to helper routines.

PHiniWriteStr			piniWriteStr;
PHiniWriteULong			piniWriteULong;
PHiniReadULong			piniReadULong;
PHiniReadStr			piniReadStr;
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
PHutil3DFrame			putil3DFrame;
PHutilBox			putilBox;
PHutilMixRGB			putilMixRGB;
PHutilLoadInsertStr		putilLoadInsertStr;
PHutilQueryProgPath		putilQueryProgPath;
PHupdateLock			pupdateLock;
PHupdateUnlock			pupdateUnlock;
PHstrFromSec			pstrFromSec;
PHstrFromBits			pstrFromBits;
PHstrLoad			pstrLoad;
PHstrLoad2			pstrLoad2;
PHctrlStaticToColorRect		pctrlStaticToColorRect;
PHctrlDlgCenterOwner		pctrlDlgCenterOwner;
PHctrlQueryText			pctrlQueryText;
PHhelpShow			phelpShow;

// BOOL cpuHaveCPUID()
// Return TRUE if CPUID allowed. https://ru.wikipedia.org/wiki/CPUID

BOOL cpuHaveCPUID();
#pragma aux cpuHaveCPUID = \
"    pushfd                " \
"    pop     eax           " \
"    mov     ebx, eax      " \
"    xor     eax, 200000h  " \
"    push    eax           " \
"    popfd                 " \
"    pushfd                " \
"    pop     eax           " \
"    xor     ecx, ecx      " \
"    xor     eax, ebx      " \
"    jz      no_cpuid      " \
"    mov     ecx, 1        " \
"no_cpuid:                 " \
  value [ecx] modify[eax ebx];

// PSZ cpuQueryVendorName()
// Returns pointer to CPU vendor name or vendor ID if name was not found.

PSZ cpuQueryVendorName()
{
  typedef struct _VENDORLISTITEM {
    CHAR		acId[12];
    PSZ		pszName;
  } VENDORLISTITEM, *PVENDORLISTITEM;

  static VENDORLISTITEM	astVendors[] = {
    { "GenuineIntel", "Intel" },
    { "AuthenticAMD", "AMD" },
    { "CyrixInstead", "Cyrix" },
    { "CentaurHauls", "Centaur" },
    { "SiS SiS SiS ", "SiS" },
    { "NexGenDriven", "NexGen" },
    { "GenuineTMx86", "Transmeta" },
    { "RiseRiseRise", "Rise" },
    { "UMC UMC UMC ", "UMC" },
    { "Geode by NSC", "National Semiconductor" },
    { "CyrixInstead", "Cyrix" },
    { "TransmetaCPU", "Transmeta" },
    { "GenuineTMx86", "Transmeta" },
    { "VIA VIA VIA ", "VIA" },
    { "Vortex86 SoC", "Vortex" },
    { "KVMKVMKVMKVM", "KVM" },
    { "Microsoft Hv", "Microsoft Hyper-V" }, // or Windows Virtual PC
    { "VMwareVMware", "VMware" },
    { "XenVMMXenVMM", "Xen HVM" },
    { "end-of-list-", NULL }
  };
  static CHAR		acVendorId[12] = "GenuineIntel";
  PVENDORLISTITEM	pItem = &astVendors;

  // Query vendor ID string.
  __asm {
    mov	    edi, offset(acVendorId)
    cld
    xor     eax, eax
    cpuid
    and     eax, 0FFFFFF00h  ; For early Intel P5 : EAX=0000_05xxh
    cmp     eax, 00000500h   ;   ( http://sandpile.org/x86/cpuid.htm ).
    je      no_data          ; Vendor ID str. not supported, old CPU - exit.
    mov     [edi], ebx       ; Store vendor ID string at acVendorId[], part 1
    mov     [edi+4], edx     ; part 2
    mov     [edi+8], ecx     ; part 3
no_data:
  }

  // Look for vender for returned id.
  do
  {
    if ( memcmp( &pItem->acId, &acVendorId, 12 ) == 0 )
      return pItem->pszName;

    pItem++;
  }
  while( pItem->pszName != NULL );

  // Vendor name not found in our table. Very bad, return "raw" string.
  acVendorId[12] = '\0';
  return &acVendorId;
}

PSZ cpuQueryBrandString()
{
  static CHAR		szBrandString[64] = { 0 };

  if ( szBrandString[0] == '\0' )
  {
    __asm {
      ; Test for early Intel P5

      xor     eax, eax         ; Get maximum supported standard level.
      cpuid
      and     eax, 0FFFFFF00h  ; For early Intel P5 : EAX=0000_05xxh
      cmp     eax, 00000500h   ;   ( http://sandpile.org/x86/cpuid.htm ).
      je      data_end         ; Extended cpuid not supported, old CPU - exit.

      ; Query brand string

      mov	    edi, offset(szBrandString)  ; Destination string buffer.
      cld
      mov     eax, 80000000h   ; Get highest function supported.
      cpuid
      mov     esi, eax         ; Store highest function supported to ESI.
      mov     eax, 80000002h   ; 80000002h..80000004h - brand string.
    next_part:
      cmp     eax, esi         ; Is function supported?
      ja      data_end
      push    eax
      cpuid                    ; Get part of brand string.
      stosd                    ; Store part (EAX:EBX:ECX:EDX) to the buffer.
      mov     eax, ebx
      stosd   
      mov     eax, ecx
      stosd   
      mov     eax, edx
      stosd   
      pop     eax
      inc     eax
      cmp     eax, 80000005h   ; Last function for brand string (have all parts)?
      jne     next_part
    data_end:

      ; Remove leading spaces
      mov     ecx, 64
      mov     edi, offset(szBrandString)
      mov     al, ' '
      repe scasb               ; Search first not space character.
      dec     edi              ; Return to found not space character.
      mov     esi, edi
      mov     edi, offset(szBrandString) ; Destination - begin of the buffer.
      mov     ecx, 63
      sub     ecx, esi
      add     ecx, edi
      rep movsb                ; Move string to the begin of buffer.
    }
  }

  return &szBrandString;
}


// grValToStr() callback fn. for pgrDraw(). This function should
// return the string at pcBuf for ordinate minimum and maximum captions.
// We use a constant string "0%" for minimum and "100%" for maximum.
static LONG grValToStr(ULONG ulVal, PCHAR pcBuf, ULONG cbBuf)
{
  if ( ulVal == 0 )
  {
    *((PUSHORT)pcBuf) = (USHORT)'%0';
    return 2;
  }

  *((PULONG)pcBuf) = (ULONG)'%001';
  return 4;
}



// Interface routines of data source
// ---------------------------------

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime);

DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  PHutilGetEmSize	putilGetEmSize;

  debugInit();

  // Store module handle of data source.
  hDSModule = hMod;
  // Store anchor block handle.
  hab = pSLInfo->hab;

  // Query pointers to helper routines.

  piniWriteStr = (PHiniWriteStr)pSLInfo->slQueryHelper( "iniWriteStr" );
  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  piniReadStr = (PHiniReadStr)pSLInfo->slQueryHelper( "iniReadStr" );
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
  putil3DFrame = (PHutil3DFrame)pSLInfo->slQueryHelper( "util3DFrame" );
  putilBox = (PHutilBox)pSLInfo->slQueryHelper( "utilBox" );
  putilMixRGB = (PHutilMixRGB)pSLInfo->slQueryHelper( "utilMixRGB" );
  putilLoadInsertStr = (PHutilLoadInsertStr)pSLInfo->slQueryHelper( "utilLoadInsertStr" );
  putilQueryProgPath = (PHutilQueryProgPath)pSLInfo->slQueryHelper( "utilQueryProgPath" );
  pupdateLock = (PHupdateLock)pSLInfo->slQueryHelper( "updateLock" );
  pupdateUnlock = (PHupdateUnlock)pSLInfo->slQueryHelper( "updateUnlock" );
  pstrFromSec = (PHstrFromSec)pSLInfo->slQueryHelper( "strFromSec" );
  pstrFromBits = (PHstrFromBits)pSLInfo->slQueryHelper( "strFromBits" );
  pstrLoad = (PHstrLoad)pSLInfo->slQueryHelper( "strLoad" );
  pstrLoad2 = (PHstrLoad2)pSLInfo->slQueryHelper( "strLoad2" );
  pctrlStaticToColorRect = (PHctrlStaticToColorRect)pSLInfo->slQueryHelper( "ctrlStaticToColorRect" );
  pctrlDlgCenterOwner = (PHctrlDlgCenterOwner)pSLInfo->slQueryHelper( "ctrlDlgCenterOwner" );
  pctrlQueryText = (PHctrlQueryText)pSLInfo->slQueryHelper( "ctrlQueryText" );
  phelpShow = (PHhelpShow)pSLInfo->slQueryHelper( "helpShow" );

  putilGetEmSize = (PHutilGetEmSize)pSLInfo->slQueryHelper( "utilGetEmSize" );
  psizeEm = putilGetEmSize( hMod );

  // Return data source information for main program
  return &stDSInfo;
}

DSEXPORT VOID APIENTRY dsUninstall()
{
  debugDone();
}

DSEXPORT BOOL APIENTRY dsInit()
{
  ULONG		ulRC;
  ULONG		ulTime;
  ULONG		ulIdx;
  CHAR		szBuf[128];

  // ACPI initialization - load acpi32.dll and version check.
  tzInit();

  // Load CPUTEMP.DLL.
  pfnCPUTempQuery = NULL;
  ulRC = DosLoadModule( szBuf, sizeof(szBuf), "CPUTEMP.DLL", &hCPUTemp );
  if ( ulRC != NO_ERROR )
  {
    debug( "CPUTEMP.DLL loading failed" );
    hCPUTemp = NULLHANDLE;
  }
  else
  {
    ulRC = DosQueryProcAddr( hCPUTemp, ORD_CPUTEMPQUERY, NULL,
                             (PFN *)&pfnCPUTempQuery );
    if ( ulRC != NO_ERROR )
    {
      debug( "Function CPUTEMP.DLL#%u does not exist", ORD_CPUTEMPQUERY );
      DosFreeModule( hCPUTemp );
      hCPUTemp = NULLHANDLE;
    }
  }

  paCPUTemp = NULL;
  cCPUTempMax = 0;
  cCPUTemp = 0;

  // Necessary initialization to obtain information about processors.
  ulRC = DosPerfSysCall( CMD_KI_ENABLE, 0, 0, 0 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosPerfSysCall(CMD_KI_ENABLE), rc = %u\n", ulRC );
    return FALSE;
  }

  // Request number of processors.
  ulRC = DosPerfSysCall( CMD_PERF_INFO, 0, (ULONG)&cCPU, 0 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosPerfSysCall(CMD_PERF_INFO), rc = %u\n", ulRC );
    return FALSE;
  }

  // Array of counters for DosPerfSysCall() calls.
  pCPUCounters = debugCAlloc( cCPU, sizeof(CPUCNT) );
  if ( pCPUCounters == NULL )
  {
    debug( "Not enough memory" );
    return FALSE;
  }

  // Allocate CPU records and additional record for interrupts.
  pCPUList = debugCAlloc( cCPU, sizeof(CPU) );
  if ( pCPUList == NULL )
  {
    debug( "Not enough memory" );
    debugFree( pCPUCounters );
    return FALSE;
  }

  // Graph initialization.

  ulTimeWindow = piniReadULong( hDSModule, "TimeWindow",
                                           DEF_TIMEWINDOW );
  if ( ulTimeWindow < 60 || ulTimeWindow > (60*5) )
    ulTimeWindow = DEF_TIMEWINDOW;

  // pGrValParam is the array of parameters for every single graph.
  pGrValParam = debugMAlloc( (cCPU + 1) * sizeof(GRVALPARAM) );
  if ( pGrValParam == NULL )
  {
    debug( "Not enough memory" );
    debugFree( pCPUCounters );
    return FALSE;
  }

  // Fill CPU load graph paramethers list.
  for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
  {
    pGrValParam[ulIdx].ulLineWidth = 2;
    sprintf( &szBuf, "CPU%uColor", ulIdx );
    pGrValParam[ulIdx].clrGraph =
      piniReadULong( hDSModule, &szBuf,
                                aulDefColors[ulIdx % DEF_COLORS] );
  }
  // Graph paramethers for graph of interrupts.
  pGrValParam[cCPU].ulLineWidth = 2;
  pGrValParam[cCPU].clrGraph =
    piniReadULong( hDSModule, "IRQColor", DEF_IRQ_COLOR );
  pGrValParam[cCPU].ulPlygonBright = 0;

  stGrParam.cParamVal = cCPU + 1;
  stGrParam.pParamVal = pGrValParam;
  stGrParam.clrBackground =
    piniReadULong( hDSModule, "GrBgColor", DEF_GRBGCOLOR );
  stGrParam.clrBorder =
    piniReadULong( hDSModule, "GrBorderColor", DEF_GRBORDERCOLOR );

  // Graph "base object" initialization.
  ulTime = ulTimeWindow * 1000;
  if ( !pgrInit( &stGraph, ulTime / UPDATE_INTERVAL, ulTime, 0,
                            1000 ) ) // max. 1000: 0..100% and tenths.
  {
    debug( "grInit() fail" );
    debugFree( pCPUCounters );
    debugFree( pCPUList );
    return FALSE;
  }

  // Graph's values data storages of CPUs initialization.
  for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
    pgrInitVal( &stGraph, &pCPUList[ulIdx].stGrVal );

  // Graph's values data storages of IRQ initialization.
  pgrInitVal( &stGraph, &stGrIRQVal );

  // Thermal Zone temperature.

  ulTemperature = (ULONG)(-1);
  pszTZPathname = NULL;
  // Set measurement units.
  ulPTMU = piniReadULong( hDSModule, "TMU", TZ_MU_CELSIUS );

  // Set pathname for ACPI table

  // Load pathname from the ini-file.
  if ( piniReadStr( hDSModule, "TZPathname", &szBuf,
                               sizeof(szBuf), NULL ) != 0 )
  {
    pszTZPathname = debugStrDup( &szBuf );

    // Trying to query CPU temperature.
    if ( tzQuery( &szBuf, ulPTMU, &ulTemperature ) != TZ_OK )
      debug( "Can't get Thermal Zone t. with pathname from ini-file: %s",
             &szBuf );
  }

  if ( ulTemperature == (ULONG)(-1) )
  {
    // Try pathnames from resource.

    for( ulIdx = IDS_PATHNAME_FIRST_ID; ; ulIdx++ ) 
    {
      if ( pstrLoad( hDSModule, ulIdx, sizeof(szBuf), &szBuf ) == 0
           || szBuf[0] != '\\' )
        break;

      if ( tzQuery( &szBuf, ulPTMU, &ulTemperature ) != TZ_OK )
        continue;

      // CPU t successfully determined. Use found pathname.
      if ( pszTZPathname != NULL )
        debugFree( pszTZPathname );
      pszTZPathname = debugStrDup( &szBuf );

      debug( "CPU t successfully determined with default pathname: %s",
             &szBuf );
      break;
    }
  }

  // CPU brand string.

  if ( cpuHaveCPUID() )
  {
    pszBrandStr = cpuQueryBrandString();

    if ( pszBrandStr[0] == '\0' )         // Brand string is not available -
      pszBrandStr = cpuQueryVendorName(); // use vendor name.
  }

  // Show real frequency flag.
  fRealFreq = (BOOL)piniReadULong( hDSModule, "realFreq",
                                   (ULONG)DEF_REALFREQ );
  // Show separate rates user/IRQ loads.
  ulSeparateRates = piniReadULong( hDSModule, "separateRates",
                                   DEF_SEPARATERATES );

  // Load CPU icons.
  hIcon = WinLoadPointer( HWND_DESKTOP, hDSModule, ID_ICON_CPU );
  hIconOff = WinLoadPointer( HWND_DESKTOP, hDSModule, ID_ICON_CPUOFF );

  // Initial query counters.
  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulTime, sizeof(ULONG) );
  dsUpdate( ulTime );

  return TRUE;
}

DSEXPORT VOID APIENTRY dsDone()
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
    pgrDoneVal( &pCPUList[ulIdx].stGrVal );

  pgrDoneVal( &stGrIRQVal );

  if ( pszTZPathname != NULL )
    debugFree( pszTZPathname );

  pgrDone( &stGraph );

  debugFree( pCPUCounters );
  debugFree( pCPUList );
  debugFree( pGrValParam );
  if ( paCPUTemp != NULL )
    debugFree( paCPUTemp );
  tzDone();

  if ( hCPUTemp != NULLHANDLE )
    DosFreeModule( hCPUTemp );

  WinDestroyPointer( hIcon );
  WinDestroyPointer( hIconOff );
}

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return UPDATE_INTERVAL;
}

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return cCPU;
}

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  ULONG		ulRC;
  BOOL		fInterval;
  ULONG		ulPrevTime;
  ULONG		ulIdx;
  ULLONG	ullDeltaTime;
  ULLONG	ullDeltaBusy;
  ULLONG	ullDeltaIntr;

  fInterval = pgrGetTimestamp( &stGraph, &ulPrevTime );
  if ( fInterval && ( (ulTime - ulPrevTime) < 100 ) )
    return DS_UPD_NONE;

  // Query CPU temperatures.
  while( TRUE )
  {
    if ( pfnCPUTempQuery != NULL )
    {
      ulRC = pfnCPUTempQuery( cCPUTempMax, paCPUTemp, &cCPUTemp );
      if ( ulRC == CPUTEMP_OK )
        break;

      if ( ulRC == CPUTEMP_OVERFLOW )
      {
        cCPUTempMax = cCPUTemp;
        if ( paCPUTemp != NULL )
          debugFree( paCPUTemp );
        paCPUTemp = debugMAlloc( sizeof(CPUTEMP) * cCPUTempMax );
        if ( paCPUTemp != NULL )
          continue;

        debug( "Not enough memory" );
      }
    }

    cCPUTempMax = 0;
    cCPUTemp = 0;
    break;
  }

  ulRC = DosPerfSysCall( CMD_KI_RDCNT, (ULONG)pCPUCounters, 0, 0 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosPerfSysCall(CMD_KI_RDCNT), rc = %u\n", ulRC );
    return DS_UPD_NONE;
  }

  // fInterval is FALSE - this is first call dsUpdate().
  pgrNewTimestamp( &stGraph, ulTime );

  // Calculations for CPU records.

  for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
  {
    if ( DosGetProcessorStatus( ulIdx+1, &pCPUList[ulIdx].fOnline ) != NO_ERROR )
      pCPUList[ulIdx].fOnline = TRUE;

    if ( fInterval )
    {
      // Have previous data - calculate and store cpu load.

      ullDeltaTime = pCPUCounters[ulIdx].ullTime - pCPUList[ulIdx].ullTime;
      ullDeltaBusy = pCPUCounters[ulIdx].ullBusy - pCPUList[ulIdx].ullBusy;
      ullDeltaIntr = pCPUCounters[ulIdx].ullIntr - pCPUList[ulIdx].ullIntr;

      if ( !pCPUList[ulIdx].fOnline || ( ullDeltaTime == 0 ) )
      {
        // WTF?! Sometimes ullDeltaTime is 0 (on IBM kernel ?) -=8( )
        debugCP( "ullDeltaTime is ZERO !" );
        pCPUList[ulIdx].ulLoadIntr = 0;
        pgrSetValue( &stGraph, &pCPUList[ulIdx].stGrVal, 0 );
      }
      else
      {
        // We measure the load in the range from 0 to 1000 to later convert the
        // values in tenths of a percent.

        // Interrupt load value.
        pCPUList[ulIdx].ulLoadIntr = (ULONG)(ullDeltaIntr * 1000 / ullDeltaTime);

        // Store load (busy+interrupt) value to storage.
        pgrSetValue( &stGraph, &pCPUList[ulIdx].stGrVal,
                  (ULONG)((ullDeltaBusy + ullDeltaIntr) * 1000 / ullDeltaTime) );
      }

      // Store load interrupt value from CPU0 to IRQ's data storage.
      if ( ulIdx == 0 )
      {
        pgrSetValue( &stGraph, &stGrIRQVal, pCPUList[0].ulLoadIntr );
        ullCPU0Freq = ( ullDeltaTime * 1000 ) / ( ulTime - ulPrevTime );
      }
    }
    else
      pCPUList[ulIdx].ulLoadIntr = 0;

    // Store new time, busy and interrupt counter values.
    pCPUList[ulIdx].ullTime = pCPUCounters[ulIdx].ullTime;
    pCPUList[ulIdx].ullBusy = pCPUCounters[ulIdx].ullBusy;
    pCPUList[ulIdx].ullIntr = pCPUCounters[ulIdx].ullIntr;
  }

  // Query Thermal Zone temperature via ACPI only if a previous attempt was
  // successful.
  if ( ulTemperature != (ULONG)(-1) &&
       tzQuery( pszTZPathname, ulPTMU, &ulTemperature ) != TZ_OK )
    ulTemperature = (ULONG)(-1);

  if ( ulLastSeparateRates != ulSeparateRates )
  {
    // Value of ulSeparateRates was changed by user (in the properties dialog).
    ulLastSeparateRates = ulSeparateRates;
    // Windows sizes must be changed - returns DS_UPD_LIST.
    return DS_UPD_LIST;
  }

  return fInterval ? DS_UPD_DATA : DS_UPD_LIST;
}

DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  SIZEL		sizeText;
  CHAR		szBuf[128];
  PCHAR		pcBuf = &szBuf;
  ULONG		cbBuf = sizeof(szBuf);

  ulHPad = (psizeEm->cx >> 1) + (psizeEm->cx >> 3);
  ulVPad = (psizeEm->cy >> 1) + (psizeEm->cy >> 2);

  ulIconSize = WinQuerySysValue( HWND_DESKTOP, SV_CXICON );

  cbBuf = pstrLoad( hDSModule, IDS_CPU, sizeof(szBuf), &szBuf );
  cbBuf += _snprintf( &szBuf[cbBuf], sizeof(szBuf) - cbBuf, " %u", cCPU );
  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );
  ulIconAreaCX = max( sizeText.cx, ulIconSize ) + ( psizeEm->cx >> 1 );

  cbBuf = pstrLoad( hDSModule, IDS_USER, sizeof( szBuf ), pcBuf );
  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );
  ulBarTextWidth = sizeText.cx;

  cbBuf = pstrLoad( hDSModule, IDS_IRQ, sizeof( szBuf ), pcBuf );
  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );
  if ( ulBarTextWidth < sizeText.cx )
    ulBarTextWidth = sizeText.cx;

  cbBuf = pstrLoad( hDSModule, IDS_TEMP, sizeof( szBuf ), pcBuf );
  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );
  if ( ulBarTextWidth < sizeText.cx )
    ulBarTextWidth = sizeText.cx;

  putilGetTextSize( hps, sprintf( &szBuf, " 99.9%%" ), &szBuf, &sizeText );
  if ( ulBarTextWidth < sizeText.cx )
    ulBarTextWidth = sizeText.cx;

  ulBarTextWidth += psizeEm->cx >> 1;
  ulBarWidth = psizeEm->cx;
  ulLineHeight = sizeText.cy;
  ulBarHeight = ulIconSize + ulLineHeight;

  pSize->cx = 2 * ulHPad + ulIconAreaCX + 2 * ulBarTextWidth;
  pSize->cy = 2 * ulVPad + ulBarHeight + 2 * ulLineHeight;

  // Space for core temperature load-bar if this value is available.
  if ( cCPUTemp != 0 )
    pSize->cx += ulBarTextWidth;
}


static VOID _loadBar(HPS hps, PRECTL pRect, ULONG ulMax, ULONG ulVal,
                     ULONG cbBottomCap, PCHAR pcBottomCap,
                     ULONG cbTopCap, PCHAR pcTopCap)
{
  RECTL		rect = *pRect;
  ULONG		ulUnitCY;
  ULONG		ulIdx;
  LONG		lColor = BARCLR_1;
  POINTL	pt;
  SIZEL		sizeText;

  if ( cbBottomCap != 0 && pcBottomCap != NULL )
  {
    putilGetTextSize( hps, cbBottomCap, pcBottomCap, &sizeText );
    pt.x = ( ( pRect->xLeft + pRect->xRight ) / 2 ) - ( sizeText.cx / 2 );
    pt.y = pRect->yBottom - sizeText.cy;
    GpiCharStringAt( hps, &pt, cbBottomCap, pcBottomCap );
  }

  if ( cbTopCap != 0 && pcTopCap != NULL )
  {
    putilGetTextSize( hps, cbTopCap, pcTopCap, &sizeText );
    pt.x = ( ( pRect->xLeft + pRect->xRight ) / 2 ) - ( sizeText.cx / 2 );
    pt.y = pRect->yTop + 2;
    GpiCharStringAt( hps, &pt, cbTopCap, pcTopCap );
  }

  // Bar's unit height ( -4 is (1 frame + 1 space frame<->lines) * 2 for top
  // and bottom ).
  ulUnitCY = ( rect.yTop - rect.yBottom - 4 ) / BAR_LINES;

  rect.yTop = pRect->yBottom + ( BAR_LINES * ulUnitCY ) + 4;

  putil3DFrame( hps, &rect, BARCLR_SHADOW, BARCLR_LIGHT );
  WinInflateRect( NULLHANDLE, &rect, -1, -1 );
  putilBox( hps, &rect, BARCLR_BG );

  rect.xRight--;
  rect.xLeft++;
  rect.yBottom += 2;
  rect.yTop = rect.yBottom + (ulUnitCY >> 1);

  if ( ulMax != 0 )
  {
    for( ulIdx = 0;
         ulIdx < ( (ulVal > ulMax) ? BAR_LINES : ((BAR_LINES * ulVal) / ulMax) );
         ulIdx++ )
    {
      if ( ulIdx == (BAR_LINES / 2) )
        lColor = BARCLR_2;
      else if ( ulIdx == BAR_LINES - (BAR_LINES / 4) )
        lColor = BARCLR_3;

      putilBox( hps, &rect, lColor );
      WinOffsetRect( NULLHANDLE, &rect, 0, ulUnitCY );
    }
  }
}

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  PCPU		pCPU = &pCPUList[ulIndex];
  POINTL	pt;
  CHAR		szBuf[128];
  ULONG		cbBuf;
  ULONG		ulLoad = 0;
  BOOL		fInterval = pgrGetValue( &stGraph, &pCPU->stGrVal, &ulLoad );
  ULONG		ulMaxLoad;
  RECTL		rect;
  CHAR		szCaption[32];
  ULONG		cbCaption = 0;

  // Icon
  WinDrawPointer( hps, ulHPad, ulVPad + (ulLineHeight >> 1),
                  pCPU->fOnline ? hIcon : hIconOff, DP_NORMAL );

  // String "CPU: N"
  cbBuf = pstrLoad( hDSModule, IDS_CPU, sizeof(szBuf), &szBuf );
  cbBuf += _snprintf( &szBuf[cbBuf], sizeof(szBuf) - cbBuf, " %u", ulIndex );
  pt.x = ulHPad;
  pt.y = ulVPad + ulIconSize + ulLineHeight;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );

  // "User" CPU load.

  if ( fInterval )
    ulLoad -= pCPU->ulLoadIntr;

  if ( fInterval )
  {
    ldiv_t	stLoad = ldiv( ulLoad, 10 );

    cbBuf = sprintf( &szBuf, "%u.%u%%", stLoad.quot, stLoad.rem );
  }
  else
    cbBuf = 0;

  cbCaption = pstrLoad( hDSModule, IDS_USER, sizeof(szCaption), &szCaption );

  rect.xLeft = ulHPad + ulIconAreaCX + (ulBarTextWidth / 2) - (ulBarWidth / 2);
  rect.yBottom = ulVPad + ulLineHeight;
  rect.xRight = rect.xLeft + ulBarWidth;
  rect.yTop = rect.yBottom + ulBarHeight;
  _loadBar( hps, &rect, 1000, ulLoad, cbCaption, &szCaption, cbBuf, &szBuf );

  // "IRQ" CPU load.

  if ( fInterval )
  {
    ldiv_t	stLoad = ldiv( pCPU->ulLoadIntr, 10 );

    cbBuf = sprintf( &szBuf, "%u.%u%%", stLoad.quot, stLoad.rem );
  }
  else
    cbBuf = 0;

  cbCaption = pstrLoad( hDSModule, IDS_IRQ, sizeof(szCaption), &szCaption );

  WinOffsetRect( hab, &rect, ulBarTextWidth, 0 );
  _loadBar( hps, &rect, 1000, pCPU->ulLoadIntr, cbCaption, szCaption, cbBuf,
            &szBuf );

  // Core temperature.

  if ( cCPUTemp != 0 )
  {
    ULONG	ulIdx;
    PCPUTEMP	pCPUTemp = NULL;

    // Search CPUTEMP record for processor.
    for( ulIdx = 0; ulIdx < cCPUTemp; ulIdx++ )
    {
      if ( paCPUTemp[ulIdx].ulId == ulIndex )
      {
        pCPUTemp = &paCPUTemp[ulIndex];
        break;
      }
    }

    if ( pCPUTemp == NULL ||
         ( pCPUTemp->ulCode != CPU_OK && pCPUTemp->ulCode != CPU_OK_OFFLINE ) )
    {
      // Temperature was not detected (processor OFFLINE or error).
      ulLoad = 0;
      ulMaxLoad = 0;
      cbBuf = 0;
    }
    else
    {
      ulLoad = pCPUTemp->ulCurTemp;
      ulMaxLoad = pCPUTemp->ulMaxTemp;

      if ( ulPTMU == TZ_MU_FAHRENHEIT )
        cbBuf = sprintf( &szBuf, "%u øF", ( 9 * ulLoad / 5 + 32 ) );
      else
        cbBuf = sprintf( &szBuf, "%u øC", ulLoad );
    }

    cbCaption = pstrLoad( hDSModule, IDS_TEMP, sizeof(szCaption), &szCaption );

    WinOffsetRect( hab, &rect, ulBarTextWidth, 0 );
    _loadBar( hps, &rect, ulMaxLoad, ulLoad, cbCaption, szCaption, cbBuf, &szBuf );
  }
}

DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  RECTL			rclGraph;
  PGRVAL		apGrVal[64];
  ULONG			ulIdx, ulColumnTop, ulColumnMaxRight;
  CHAR			szBuf[256];
  PCHAR			pcBuf;
  CHAR			szAbsc[32];
  ULONG			cbBuf;
  POINTL		pt, pt2;
  PCHAR			pCh;
  COLOR			lColor = GpiQueryColor( hps );
  SIZEL			sizeText;
  POINTL		aptText[TXTBOX_COUNT];
  SIZEL			sizeEm;

#define DETAILS_LPAD		30
#define DETAILS_RPAD		30

  GpiQueryTextBox( hps, 4, "100%", TXTBOX_COUNT, &aptText );
  sizeText.cx = aptText[TXTBOX_TOPRIGHT].x - aptText[TXTBOX_BOTTOMLEFT].x;
  sizeText.cy = aptText[TXTBOX_TOPRIGHT].y - aptText[TXTBOX_BOTTOMLEFT].y;

  // Graph position.
  rclGraph.xLeft = DETAILS_LPAD + sizeText.cx;
  rclGraph.xRight = psizeWnd->cx - DETAILS_RPAD;
  rclGraph.yTop = psizeWnd->cy - sizeText.cy - GRAPG_ABSCISSA_TOP_PAD - 5;
  rclGraph.yBottom = sizeText.cy + GRAPG_ABSCISSA_BOTTOM_PAD + 5;

  // CPU information

  if ( pszBrandStr != NULL )
    strcpy( szBuf, pszBrandStr );
  else
    szBuf[0] = '\0';

  pcBuf = strchr( szBuf, '\0' );

  if ( ulTemperature != ((ULONG)(-1)) )
  {
    ldiv_t	stT = ldiv( ulTemperature, 10 );
    CHAR	cMU = ulPTMU == TZ_MU_CELSIUS ? 'C' : 'F';

    if ( pcBuf != szBuf )
      *(pcBuf++) = ',';

    if ( stT.rem == 0 )
      pcBuf += sprintf( pcBuf, " %u ø%c", stT.quot, cMU );
    else
      pcBuf += sprintf( pcBuf, " %u.%u ø%c", stT.quot, stT.rem, cMU );
  }

  if ( fRealFreq && ( ullCPU0Freq != 0 ) )
  {
    if ( ( sizeof(szBuf) - (pcBuf - &szBuf) ) > 11 )
    {
      if ( pcBuf != &szBuf )
        *(pcBuf++) = ',';
      *(pcBuf++) = ' ';

      // The trick. Use the function to convert units of information for CPU speed.
      pstrFromBits( ullCPU0Freq, 9, pcBuf, FALSE );
      pCh = strchr( pcBuf, ' ' );
      if ( pCh != NULL )
      {
        // Skip space and first letter after space (' G') and copy with "Hz"
        *((PUSHORT)(pCh + 2)) = (USHORT)'zH';
        pcBuf = pCh + 4;
      }
      else
        pcBuf = strchr( pcBuf, '\0' );
    }
  }

  // Set result string as abscissa caption
  stGrParam.pszAbscissaTopCaption = &szBuf;

  // Graph.

  // Fill list of data storages for CPU and interrupts load graphs
  for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
    apGrVal[ulIdx] = &pCPUList[ulIdx].stGrVal;

  apGrVal[cCPU] = &stGrIRQVal;

  // Time window caption
  if ( pstrFromSec( ulTimeWindow, sizeof(szAbsc), &szAbsc ) > 0 )
    stGrParam.pszAbscissaBottomCaption = &szAbsc;
  else
    stGrParam.pszAbscissaBottomCaption = NULL;

  pgrDraw( &stGraph, hps, &rclGraph, cCPU + 1, &apGrVal, &stGrParam );

  // Colors map.

  putilGetTextSize( hps, 2, "Em", &sizeEm );
  ulColumnTop = rclGraph.yTop - 2 - sizeText.cy;
  ulColumnMaxRight = 0;

  pt.x = rclGraph.xLeft + 5;
  pt.y = ulColumnTop;

  for( ulIdx = 0; ulIdx <= cCPU; ulIdx++ )
  {
    if ( pt.y <= rclGraph.yBottom )
    {
      pt.x = ulColumnMaxRight + ( sizeEm.cx / 2 );
      pt.y = ulColumnTop;
    }

    cbBuf = pstrLoad( hDSModule, ulIdx < cCPU ? IDS_CPU : IDS_IRQ,
                                 sizeof(szBuf), &szBuf );
    if ( ulIdx < cCPU )
      cbBuf += _snprintf( &szBuf[cbBuf], sizeof(szBuf) - cbBuf, " %u", ulIdx );
    GpiSetColor( hps, pGrValParam[ulIdx].clrGraph );
    GpiCharStringAt( hps, &pt, cbBuf, &szBuf );

    GpiQueryCurrentPosition( hps, &pt2 );
    if ( pt2.x > ulColumnMaxRight )
      ulColumnMaxRight = pt2.x;

    pt.y -= sizeText.cy;
  }

  GpiSetColor( hps, lColor );
}
