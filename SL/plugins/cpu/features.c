#define INCL_ERRORS
#define INCL_WIN
#include <os2.h>
#include <ds.h>
#include <string.h>
#include "cpu.h"
#include <debug.h>

extern HMODULE		hDSModule;	// Module handle, cpu.c
// Helpers, cpu.c
extern PHutilLoadInsertStr		putilLoadInsertStr;
extern PHctrlDlgCenterOwner		pctrlDlgCenterOwner;

static PCHAR		pcMLEImpBuf;
static IPT		iptOffset;
static HWND		hwndMLE;

// Macros for CPUID signature.
#define CPUID_STEPPING      0x0000000f
#define CPUID_MODEL         0x000000f0
#define CPUID_FAMILY        0x00000f00
#define CPUID_EXT_MODEL     0x000f0000
#define CPUID_EXT_FAMILY    0x0ff00000
#define CPUID_TO_STEPPING(id)       ((id) & CPUID_STEPPING)
#define CPUID_TO_MODEL(id) \
   ((((id) & CPUID_MODEL) >> 4) | ((((id) & CPUID_FAMILY) >= 0x600) ? \
     (((id) & CPUID_EXT_MODEL) >> 12) : 0))
#define CPUID_TO_FAMILY(id) \
   ((((id) & CPUID_FAMILY) >> 8) + ((((id) & CPUID_FAMILY) == 0xF00) ? \
     (((id) & CPUID_EXT_FAMILY) >> 20) : 0))


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

static VOID _cpuid(PLONG plInfo, ULONG ulAX)
{
  __asm {
    mov     edi, plInfo
    mov     eax, ulAX
    cpuid
    mov     [edi], eax
    mov     [edi+4], ebx
    mov     [edi+8], ecx
    mov     [edi+12], edx
  }
}

static VOID _textString(ULONG ulId/*, BOOL fTabPref*/)
{
  PCHAR		pcBuf = pcMLEImpBuf;
  LONG		cbLine = 0;

//  if ( fTabPref )
//  {
    *(pcBuf++) = '\t';
    cbLine++;
//  }

  cbLine += WinLoadMessage( NULLHANDLE, hDSModule, ulId, 4094, pcBuf );
  if ( cbLine == 0 )
  {
    debug( "Cannot load message #%u", ulId );
    return;
  }
  pcMLEImpBuf[cbLine++] = '\n';
  WinSendMsg( hwndMLE, MLM_IMPORT, MPFROMP( &iptOffset ), MPFROMLONG(cbLine) );
}

static VOID _textStrValue(ULONG ulId, PSZ pszVal)
{
  ULONG		cbLine;

  cbLine = putilLoadInsertStr( hDSModule,       // module handle
                               FALSE,           // string (1) / message (0)
                               ulId,            // string/message id
                               1, &pszVal,      // count and pointers to values
                               4096, pcMLEImpBuf ); // result buffer

  pcMLEImpBuf[cbLine++] = '\n';
  WinSendMsg( hwndMLE, MLM_IMPORT, MPFROMP( &iptOffset ), MPFROMLONG(cbLine) );
}

static VOID _textULValue(ULONG ulId, ULONG ulVal)
{
  CHAR		szVal[32];

  ultoa( ulVal, &szVal, 10 );
  _textStrValue( ulId, &szVal );
}

static VOID _textULValueHex(ULONG ulId, ULONG ulVal)
{
  CHAR		szVal[32];

  *((PUSHORT)szVal) = (USHORT)'x0';
  ultoa( ulVal, &szVal[2], 16 );
  _textStrValue( ulId, &szVal );
}


