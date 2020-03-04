#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#define INCL_DOSSPINLOCK
#define INCL_DOSMISC
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <stdlib.h>
#include <string.h>
#include "..\rdmsr\rdmsr.h"
#include "cputemp.h"
#include "debug.h"

// Data for one CPU will be updated not more often than once every
// MIN_DELTA_TIME milliseconds.
#define MIN_DELTA_TIME		950

typedef unsigned long long	ULLONG;
typedef ULLONG			*PULLONG;

static BOOL                     fRenKrnl = FALSE;
static HFILE			hDriver = NULLHANDLE;
static PCPUTEMP			paCPU = NULL;
static ULONG			cCPU = 0;
static HMTX			hmtxCPUList = NULLHANDLE;

#define _CURRENTCPU          (~0)

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


// BOOL _haveCPUID()
//
// Returns TRUE if command CPUID supported by installed processor.

BOOL _haveCPUID();
#pragma aux _haveCPUID = \
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

// VOID _cpuid(ULONG ulCPU, PULONG pulCPUInfo, ULONG ulAX)
//
// Query CPU specific information (processor supplementary instruction CPUID).
// If CPU was specified (ulCPU is NOT _CURRENTCPU) a driver RDMSR$ will be
// requested - valid in "rendezvous" mode only.
// Returns FALSE if the request to the driver fails.

static BOOL _cpuid(ULONG ulCPU, PRDMSRCPUID pCPUID, ULONG ulEAX)
{
  RDMSRPARAM	stParam;

  if ( ulCPU == _CURRENTCPU )
  {
    __asm {
      mov     edi, pCPUID
      mov     eax, ulEAX
      cpuid
      mov     [edi], eax
      mov     [edi+4], ebx
      mov     [edi+8], ecx
      mov     [edi+12], edx
    }
    return TRUE;
  }

  stParam.ulReg = ulEAX;
  stParam.usCPU = ulCPU;

  return DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY, IOCTL_RDMSR_FN_CPUID,
                      &stParam, sizeof(RDMSRPARAM), &ulEAX,
                      pCPUID, sizeof(RDMSRCPUID), &ulCPU ) == NO_ERROR;
}

// BOOL _rdmsr(ULONG ulCPU, ULONG ulReg, PRDMSRVAL pVal)
//
// Send request to the driver to execute RDMSR processor instruction on CPU
// specified with ulCPU or on the current CPU (ulCPU = _CURRENTCPU).

static ULONG _rdmsr(ULONG ulCPU, ULONG ulReg, PRDMSRVAL pVal)
{
  RDMSRPARAM	stParam = { 0 };
  ULONG		cbParam;
  ULONG		cbVal;
  ULONG		ulRC;

  debug( "CPU: 0x%lX, Reg: 0x%lX", ulCPU, ulReg );

  // Let chance to keep debug output on TRAP in driver.
  debugDone( 0 );
#ifdef DEBUG_CODE
  DosSleep( 1 );
#endif

  stParam.ulReg = ulReg;
  stParam.usCPU = ulCPU;
  ulRC = DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY,
                      ulCPU == _CURRENTCPU            // For current CPU ? 
                        ? IOCTL_RDMSR_FN_READCURCPU  // Valid for any kernel.
                        : IOCTL_RDMSR_FN_READSPECPU, // For "rendezvous" mode.
                      &stParam, sizeof(RDMSRPARAM), &cbParam,
                      pVal, sizeof(RDMSRVAL), &cbVal );

  debugInit();
  debug( "Driver result code: %lu", ulRC );

  return ulRC;
}

// ULONG detectTjmax(ULONG ulCPU, PULONG pulAPICPhysId)
//
// Returns Tjunction maximum value for processor ulCPU or current processor
// (ulCPU = _CURRENTCPU). The values of the sensors will be subtracted from this
// value for calculating temperature.
// Other return codes: 0 - CPU not supported, ~0 - driver IO error.

