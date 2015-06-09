#define INCL_DOSDEVICES   /* Device values */
#define INCL_DOSERRORS    /* Error values */
#define INCL_DOSPROCESS
#define INCL_DOSSPINLOCK
#define INCL_DOSMISC
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <stdlib.h>
#include <string.h>
#include "rdmsr.h"
#include "cputemp.h"

// Data for one CPU will be updated not more often than once every
// MIN_DELTA_TIME milliseconds.
#define MIN_DELTA_TIME		950

typedef unsigned long long	ULLONG;
typedef ULLONG			*PULLONG;

static HFILE			hDriver = NULLHANDLE;
static PCPUTEMP			paCPU = NULL;
static ULONG			cCPU = 0;
static HMTX			hmtxCPUList = NULLHANDLE;
static HEV			hevStop = NULLHANDLE;
static TID			tid = 0;

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

// VOID _cpuid(PULONG pulCPUInfo, ULONG ulAX)
//
// Query CPU specific information (processor supplementary instruction CPUID).

static VOID _cpuid(PULONG pulCPUInfo, ULONG ulAX)
{
  __asm {
    mov     edi, pulCPUInfo
    mov     eax, ulAX
    cpuid
    mov     [edi], eax
    mov     [edi+4], ebx
    mov     [edi+8], ecx
    mov     [edi+12], edx
  }
}

// BOOL _rdmsr(ULONG ulReg, PRDMSRVAL pVal)
//
// Send request to the driver to execute RDMSR processor instruction.

static ULONG _rdmsr(ULONG ulReg, PRDMSRVAL pVal)
{
  RDMSRPARAM	stParam = { 0 };
  ULONG		cbParam;
  ULONG		cbVal;
  ULONG		ulRC;

  stParam.ulReg = ulReg;
  ulRC = DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY, IOCTL_RDMSR_FN_READ,
                      &stParam, sizeof(RDMSRPARAM), &cbParam,
                      pVal, sizeof(RDMSRVAL), &cbVal );

  return ulRC;
}

// ULONG queryCPUIDSignature()
//
// Returns processor signature containing Model, Stepping and Family that can
// be readed with CPUID_TO_xxxxx(id) macros or zero if installed CPU not
// supported by this software (not Intel processor or processor have not
// thermal sensors).

static ULONG queryCPUIDSignature(PULONG pulAPICPhysId)
{
  ULONG		aulCPUInfo[4];

  if ( !_haveCPUID() )
    return 0;	// Have not CPUID.

  _cpuid( &aulCPUInfo, 0 );
  if ( aulCPUInfo[0] < 6 ||		// Check CPUID level.
       aulCPUInfo[1] != (ULONG)'uneG' )	// INTEL ?
    return 0;

  _cpuid( &aulCPUInfo, 6 );
  if ( (aulCPUInfo[0] & 0x00000001 ) == 0 )
    return 0;				// CPU havn't Digital Thermal Sensor.

  _cpuid( &aulCPUInfo, 1 );
  *pulAPICPhysId = (aulCPUInfo[1] >> 24) & 0xFF;

  return aulCPUInfo[0];
}

// ULONG queryAPICPhysId()
//
// Returns processor's APIC ID for current thread.

static ULONG queryAPICPhysId()
{
  ULONG		aulCPUInfo[4];

  _cpuid( &aulCPUInfo, 1 );
  return (aulCPUInfo[1] >> 24) & 0xFF;
}

// ULONG detectTjmax(ULONG ulCPUIDSignature)
//
// Returns Tjunction maximum value for installed processor. The values of the
// sensors will be subtracted from this value for calculating temperature.