static VOID _dlgResized(HWND hwnd)
{
  HENUM		hEnum;
  HWND		hwndNext;
  HWND		hwndMLE;
  CHAR		szClass[128];
  RECTL		rect;
  ULONG		cButtons = 0;
  POINTL	aPt[2];
  struct BUTTON {
    HWND hwnd;
    SWP  swp;
  }		aButtons[4];

  hEnum = WinBeginEnumWindows( hwnd );

  while( ( hwndNext = WinGetNextWindow( hEnum ) ) != NULLHANDLE )
  {
    if ( WinQueryClassName( hwndNext, sizeof( szClass ), szClass ) == 0 )
      continue;

    if ( strcmp( &szClass, "#10" ) == 0 )
      hwndMLE = hwndNext;
    else if ( ( strcmp( &szClass, "#3" ) == 0 ) &&
              ( cButtons < sizeof( aButtons ) / sizeof( struct BUTTON ) ) &&
              ( WinQueryWindowPos( hwndNext, &aButtons[cButtons].swp ) ) &&
              ( !( aButtons[cButtons].swp.fl & SWP_HIDE ) ) )
    {
      aButtons[cButtons].hwnd = hwndNext;
      cButtons++;
    }
  }

  WinEndEnumWindows( hEnum );

  if ( !WinQueryWindowRect( hwnd, &rect ) ||
       !WinCalcFrameRect( hwnd, &rect, TRUE ) )
    return;

  // Resizes MLE.
  if( hwndMLE != NULLHANDLE )
  {
    aPt[0].x = rect.xLeft;
    aPt[0].y = rect.yBottom;
    aPt[1].x = rect.xRight;
    aPt[1].y = rect.yTop;

    if( cButtons != 0 )
    {
      WinMapDlgPoints( hwnd, aPt, 2, FALSE );
      aPt[0].y += 11;
      WinMapDlgPoints( hwnd, aPt, 2, TRUE  );
    }

    WinSetWindowPos( hwndMLE, 0, aPt[0].x,  aPt[0].y, aPt[1].x - aPt[0].x,
                     aPt[1].y - aPt[0].y, SWP_MOVE | SWP_SIZE );
  }

  // Adjust buttons.
  if ( cButtons != 0 )
  {
    ULONG	ulTotalWidth = cButtons * 2;
    ULONG	ulStart;
    ULONG	ulIdx;

    for( ulIdx = 0; ulIdx < cButtons; ulIdx++ )
      ulTotalWidth += aButtons[ulIdx].swp.cx;

    ulStart = ( rect.xRight - rect.xLeft + 1 - ulTotalWidth ) >> 1;

    for( ulIdx = 0; ulIdx < cButtons; ulIdx++ )
    {
      WinSetWindowPos( aButtons[ulIdx].hwnd, 0, ulStart, aButtons[ulIdx].swp.y,
                       0, 0, SWP_MOVE );
      WinInvalidateRect( aButtons[ulIdx].hwnd, NULL, FALSE );
      ulStart += aButtons[ulIdx].swp.cx + 2;
    }
  }
}

static MRESULT EXPENTRY DialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      WinSendDlgItemMsg( hwnd, IDD_MLE_FEATURES, MLM_SETWRAP, 0, 0 );
      pctrlDlgCenterOwner( hwnd );
      _dlgResized( hwnd );
      break;

    case WM_WINDOWPOSCHANGED:
      if ( (((PSWP)mp1)[0].fl & SWP_SIZE) != 0 )
        _dlgResized( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_PB_SELALL:
          {
            HWND	hwndMLE = WinWindowFromID( hwnd, IDD_MLE_FEATURES );

            WinSendMsg( hwndMLE, MLM_SETSEL, MPFROMLONG(0), MPFROMLONG(4096) );
            WinSetFocus( HWND_DESKTOP, hwndMLE );
          }
          return (MRESULT)FALSE;

        case IDD_PB_COPY:
          WinSendDlgItemMsg( hwnd, IDD_MLE_FEATURES, MLM_COPY, 0, 0 );
          return (MRESULT)FALSE;

        case IDD_PB_CLOSE:
          return WinDefDlgProc( hwnd, msg, mp1, mp2 );
      }
      return (MRESULT)FALSE;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