static ULONG detectTjmax(ULONG ulCPU, PULONG pulAPICPhysId)
{
#define DEF_TJMAX	100

/*
CPUs with IA32_TEMPERATURE_TARGET:
#define INTEL_FUTURE_MODEL_4E                0x4E
#define INTEL_FUTURE_MODEL_56                0x56
#define INTEL_BROADWELL_MODEL_3D     0x3d
#define INTEL_HASWELL_MODEL_3C               0x3c
#define INTEL_HASWELL_MODEL_3F               0x3F
#define INTEL_HASWELL_MODEL_45               0x45
#define INTEL_HASWELL_MODEL_46               0x46
#define INTEL_IVYBRIDGE_MODEL_3A     0x3a
#define INTEL_IVYBRIDGE_MODEL_3E     0x3e
#define INTEL_NEHALEM_MODEL_1A               0x1a
#define INTEL_NEHALEM_MODEL_1E               0x1e
#define INTEL_NEHALEM_MODEL_1F               0x1f
#define INTEL_NEHALEM_MODEL_2E               0x2e
#define INTEL_SANDYBRIDGE_MODEL_2A   0x2a
#define INTEL_SANDYBRIDGE_MODEL_2D   0x2d
#define INTEL_SILVERMONT_MODEL_37    0x37
#define INTEL_SILVERMONT_MODEL_4A    0x4a
#define INTEL_SILVERMONT_MODEL_4D    0x4d
#define INTEL_SILVERMONT_MODEL_5A    0x5a
#define INTEL_SILVERMONT_MODEL_5D    0x5d
#define INTEL_WESTMERE_MODEL_25              0x25
#define INTEL_WESTMERE_MODEL_2C              0x2c
#define INTEL_WESTMERE_MODEL_2F              0x2f*/

  RDMSRCPUID           stCPUID;
  ULONG                ulStepping, ulModel, ulFamily;
  RDMSRVAL             stMSR;

  // Query CPU signature.

  if ( !_haveCPUID() )
  {
    debug( "Have not CPUID" );
    return 0;	// Have not CPUID.
  }

  _cpuid( ulCPU, &stCPUID, 0 );
  if ( stCPUID.ulEAX < 6 )		// Check CPUID level.
  {
    debug( "CPUID level: %u, we need at least 6", stCPUID.ulEAX );
    return 0;
  }

  if ( stCPUID.ulEBX != (ULONG)'uneG' )	// INTEL ?
  {
    debug( "Not Intel CPU" );
    return 0;
  }

  // CPUID.06H.EAX[0] indicates whether the CPU has thermal sensors.
  debug( "call _cpuid(%lu,,6)...", ulCPU );
  _cpuid( ulCPU, &stCPUID, 6 );
  if ( (stCPUID.ulEAX & 0x00000001) == 0 )
  {
    debug( "CPU havn't Digital Thermal Sensor" );
    return 0;
  }

  debug( "call _cpuid(%lu,,1)...", ulCPU );
  _cpuid( ulCPU, &stCPUID, 1 );
  debug( "EAX: 0x%lX (APIC id: %u)",
         stCPUID.ulEBX, (stCPUID.ulEBX >> 24) & 0xFF );
  if ( pulAPICPhysId != NULL )
    *pulAPICPhysId = (stCPUID.ulEBX >> 24) & 0xFF;

  ulStepping = CPUID_TO_STEPPING( stCPUID.ulEAX );
  ulModel = CPUID_TO_MODEL( stCPUID.ulEAX );
  ulFamily = CPUID_TO_FAMILY( stCPUID.ulEAX );
  debug( "CPU stepping/model/family: %lu/%lu/%lu", ulStepping, ulModel, ulFamily );

  // Pentium III Mobile CPU
//  if ( ulModel == 0x0B && ulStepping == 0x01 )
//    return 0;

  // Intel "Family Model Stepping" and Intel Erratum Reference.
  // URL: https://blogs.msdn.microsoft.com/andrew_richards/2011/05/20/intel-family-model-stepping-and-intel-erratum-reference/

  if ( ulFamily == 6 )
    // It's a very different way to get temperature on Pentium III:
    // http://download.intel.com/design/intarch/applnots/27340501.pdf
    switch( ulModel )
    {
      case 03: // Pentium II processor, model 3 
      case 05: // Pentium II processor, Pentium II Xeon processor, Celeron
      case 11: // Pentium III processor, model B
        debugCP( "Pentium II/III detected - unsupported CPU" );
        return 0;
    }

  // The mobile Penryn family.
  if ( ulModel == 0x17 && ulStepping == 0x06 )
  {
    debug( "The mobile Penryn family" );
    return 105;
  }

  // Atom D400, N400 and D500 series.
  if ( ulModel == 0x1C )
  {
    debug( "Atom D400, N400 and D500 series" );
    return ulStepping == 0x0A ? 100 : 90;
  }

  if ( // Atom Tunnel Creek (Exx), Lincroft (Z6xx).
       // Note: TjMax for E6xxT is 110C, but CPU type is undetectable.
       ulModel == 0x26 ||
       // Atom Medfield (Z2460).
       ulModel == 0x27 ||
       // Atom Clover Trail/Cloverview (Z2760).
       ulModel == 0x35 )
  {
    debug( "Atom Tunnel Creek (Exx), Lincroft (Z6xx) / Medfield (Z2460) / Trail/Cloverview (Z2760)" );
    return 90;
  }

  // Atom Cedar Trail/Cedarview (N2xxx, D2xxx).
  if ( ulModel == 0x36 )
  {
    debug( "Atom Cedar Trail/Cedarview (N2xxx, D2xxx)" );
    return 100;
  }

  // On some Core 2 CPUs, there is an undocumented
  // MSR that tells if Tj(max) is 100 or 85. Note
  // that MSR_IA32_EXT_CONFIG is not safe on all CPUs.
  if ( ( ulModel == 0x0F && ulStepping >= 2 ) ||
       ( ulModel == 0x0E ) )
  {
    debug( "call _rdmsr(%lu,MSR_IA32_EXT_CONFIG,)...", ulCPU );
    if ( _rdmsr( ulCPU, MSR_IA32_EXT_CONFIG, &stMSR ) != NO_ERROR ) // 0x0EE
    {
      debug( "No access to the driver" );
      return ~0; // No access to the driver.
    }

    if ( (stMSR.ulEAX & 0x40000000) != 0 )
    {
      debug( "Read MSR MSR_IA32_EXT_CONFIG - Ok" );
      return 85;
    }

    debug( "MSR_IA32_EXT_CONFIG, EAX: 0x%lX", stMSR.ulEAX );
  }

  // Attempt to get Tj(max) from IA32_TEMPERATURE_TARGET (0x1A2), but only
  // consider the interval [70, 120] C as valid. It is not fully known which
  // CPU models have the MSR (see models list above).
  // Newer CPUs can tell you what their max temperature is.
  if ( ulModel > 0x0E )
  {
    debug( "call _rdmsr(%lu,MSR_TEMPERATURE_TARGET,)...", ulCPU );
    if ( _rdmsr( ulCPU, MSR_TEMPERATURE_TARGET, &stMSR ) != NO_ERROR )
    {
      debug( "No access to the driver" );
      return ~0; // No access to the driver.
    }

    stMSR.ulEAX = (stMSR.ulEAX >> 16) & 0xFF;
    if ( stMSR.ulEAX >= 70 && stMSR.ulEAX <= 120 )
    {
      debug( "Read MSR MSR_TEMPERATURE_TARGET - Ok" );
      return stMSR.ulEAX;
    }
    else
      debug( "Incorrect MSR MSR_TEMPERATURE_TARGET value: %u", stMSR.ulEAX );
  }

  debug( "Use default value" );
  return DEF_TJMAX;
}