static ULONG detectTjmax(ULONG ulCPUIDSignature)
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

  ULONG		ulStepping = CPUID_TO_STEPPING( ulCPUIDSignature );
  ULONG		ulModel = CPUID_TO_MODEL( ulCPUIDSignature );
  RDMSRVAL	stMSR;

  // The mobile Penryn family.
  if ( ulModel == 0x17 && ulStepping == 0x06 )
    return 105;

  // Atom D400, N400 and D500 series.
  if ( ulModel == 0x1C )
    return ulStepping == 0x0A ? 100 : 90;

  // On some Core 2 CPUs, there is an undocumented
  // MSR that tells if Tj(max) is 100 or 85. Note
  // that MSR_IA32_EXT_CONFIG is not safe on all CPUs.
  if ( ( ulModel == 0x0F && ulStepping >= 2 ) ||
       ( ulModel == 0x0E ) )
  {
    if ( _rdmsr( MSR_IA32_EXT_CONFIG, &stMSR ) != NO_ERROR ) // 0x0EE
      return 0; // No access to the driver.

    if ( (stMSR.ulEAX & 0x40000000) != 0 )
      return 85;
  }

  // Attempt to get Tj(max) from IA32_TEMPERATURE_TARGET (0x1A2), but only
  // consider the interval [70, 120] C as valid. It is not fully known which
  // CPU models have the MSR (see models list above).
  // Newer CPUs can tell you what their max temperature is.
  if ( ( ulModel == 0x0E ) ||
       ( ulModel > 0x17 && /* ulModel != 0x1C && -- already checked */
         ulModel != 0x26 && ulModel != 0x27 && ulModel != 0x35 &&
         ulModel != 0x36 ) )
  {
    if ( _rdmsr( MSR_TEMPERATURE_TARGET, &stMSR ) != NO_ERROR )
      return 0; // No access to the driver.

    stMSR.ulEAX = (stMSR.ulEAX >> 16) & 0xFF;
    if ( stMSR.ulEAX >= 70 && stMSR.ulEAX <= 120 )
      return stMSR.ulEAX;
  }

  return DEF_TJMAX;
}

// PCPUTEMP findCPU(ULONG ulAPICPhysId)
//
// Search listed object CPUTEMP by APIC physical Id.
// Returns NULL if CPU not listed.

static PCPUTEMP findCPU(ULONG ulAPICPhysId)
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


// VOID updateCurrentCPU()
//
// Updates data of the current processor, but does not more often than once in
// MIN_DELTA_TIME milliseconds for each processors.

static VOID updateCurrentCPU()
{
  ULONG		ulAPICPhysId = queryAPICPhysId(); // APIC ID of the current CPU.
  PCPUTEMP	pCPU;
  ULONG		ulTimestamp;
  RDMSRVAL	stMSR;

  pCPU = findCPU( ulAPICPhysId );
  if ( pCPU == NULL || pCPU->ulCode == CPU_UNSUPPORTED )
    // We cannot work with CPU which was not initialized (CPU not listed).
    return;

  DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &ulTimestamp, sizeof(ULONG) );
  if ( ( ulTimestamp - pCPU->ulTimestamp ) < MIN_DELTA_TIME )
    // The data have been updated recently.
    return;

  if ( _rdmsr( MSR_IA32_THERM_STATUS, &stMSR ) != NO_ERROR )
  {
    // Driver access failed.
    pCPU->ulCode = CPU_IOCTL_ERROR;
    return;
  }

  if ( (stMSR.ulEAX & 0x80000000) == 0 )
  {
    // Invalid value received,
    pCPU->ulCode = CPU_INVALID_VALUE;
    return;
  }

  if ( stMSR.usCPU != ulAPICPhysId )
  {
    // It is only in theory - can be thread changed to another processor on
    // DosDevIOCtl() ( at _rdmsr() ) ?
    pCPU = findCPU( ulAPICPhysId );
    if ( pCPU == NULL || pCPU->ulCode == CPU_UNSUPPORTED )
      return;
  }

  // Store current temperature value.
  pCPU->ulCurTemp = pCPU->ulMaxTemp - ( (stMSR.ulEAX >> 16) & 0x7F );
  pCPU->ulTimestamp = ulTimestamp;
  pCPU->ulCode = CPU_OK;
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

    updateCurrentCPU();

    DosReleaseMutexSem( hmtxCPUList );

    DosSetPriority( PRTYS_THREAD, PRTYC_IDLETIME, 0, tid );
  }
}