VOID showFeatures(HWND hwndOwner)
{
  HWND		hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndOwner, DialogProc,
                                      hDSModule, IDD_FEATURES, NULL );
  ULONG		ulRC;
  LONG		alInfo[4];
  ULONG		ulIdx;

  struct _INFO {
    LONG		cIds;
    LONG		cExIds;

    CHAR		acCPUString[13];
    CHAR		acBrandString[52];
    ULONG		ulSteppingID;
    ULONG		ulModel;
    ULONG		ulFamily;
    ULONG		ulProcessorType;
    ULONG		ulExtendedmodel;
    ULONG		ulExtendedfamily;
    ULONG		ulBrandIndex;
    ULONG		ulCLFLUSHcachelinesize;
    ULONG		ulLogicalProcessors;
    ULONG		ulAPICPhysicalID;
    BOOL		fSSE3Instructions;
    BOOL		fMONITOR_MWAIT;
    BOOL		fCPLQualifiedDebugStore;
    BOOL		fVirtualMachineExtensions;
    BOOL		fEnhancedIntelSpeedStepTechnology;
    BOOL		fThermalMonitor2;
    BOOL		fSupplementalSSE3;
    BOOL		fL1ContextID;
    BOOL		fCMPXCHG16B;
    BOOL		fxTPRUpdateControl;
    BOOL		fPerfDebugCapabilityMSR;
    BOOL		fSSE41Extensions;
    BOOL		fSSE42Extensions;
    BOOL		fPOPCNT;
    ULONG		ulFeatureInfo;
    BOOL		fMultithreading;

    BOOL		fLAHF_SAHFAvailable;
    BOOL		fCmpLegacy;
    BOOL		fSVM;
    BOOL		fExtApicSpace;
    BOOL		fAltMovCr8;
    BOOL		fLZCNT;
    BOOL		fSSE4A;
    BOOL		fMisalignedSSE;
    BOOL		fPREFETCH;
    BOOL		fSKINITandDEV;
    BOOL		fSYSCALL_SYSRETAvailable;
    BOOL		fExecuteDisableBitAvailable;
    BOOL		fMMXExtensions;
    BOOL		fFFXSR;
    BOOL		f1GBSupport;
    BOOL		fRDTSCP;
    BOOL		f64Available;
    BOOL		f3DNowExt;
    BOOL		f3DNow;

    ULONG		ulCacheLineSize;
    ULONG		ulL2Associativity;
    ULONG		ulCacheSizeK;
    ULONG		ulPhysicalAddress;
    ULONG		ulVirtualAddress;
    BOOL		fNestedPaging;
    BOOL		fLBRVisualization;
    BOOL		fFP128;
    BOOL		fMOVOptimization;
  } stInf = { 0 };

  if ( !cpuHaveCPUID() )
  {
    debug( "CPUID no supported on this CPU" );
    return;
  }

  // _cpuid with an argument of 0 returns the number of valid Ids in alInfo[0]
  // and the CPU identification string in the other three array elements. The
  // CPU identification string is not in linear order. The code below arranges
  // the information in a human readable form.
  _cpuid( &alInfo, 0 );
  stInf.cIds = alInfo[0];
  *((PLONG)(&stInf.acCPUString[0])) = alInfo[1];
  *((PLONG)(&stInf.acCPUString[4])) = alInfo[3];
  *((PLONG)(&stInf.acCPUString[8])) = alInfo[2];
  stInf.acCPUString[12] = '\0';

  // Get the information associated with Id 1
  _cpuid( &alInfo, 1 );

  // Interpret CPU feature information.
#if 0
  stInf.ulSteppingID = alInfo[0] & 0x0F;
  stInf.ulModel = (alInfo[0] >> 4) & 0x0F;
  stInf.ulFamily = (alInfo[0] >> 8) & 0x0F;
#else
  stInf.ulSteppingID = CPUID_TO_STEPPING( alInfo[0] );
  stInf.ulModel = CPUID_TO_MODEL( alInfo[0] );
  stInf.ulFamily = CPUID_TO_FAMILY( alInfo[0] );