// VOID setCPUTemp(PCPUTEMP pCPU, ULONG ulMSRTermStatEAX)
//
// Sets current temperature values for the CPU record by given sensor value.

static VOID setCPUTemp(PCPUTEMP pCPU, ULONG ulMSRTermStatEAX)
{
  if ( pCPU->ulCode == CPU_UNSUPPORTED )
  {
    debugCP( "CPU_UNSUPPORTED" );
    return;
  }

  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &pCPU->ulTimestamp,
                   sizeof(ULONG) );

  if ( (ulMSRTermStatEAX & 0x80000000) == 0
//       && ( ulMSRTermStatEAX != 0 )                  // Is it right?
     )
  {
    // Invalid value received,
    pCPU->ulCode = CPU_INVALID_VALUE;
    debug( "CPU_INVALID_VALUE (EAX: 0x%lX)", ulMSRTermStatEAX );
    return;
  }

  pCPU->ulCurTemp   = pCPU->ulMaxTemp - ( (ulMSRTermStatEAX >> 16) & 0x7F );
  pCPU->ulCode      = (pCPU->ulCurTemp & 0x80000000) != 0
                        ? CPU_INVALID_VALUE : CPU_OK;
  debug( "pCPU->ulCurTemp = %lu, pCPU->ulCode = %lu",
         pCPU->ulCurTemp, pCPU->ulCode );
}