static VOID init()
{
  MPAFFINITY	qwAffinity;
  MPAFFINITY	qwThreadAffinity;
  ULONG		ulStatus;
  ULONG		ulRC;
  PCPUTEMP	pCPU;
  RDMSRVAL	stMSR;
  ULONG		ulIdx;
  ULONG		ulCPUIDSignature;
  BOOL		fFail = FALSE;
  PPIB		pib;
  PTIB		tib;
  ULONG		aulSaveStatus[63];
  ULONG		ulSavePriority = 0;

  // Request number of processors.
  ulRC = DosPerfSysCall( 0x41, 0, (ULONG)&cCPU, 0 );
  if ( ulRC != NO_ERROR )
    return;

  // Open driver DRMSR$.
  ulRC = DosOpen( "RDMSR$", &hDriver, &ulStatus, 0, 0, FILE_OPEN,
                  OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE, NULL );
  if ( ulRC != NO_ERROR )
  {
    hDriver = NULLHANDLE;
    return;
  }

  // Allocate memory for CPUs information.
  paCPU = calloc( cCPU, sizeof(CPUTEMP) );
  if ( paCPU == NULL )
    return;

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

  // Create thread to update information. We will resume it later.
  ulRC = DosCreateThread( &tid, threadQueryTemperature, 0, CREATE_SUSPENDED,
                          4096 );
  if ( ulRC != NO_ERROR )
  {
    tid = 0;
    return;
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

    // CPU signature - used to get Tjunction maximum value.
    ulCPUIDSignature = queryCPUIDSignature( &pCPU->ulAPICPhysId );
    if ( ulCPUIDSignature == 0 )
    {
      pCPU->ulCode = CPU_UNSUPPORTED;
    }
    else
    {
      pCPU->ulMaxTemp = detectTjmax( ulCPUIDSignature );
      if ( ( pCPU->ulMaxTemp == 0 ) ||
           ( _rdmsr( MSR_IA32_THERM_STATUS, &stMSR ) != NO_ERROR ) )
      {
        // Driver IO error.
        pCPU->ulCode = CPU_IOCTL_ERROR;
      }
      else if ( (stMSR.ulEAX & 0x80000000) == 0 )
      {
        // Invalid value received, must be signed number is less than zero.
        pCPU->ulCode = CPU_INVALID_VALUE;
      }
      else
      {
        // Store temperature value.
        pCPU->ulCurTemp = pCPU->ulMaxTemp - ( (stMSR.ulEAX >> 16) & 0x7F );
        pCPU->ulCode = (pCPU->ulCurTemp & 0x80000000) != 0
                ? CPU_INVALID_VALUE   // Invalid value received.
                : CPU_OK;
      }
    }

    // Query system millisecond counter current value.
    DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &pCPU->ulTimestamp,
                     sizeof(ULONG) );

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
  for( ulIdx = 2; ulIdx <= cCPU; ulIdx++ )
    DosSetProcessorStatus( ulIdx, aulSaveStatus[ulIdx - 2] );

  // Set normal affinity.
  DosSetThreadAffinity( &qwThreadAffinity );

  // Restore thread priority.
  if ( ( ulSavePriority != 0 ) &&
       ( DosSetPriority( PRTYS_THREAD, (ulSavePriority >> 8) & 0xFF,
                         0, 0 ) == NO_ERROR ) )
    DosSetPriority( PRTYS_THREAD, 0, ulSavePriority & 0xFF, 0 );

  // Create semaphores and start thread.
  if ( ( !fFail ) &&
       ( ( DosCreateMutexSem( NULL, &hmtxCPUList, 0, FALSE ) != NO_ERROR ) ||
         ( DosCreateEventSem( NULL, &hevStop, 0, FALSE ) != NO_ERROR ) ||
         ( DosResumeThread( tid ) != NO_ERROR )
       ) )
  {
    DosKillThread( tid );
    tid = 0;
  }
}

static VOID destroy()
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
  {
    DosClose( hDriver );
    hDriver = NULLHANDLE;
  }
}

// cputempQuery()
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

  if ( ( tid == 0 ) ||
       // Thread for updates was not runned?
       ( DosRequestMutexSem( hmtxCPUList, SEM_INDEFINITE_WAIT ) != NO_ERROR ) )
       // Mutex semaphore was not created?
    return CPUTEMP_INIT_FAILED;

  // In other times updateCurrentCPU() called from another thread.
  // An additional chance here to get data from the different processors. :)
  updateCurrentCPU();

  // Copy CPUINFO records to the user buffer.
  ulActual = min( cCPU, cCPUInfo );
  if ( pCPUInfo != NULL )
  {
    memcpy( pCPUInfo, paCPU, ulActual * sizeof(CPUTEMP) );

    // Inform user about OFFLINEd CPUs (set ulCode to CPU_OFFLINE).
    for( ulIdx = 1, pCPU = pCPUInfo; ulIdx <= ulActual; ulIdx++, pCPU++ )
    {
      if ( pCPU->ulCode == CPU_UNSUPPORTED )
        continue;

      ulRC = DosGetProcessorStatus( ulIdx, &ulStatus );
      if ( ulRC == NO_ERROR && ulStatus == PROC_OFFLINE )
        pCPU->ulCode = CPU_OFFLINE;
    }
  }

  fOverflow = cCPU > ulActual;
  // Write total number of records at pulActual if user's buffer too small.
  *pulActual = fOverflow ? cCPU : ulActual;

  DosReleaseMutexSem( hmtxCPUList );

  return fOverflow ? CPUTEMP_OVERFLOW : CPUTEMP_OK; 
}

unsigned APIENTRY LibMain(unsigned hmod, unsigned termination) 
{ 
  if ( termination == 0 )
    init();
  else
    destroy();

  return 1; 
}