#endif
  stInf.ulProcessorType = (alInfo[0] >> 12) & 0x03;
  stInf.ulExtendedmodel = (alInfo[0] >> 16) & 0x0F;
  stInf.ulExtendedfamily = (alInfo[0] >> 20) & 0xFF;
  stInf.ulBrandIndex = alInfo[1] & 0xFF;
  stInf.ulCLFLUSHcachelinesize = ((alInfo[1] >> 8) & 0xFF) * 8;
  stInf.ulLogicalProcessors = ((alInfo[1] >> 16) & 0xFF);
  stInf.ulAPICPhysicalID = (alInfo[1] >> 24) & 0xFF;
  stInf.fSSE3Instructions = (alInfo[2] & 0x01) != 0;
  stInf.fMONITOR_MWAIT = (alInfo[2] & 0x08) != 0;
  stInf.fCPLQualifiedDebugStore = (alInfo[2] & 0x10) != 0;
  stInf.fVirtualMachineExtensions = (alInfo[2] & 0x20) != 0;
  stInf.fEnhancedIntelSpeedStepTechnology = (alInfo[2] & 0x80) != 0;
  stInf.fThermalMonitor2 = (alInfo[2] & 0x0100) != 0;
  stInf.fSupplementalSSE3 = (alInfo[2] & 0x0200) != 0;
  stInf.fL1ContextID = (alInfo[2] & 0x0300) != 0;
  stInf.fCMPXCHG16B= (alInfo[2] & 0x2000) != 0;
  stInf.fxTPRUpdateControl = (alInfo[2] & 0x4000) != 0;
  stInf.fPerfDebugCapabilityMSR = (alInfo[2] & 0x8000) != 0;
  stInf.fSSE41Extensions = (alInfo[2] & 0x80000) != 0;
  stInf.fSSE42Extensions = (alInfo[2] & 0x100000) != 0;
  stInf.fPOPCNT= (alInfo[2] & 0x800000) != 0;
  stInf.ulFeatureInfo = alInfo[3];
  stInf.fMultithreading = (stInf.ulFeatureInfo & (1 << 28)) != 0;

  // Calling _cpuid with 0x80000000 as argument gets the number of valid
  // extended IDs.
  _cpuid( &alInfo, 0x80000000 );
  stInf.cExIds = alInfo[0];

  // Get the information associated with extended IDs.

  if ( stInf.cExIds >= 0x80000001 )
  {
    _cpuid( &alInfo, 0x80000001 );

    stInf.fLAHF_SAHFAvailable = (alInfo[2] & 0x01) != 0;
    stInf.fCmpLegacy = (alInfo[2] & 0x02) != 0;
    stInf.fSVM = (alInfo[2] & 0x04) != 0;
    stInf.fExtApicSpace = (alInfo[2] & 0x08) != 0;
    stInf.fAltMovCr8 = (alInfo[2] & 0x10) != 0;
    stInf.fLZCNT = (alInfo[2] & 0x20) != 0;
    stInf.fSSE4A = (alInfo[2] & 0x40) != 0;
    stInf.fMisalignedSSE = (alInfo[2] & 0x80) != 0;
    stInf.fPREFETCH = (alInfo[2] & 0x0100) != 0;
    stInf.fSKINITandDEV = (alInfo[2] & 0x1000) != 0;
    stInf.fSYSCALL_SYSRETAvailable = (alInfo[3] & 0x800) != 0;
    stInf.fExecuteDisableBitAvailable = (alInfo[3] & 0x10000) != 0;
    stInf.fMMXExtensions = (alInfo[3] & 0x40000) != 0;
    stInf.fFFXSR = (alInfo[3] & 0x200000) != 0;
    stInf.f1GBSupport = (alInfo[3] & 0x400000) != 0;
    stInf.fRDTSCP = (alInfo[3] & 0x8000000) != 0;
    stInf.f64Available = (alInfo[3] & 0x20000000) != 0;
    stInf.f3DNowExt = (alInfo[3] & 0x40000000) != 0;
    stInf.f3DNow = (alInfo[3] & 0x80000000) != 0;
  }

  if ( stInf.cExIds >= 0x80000002 )
  {
    _cpuid( &alInfo, 0x80000002 );
    memcpy( &stInf.acBrandString[0], &alInfo, sizeof(alInfo) );
  }

  if ( stInf.cExIds >= 0x80000003 )
  {
    _cpuid( &alInfo, 0x80000003 );
    memcpy( &stInf.acBrandString[sizeof(alInfo)], &alInfo, sizeof(alInfo) );
  }

  if ( stInf.cExIds >= 0x80000004 )
  {
    _cpuid( &alInfo, 0x80000004 );
    memcpy( &stInf.acBrandString[2 * sizeof(alInfo)], &alInfo, sizeof(alInfo) );
  }

  if ( stInf.cExIds >= 0x80000006 )
  {
    _cpuid( &alInfo, 0x80000006 );
    stInf.ulCacheLineSize = alInfo[2] & 0xFF;
    stInf.ulL2Associativity = (alInfo[2] >> 12) & 0x0F;
    stInf.ulCacheSizeK = (alInfo[2] >> 16) & 0xFFFF;
  }

  if ( stInf.cExIds >= 0x80000008 )
  {
    _cpuid( &alInfo, 0x80000008 );
    stInf.ulPhysicalAddress = alInfo[0] & 0xFF;
    stInf.ulVirtualAddress = (alInfo[0] >> 8) & 0xFF;
  }

  if ( stInf.cExIds >= 0x8000000A )
  {
    _cpuid( &alInfo, 0x8000000A );
    stInf.fNestedPaging = (alInfo[3] & 0x01) != 0;
    stInf.fLBRVisualization = (alInfo[3] & 0x02) != 0;
  }

  if ( stInf.cExIds >= 0x8000001A )
  {
    _cpuid( &alInfo, 0x8000001A );
    stInf.fFP128 = (alInfo[0] & 0x01) != 0;
    stInf.fMOVOptimization = (alInfo[0] & 0x02) != 0;
  }


  // Create text

  // Allocate buffer for importing text to MLE.
  ulRC = DosAllocMem( (PVOID)&pcMLEImpBuf, 4096, PAG_COMMIT+PAG_WRITE );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosAllocMem(), rc = %u", ulRC );
    return;
  }

  iptOffset = 0; //  Insertion point.
  hwndMLE = WinWindowFromID( hwndDlg, IDD_MLE_FEATURES );
  // MLE: set format for buffer importing.
  WinSendMsg( hwndMLE, MLM_FORMAT, MPFROMLONG( MLFIE_NOTRANS ), 0 );
  // MLE: set import buffer. 
  WinSendMsg( hwndMLE, MLM_SETIMPORTEXPORT, MPFROMP( pcMLEImpBuf ),
              MPFROMLONG( 4096 ) );

  // Send lines to MLE.

  _textStrValue( IDMSG_PF_CPUSTRING, &stInf.acCPUString );
  if ( stInf.cIds >= 1 )
  {
    if ( stInf.ulSteppingID != 0 )
      _textULValue( IDMSG_PF_STEPPINGID, stInf.ulSteppingID );
    if ( stInf.ulModel != 0 )
      _textULValueHex( IDMSG_PF_MODEL, stInf.ulModel );
    if ( stInf.ulFamily != 0 )
      _textULValueHex( IDMSG_PF_FAMILY, stInf.ulFamily );
    if ( stInf.ulProcessorType != 0 )
      _textULValue( IDMSG_PF_PROCESSORTYPE, stInf.ulProcessorType );
    if ( stInf.ulExtendedmodel != 0 )
      _textULValue( IDMSG_PF_EXTENDEDMODEL, stInf.ulExtendedmodel );
    if ( stInf.ulExtendedfamily != 0 )
      _textULValue( IDMSG_PF_EXTENDEDFAMILY, stInf.ulExtendedfamily );
    if ( stInf.ulBrandIndex != 0 )
      _textULValue( IDMSG_PF_BRANDINDEX, stInf.ulBrandIndex );
    if ( stInf.ulCLFLUSHcachelinesize != 0 )
      _textULValue( IDMSG_PF_CLFLUSHCACHELINESIZE, stInf.ulCLFLUSHcachelinesize );
    if ( stInf.fMultithreading && ( stInf.ulLogicalProcessors != 0 ) )
      _textULValue( IDMSG_PF_LOGICALPROCESSORS, stInf.ulLogicalProcessors );
    if ( stInf.ulAPICPhysicalID != 0 )
      _textULValue( IDMSG_PF_APICPHYSICALID, stInf.ulAPICPhysicalID );
  }

  if ( stInf.cExIds >= 0x80000004 )
    _textStrValue( IDMSG_PF_CPUBRANDSTRING, &stInf.acBrandString );

  if ( stInf.cExIds >= 0x80000006 )
  {
    _textULValue( IDMSG_PF_CACHELINESIZE, stInf.ulCacheLineSize );
    _textULValue( IDMSG_PF_L2ASSOCIATIVITY, stInf.ulL2Associativity );
    _textULValue( IDMSG_PF_CACHESIZE, stInf.ulCacheSizeK );
  }

  if ( stInf.cIds >= 1 )
  {
    _textStrValue( IDMSG_PF_FEATURESTITLE, "\n" );

    if ( stInf.fSSE3Instructions )
      _textString( IDMSG_PF_SSE3INSTRUCTIONS );
    if ( stInf.fMONITOR_MWAIT )
      _textString( IDMSG_PF_MONITORMWAIT );
    if ( stInf.fCPLQualifiedDebugStore )
      _textString( IDMSG_PF_CPLQUALIFIEDDBGSTORE );
    if ( stInf.fVirtualMachineExtensions )
      _textString( IDMSG_PF_VIRTUALMACHINEEXT );
    if ( stInf.fEnhancedIntelSpeedStepTechnology )
      _textString( IDMSG_PF_ENHINTELSPEEDSTEP );
    if ( stInf.fThermalMonitor2 )
      _textString( IDMSG_PF_THERMALMONITOR2 );
    if ( stInf.fSupplementalSSE3 )
      _textString( IDMSG_PF_SUPPLEMENTALSSE3 );
    if ( stInf.fL1ContextID )
      _textString( IDMSG_PF_L1CONTEXTID );
    if ( stInf.fCMPXCHG16B )
      _textString( IDMSG_PF_CMPXCHG16B );
    if ( stInf.fxTPRUpdateControl )
      _textString( IDMSG_PF_XTPRUPDATECONTROL );
    if ( stInf.fPerfDebugCapabilityMSR )
      _textString( IDMSG_PF_PERFDBGCAPABILITYMSR );
    if ( stInf.fSSE41Extensions )
      _textString( IDMSG_PF_SSE41EXTENSIONS );
    if ( stInf.fSSE42Extensions )
      _textString( IDMSG_PF_SSE42EXTENSIONS );
    if ( stInf.fPOPCNT )
      _textString( IDMSG_PF_POPCNT );

    for( ulIdx = 0; ulIdx < 32; ulIdx++ )
    {
      if ( ( stInf.ulFeatureInfo & (1 << ulIdx) ) != 0 )
        _textString( IDMSG_PF_FEATURE1 + ulIdx );
    }

    if ( stInf.fLAHF_SAHFAvailable )
      _textString( IDMSG_PF_LAHF_SAHFAVAILABLE );
    if ( stInf.fCmpLegacy )
      _textString( IDMSG_PF_CMPLEGACY );
    if ( stInf.fSVM )
      _textString( IDMSG_PF_SVM );
    if ( stInf.fExtApicSpace )
      _textString( IDMSG_PF_EXTAPICSPACE );
    if ( stInf.fAltMovCr8 )
      _textString( IDMSG_PF_ALTMOVCR8 );
    if ( stInf.fLZCNT )
      _textString( IDMSG_PF_LZCNT );
    if ( stInf.fSSE4A )
      _textString( IDMSG_PF_SSE4A );
    if ( stInf.fMisalignedSSE )
      _textString( IDMSG_PF_MISALIGNEDSSE );
    if ( stInf.fPREFETCH )
      _textString( IDMSG_PF_PREFETCH );
    if ( stInf.fSKINITandDEV )
      _textString( IDMSG_PF_SKINITANDDEV );
    if ( stInf.fSYSCALL_SYSRETAvailable )
      _textString( IDMSG_PF_SYSCALL_SYSRET );
    if ( stInf.fExecuteDisableBitAvailable )
      _textString( IDMSG_PF_EXECUTEDISABLEBIT );
    if ( stInf.fMMXExtensions )
      _textString( IDMSG_PF_MMXEXTENSIONS );
    if ( stInf.fFFXSR )
      _textString( IDMSG_PF_FFXSR );
    if ( stInf.f1GBSupport )
      _textString( IDMSG_PF_1GBSUPPORT );
    if ( stInf.fRDTSCP )
      _textString( IDMSG_PF_RDTSCP );
    if ( stInf.f64Available )
      _textString( IDMSG_PF_64AVAILABLE );
    if ( stInf.f3DNowExt )
      _textString( IDMSG_PF_3DNOWEXT );
    if ( stInf.f3DNow )
      _textString( IDMSG_PF_3DNOW );
    if ( stInf.fNestedPaging )
      _textString( IDMSG_PF_NESTEDPAGING );
    if ( stInf.fLBRVisualization )
      _textString( IDMSG_PF_LBRVISUALIZATION );
    if ( stInf.fFP128 )
      _textString( IDMSG_PF_FP128 );
    if ( stInf.fMOVOptimization )
      _textString( IDMSG_PF_MOVOPTIMIZATION );
  }

  // Free import buffer.  
  DosFreeMem( pcMLEImpBuf  );

  WinProcessDlg( hwndDlg  );
  WinDestroyWindow( hwndDlg  );
}