// VOID initCPUTemp(ULONG ulCPU, PCPUTEMP pCPU)
//
// Sets current and maximum temperature values for the CPU record.

static VOID initCPUTemp(ULONG ulCPU, PCPUTEMP pCPU)
{
  RDMSRVAL	stMSR;

  pCPU->ulCurTemp = 0;

  // Get Tjunction maximum value for the given CPU.
  debug( "call detectTjmax(%lu,)...", ulCPU );
  pCPU->ulMaxTemp = detectTjmax( ulCPU, &pCPU->ulAPICPhysId );
  debug( "detectTjmax(): %u, APIC id: %lu",
         pCPU->ulMaxTemp, pCPU->ulAPICPhysId );

  if ( pCPU->ulMaxTemp == 0 )
  {
    pCPU->ulCode = CPU_UNSUPPORTED;
    debugCP( "CPU_UNSUPPORTED" );
  }
  else if ( ( pCPU->ulMaxTemp == ~0 ) ||
       ( _rdmsr( ulCPU, MSR_IA32_THERM_STATUS, &stMSR ) != NO_ERROR ) )
  {
    // Driver IO error.
    pCPU->ulCode = CPU_IOCTL_ERROR;
    debugCP( "CPU_IOCTL_ERROR" );
  }
  else
  {
    debug( "call setCPUTemp(,0x%lX)..,", stMSR.ulEAX );
    setCPUTemp( pCPU, stMSR.ulEAX );
  }
}




/* ********************************************************************* */
/*                                                                       */
/*              Kernels without "rendezvous" function support.           */
/*                                                                       */
/* ********************************************************************* */
/*
   On kernels without "rendezvous" support we can receive data only from
   the current processor. To collect information from all processors we will
   launch a thread threadQueryTemperature ( see initNotRenKrnl() ).

   BOOL nrkInit()      Initialization for non-rendezvous mode.
   VOID nrkDone()      Finalization for non-rendezvous mode.
   VOID nrkUpdate()    Update temperature value for the current processor.
*/

static HEV   hevStop = NULLHANDLE;
static TID   tid = 0;

// PCPUTEMP findCPU(ULONG ulAPICPhysId)
//
// Search listed object CPUTEMP by APIC physical Id.
// Returns NULL if CPU not listed.

static PCPUTEMP _findCPU(ULONG ulAPICPhysId)
{
  PCPUTEMP	pCPU;
  ULONG		ulIdx;
  BOOL		fFound = FALSE;

  for( ulIdx = 0, pCPU = paCPU; ulIdx < cCPU; ulIdx++, pCPU++ )
  {
    if ( pCPU->ulAPICPhysId == ulAPICPhysId )
    {
      fFound = TRUE;
      break;
    }
  }

  return fFound ? pCPU : NULL;
}


// VOID nrkUpdate()
//
// Updates data of the current processor, but does not more often than once in
// MIN_DELTA_TIME milliseconds for each processors.

static VOID nrkUpdate()
{
  ULONG         ulAPICPhysId;
  PCPUTEMP	pCPU;
  ULONG		ulTimestamp;
  RDMSRVAL	stMSR;
  RDMSRCPUID    stCPUID;


  // Query CPU APIC physical Id.
  _cpuid( _CURRENTCPU, &stCPUID, 1 );
  ulAPICPhysId = (stCPUID.ulEBX >> 24) & 0xFF;
  debug( "ulAPICPhysId = %lu", ulAPICPhysId );

  pCPU = _findCPU( ulAPICPhysId );
  if ( pCPU == NULL )
  {
    // We cannot work with CPU which was not initialized (or not listed).
    debugCP( "Data for CPU was not initialised" );
    return;
  }

  if ( pCPU->ulCode == CPU_UNSUPPORTED )
  {
    debugCP( "Unsupported CPU" );
    return;
  }

  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulTimestamp, sizeof(ULONG) );
  if ( ( ulTimestamp - pCPU->ulTimestamp ) < MIN_DELTA_TIME )
  {
    // The data have been updated recently.
    debugCP( "The data have been updated recently - exit" );
    return;
  }

  if ( _rdmsr( _CURRENTCPU, MSR_IA32_THERM_STATUS, &stMSR ) != NO_ERROR )
  {
    // Driver access failed.
    debugCP( "Driver access failed" );
    pCPU->ulCode = CPU_IOCTL_ERROR;
    return;
  }

  if ( stMSR.usAPICPhysId != ulAPICPhysId )
  {
    // It is only in theory - can be thread changed to another processor on
    // DosDevIOCtl() ( at _rdmsr() ) ?

    debug( "Thread has been jump to other CPU (APIC id: %lu)",
           stMSR.usAPICPhysId );
    pCPU = _findCPU( ulAPICPhysId );
    if ( pCPU == NULL )
    {
      debugCP( "CPU data not found" );
      return;
    }
  }

  // Store current temperature value.
  debugCP( "call setCPUTemp()..." );
  setCPUTemp( pCPU, stMSR.ulEAX );
}

// Thread threadQueryTemperature()
//
// Periodic polling processor sensors. It will end when semaphore hevStop set.

VOID _System threadQueryTemperature(ULONG ulData)
{
  ULONG		ulRC;

  // We will lower priority of the thread while waiting for the semaphore
  // (except the first time). According to my observations, it increases
  // chance that the thread will run on a new processor after pause.

  while( DosWaitEventSem( hevStop, 50 ) == ERROR_TIMEOUT )
  {
    DosSetPriority( PRTYS_THREAD, PRTYC_REGULAR, 0, tid );

    ulRC = DosRequestMutexSem( hmtxCPUList, SEM_INDEFINITE_WAIT );
    if ( ulRC != NO_ERROR )
      break;

    nrkUpdate();

    DosReleaseMutexSem( hmtxCPUList );

    DosSetPriority( PRTYS_THREAD, PRTYC_IDLETIME, 0, tid );
  }
}

static BOOL nrkInit()
{
  MPAFFINITY	qwAffinity;
  MPAFFINITY	qwThreadAffinity;
  ULONG		ulRC;
  PCPUTEMP	pCPU;
  ULONG		ulStatus;
  ULONG		ulIdx;
  BOOL		fFail = FALSE;
  PPIB		pib;
  PTIB		tib;
  ULONG		aulSaveStatus[63];
  ULONG		ulSavePriority = 0;

  // Now we need to get some information about each processor:
  // - APIC physical Id, it will be used to find record in paCPU from the
  //   thread threadQueryTemperature().
  // - Find out Tjunction maximum value for the temperature calculations.
  //
  // Workaround IBM kernel bug in DosSetThreadAffinity():
  // Switching only specific CPU (except first, which always ONLINEd) in ONLINE
  // status helps to avoid freezes in DosSetThreadAffinity() on IBM kernel.
  //
  // 1. Creates thread threadQueryTemperature() before any calls
  //    DosSetThreadAffinity(). Otherwise thread will freeze when CPU
  //    switches to OFFLINE status (by CPUGOV/ACPIDAEMON or smbd. else) on the
  //    IBM kernel. -=8( )
  // 2. Set high priority - nobody should switch CPUs to ONLINE/OFFLINE status
  //    while we work.
  // 3. Set all CPUs (except first) to OFFLINE status.
  // 4. For each CPU:
  //      1. Switch CPU (except first) to ONLINE status.
  //      2. Call DosSetThreadAffinity() to switch thread to a specific CPU.
  //      3. Do necessary work on CPU: query APIC ID, Tjunction maximum and the
  //         sensor current values.
  //      4. Switch CPU (except first) to OFFLINE status.
  // 5. Restore status of the CPUs and the thread priority.
  //
  // *** People, let's finally use OS/4 kernel !

  if ( cCPU == 1 )
  {
    // No need the addiditional thread for UNI systems.
    debugCP( "UNI system, call initCPUTemp(_CURRENTCPU,)..." );
    initCPUTemp( _CURRENTCPU, paCPU );
    return TRUE;
  }

  // Create thread to update information. We will resume it later.
  ulRC = DosCreateThread( &tid, threadQueryTemperature, 0, CREATE_SUSPENDED,
                          4096 );
  if ( ulRC != NO_ERROR )
  {
    tid = 0;
    debugCP( "DosCreateThread() failed" );
    return FALSE;
  }

  DosQueryThreadAffinity( AFNTY_THREAD, &qwThreadAffinity );

  // Save current thread priority, set time-critical priority (if it not
  // current).
  ulRC = DosGetInfoBlocks( &tib, &pib );
  if ( ( ulRC == NO_ERROR ) &&
       ( (tib->tib_ptib2->tib2_ulpri & 0xFF00) < (PRTYC_TIMECRITICAL << 8) ) )
  {
    ulSavePriority = tib->tib_ptib2->tib2_ulpri;
    DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, 0, 0 );
  }

  // Save current status and set all CPUs (except the first) OFFLINE status.
  for( ulIdx = 2; ulIdx <= cCPU; ulIdx++ )
  {
    if ( DosGetProcessorStatus( ulIdx, &ulStatus ) != NO_ERROR )
      ulStatus = PROC_ONLINE;

    aulSaveStatus[ulIdx - 2] = ulStatus;

    if ( ulStatus == PROC_ONLINE )
      DosSetProcessorStatus( ulIdx, PROC_OFFLINE );
  }

  // Walk on each processor and fill paCPU records.

  debug( "Fill paCPU records for %lu CPUs", cCPU );
  for( ulIdx = 0, pCPU = paCPU; ulIdx < cCPU; ulIdx++, pCPU++ )
  {
    pCPU->ulId = ulIdx;

    // Wakeup CPU.
    if ( ulIdx != 0 )
      DosSetProcessorStatus( ulIdx + 1, PROC_ONLINE );

    // Go to ulIdx'th CPU.
    *((PULLONG)&qwAffinity.mask) = ( 1 << ulIdx );
    ulRC = DosSetThreadAffinity( &qwAffinity );
    if ( ulRC != NO_ERROR )
    {
      fFail = TRUE;
      break;
    }

    initCPUTemp( _CURRENTCPU, pCPU );

    // Return OFFLINE status.
    if ( ulIdx != 0 )
    {
      // Let thread go away from this CPU.
      DosSetThreadAffinity( &qwThreadAffinity );
      // Now we can switch CPU back to OFFLINE.
      DosSetProcessorStatus( ulIdx + 1, PROC_OFFLINE );
    }
  }

  // Restore the status of all processors.
  debugCP( "Restore the status of all processors" );
  for( ulIdx = 2; ulIdx <= cCPU; ulIdx++ )
    DosSetProcessorStatus( ulIdx, aulSaveStatus[ulIdx - 2] );

  // Set normal affinity.
  debugCP( "Set normal affinity" );
  DosSetThreadAffinity( &qwThreadAffinity );

  // Restore thread priority.
  debugCP( "Restore thread priority" );
  if ( ( ulSavePriority != 0 ) &&
       ( DosSetPriority( PRTYS_THREAD, (ulSavePriority >> 8) & 0xFF,
                         0, 0 ) == NO_ERROR ) )
    DosSetPriority( PRTYS_THREAD, 0, ulSavePriority & 0xFF, 0 );

  // Create semaphores and start thread.
  debugCP( "Create semaphores and resume a thread" );
  if ( ( !fFail ) &&
       ( ( DosCreateMutexSem( NULL, &hmtxCPUList, 0, FALSE ) != NO_ERROR ) ||
         ( DosCreateEventSem( NULL, &hevStop, 0, FALSE ) != NO_ERROR ) ||
         ( DosResumeThread( tid ) != NO_ERROR )
       ) )
  {
    DosKillThread( tid );
    tid = 0;
    return FALSE;
  }

  debugCP( "Done" );
  return TRUE;
}

static VOID nrkDone()
{
  if ( hevStop != NULLHANDLE )
  {
    if ( tid != 0 )
    {
      DosPostEventSem( hevStop );
      DosWaitThread( &tid, DCWW_WAIT );
      tid = 0;
    }

    DosCloseEventSem( hevStop );
    hevStop = NULLHANDLE;
  }

  debugCP( "Done" );
}


/* ********************************************************************* */
/*                                                                       */
/*                Library initialization and finalization.               */
/*                                                                       */
/* ********************************************************************* */

static VOID destroy()
{
  nrkDone();

  if ( hmtxCPUList != NULLHANDLE )
  {
    DosCloseMutexSem( hmtxCPUList );
    hmtxCPUList = NULLHANDLE;
  }  

  if ( paCPU != NULL )
  {
    free( paCPU );
    paCPU = NULL;
  }

  if ( hDriver != NULLHANDLE )
    DosClose( hDriver );

  cCPU = 0;
}

static VOID init()
{
  ULONG		ulRC;
  ULONG		ulStatus;
  RDMSRCPUID    stCPUID;

  // Open driver DRMSR$.
  debugCP( "--- INITIALIZATION, Open RDMSR$..." );
  ulRC = DosOpen( "RDMSR$", &hDriver, &ulStatus, 0, 0, FILE_OPEN,
                  OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE, NULL );
  if ( ulRC != NO_ERROR )
  {
    debugCP( "Can't open RDMSR$" );
    hDriver = NULLHANDLE;
    return;
  }

  debugCP( "call DosPerfSysCall(0x41,,,)..." );
  if ( ( DosPerfSysCall( 0x41, 0, (ULONG)&cCPU, 0 ) == NO_ERROR ) &&
       ( cCPU != 0 ) &&
       ( DosCreateMutexSem( NULL, &hmtxCPUList, 0, FALSE ) == NO_ERROR ) )
  {
    debug( "We have %lu CPU(s)", cCPU );

    // Allocate memory for CPUs information.
    paCPU = calloc( cCPU, sizeof(CPUTEMP) );
    if ( paCPU != NULL )
    {
      // Detect kernel with "rendezvous" support.
      // Query CPUID on CPU 0: _cpuid() will call driver DRMSR$ with function
      // IOCTL_RDMSR_FN_CPUID that supported only by kernel with "rendezvous".
      debugCP( "Detect kernel with \"rendezvous\" support..." );
      fRenKrnl = _cpuid( 0, &stCPUID, 0 );
      debug( "\"Rendezvous\" kernel support: %s", fRenKrnl ? "YES" : "NO" );

      if ( fRenKrnl )
      {
        // On kernel with "rendezvous" support we can point CPU number to
        // initailzatin.
        PCPUTEMP             pCPU;
        ULONG                ulIdx;

        for( ulIdx = 0, pCPU = paCPU; ulIdx < cCPU; ulIdx++, pCPU++ )
        {
          pCPU->ulId = ulIdx;
          initCPUTemp( ulIdx, pCPU );
        }

        return; // Success for "rendezvous" mode.
      }

      debugCP( "call nrkInit()..." );
      if ( nrkInit() )
      {
        debugCP( "success" );
        return; // Success.
      }
    }
  }

  debugCP( "call destroy()..." );
  destroy();
}

unsigned APIENTRY LibMain(unsigned hmod, unsigned termination) 
{ 
  if ( termination == 0 )
  {
    debugInit();
    init();
  }
  else
  {
    destroy();
    debugDone( 0 );
  }

  return 1; 
}



/* ********************************************************************* */
/*                                                                       */
/*                           Public routines.                            */
/*                                                                       */
/* ********************************************************************* */

// cputempQuery() - public routine.
//
// Fills the user buffer pCPUInfo up to cCPUInfo records of CPUTEMP.
// Number of actual written items stored in pulActual.
// Returns error code CPUTEMP_xxxxxxx. When an error code CPUTEMP_OVERFLOW
// returned pulActual will contain the number of required records.

unsigned long APIENTRY cputempQuery(unsigned long cCPUInfo, PCPUTEMP pCPUInfo,
                                    unsigned long *pulActual)
{
  ULONG		ulRC;
  ULONG		ulIdx;
  PCPUTEMP	pCPU;
  ULONG		ulStatus;
  ULONG		ulActual;
  BOOL		fOverflow;

  if ( hDriver == NULLHANDLE )
    return CPUTEMP_NO_DRIVER;

  if ( ( cCPU == 0 ) ||
       // Thread for updates was not runned?
       ( DosRequestMutexSem( hmtxCPUList, SEM_INDEFINITE_WAIT ) != NO_ERROR ) )
       // Mutex semaphore was not created?
  {
    debugCP( "CPUTEMP_INIT_FAILED" );
    return CPUTEMP_INIT_FAILED;
  }

  if ( fRenKrnl )
  {
    // Kernel with "rendezvous" support. We can get data for all CPU right now
    // (in user request time). Even for OFFLINEd processors!
    RDMSRPARAM           stParam;
    RDMSRVAL2            aVal[64] = { 0 };
    PRDMSRVAL2           pVal;

    // Read model-specific registers MSR_IA32_THERM_STATUS on all installed CPU.

    debugCP( "Read MSR_IA32_THERM_STATUS on all CPUs" );
    debugDone( 0 );

    stParam.ulReg = MSR_IA32_THERM_STATUS;
    ulRC = DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY, IOCTL_RDMSR_FN_READALLCPU,
                        &stParam, sizeof(RDMSRPARAM), &ulStatus,
                        aVal, sizeof(aVal), &ulActual );

    debugInit();
    debug( "Driver result code: %lu", ulRC );

    // Update temperature values for all CPU records.
    for( ulIdx = 0, pVal = aVal; ulIdx < cCPU; ulIdx++, pVal++ )
      setCPUTemp( &paCPU[ pVal->ucCPU ], pVal->ulEAX );
  }
  else
  {
    // Kernel without "rendezvous" support. 
    // In other times nrkUpdate() called from another thread.
    // Reason for additional nrkUpdate() call here is another chance to
    // get data from the different processors. And it's only one nrkUpdate()
    // call here for UNI systems.
    debugCP( "call nrkUpdate()..." );
    nrkUpdate();
  }

  // Copy CPUINFO records to the user buffer.
  ulActual = min( cCPU, cCPUInfo );
  if ( pCPUInfo != NULL )
  {
    memcpy( pCPUInfo, paCPU, ulActual * sizeof(CPUTEMP) );

    if ( cCPU > 1 )   // For SMP systems only.
    {
      // Inform user about OFFLINEd CPUs (set ulCode to CPU_OFFLINE).
      // We don't have information from OFFLINEd CPUs - thred
      // threadQueryTemperature() can't work and fill data on that processors.
      for( ulIdx = 1, pCPU = pCPUInfo; ulIdx <= ulActual; ulIdx++, pCPU++ )
      {
        if ( pCPU->ulCode == CPU_UNSUPPORTED )
          continue;

        ulRC = DosGetProcessorStatus( ulIdx, &ulStatus );
        if ( ulRC == NO_ERROR && ulStatus == PROC_OFFLINE )
          pCPU->ulCode = fRenKrnl ? CPU_OK_OFFLINE : CPU_OFFLINE;
      }
    }
  }

  fOverflow = cCPU > ulActual;
  // Write total number of records at pulActual if user's buffer too small.
  *pulActual = fOverflow ? cCPU : ulActual;

  DosReleaseMutexSem( hmtxCPUList );

  if ( fOverflow )
    debugCP( "CPUTEMP_OVERFLOW" );

  return fOverflow ? CPUTEMP_OVERFLOW : CPUTEMP_OK; 
}
