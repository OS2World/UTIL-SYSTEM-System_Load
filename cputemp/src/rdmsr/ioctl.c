/*
    OS/4 RDMSR$

    Copyright (c) OS/4 Team 2017
    All Rights Reserved

   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

*/
#include <ddk4/os4types.h>
#include <os2.h>
#include <ddk4/bsekee.h>
#include <ddk4/bseerr.h>
#include <ddk4/reqpkt4.h>
#include "rdmsr.h"

#ifdef DEBUGCODE
#define debug(s,...) \
  do { if ( (ULONG)&KernKEEVersion < 0x00010006 ) \
  KernPrintf("RDMSR %s(): "s"\r\n", __FUNCTION__, ##__VA_ARGS__); } while( 0 )
#else
#define debug(s,...)
#endif

extern BOOL fCPUIDEnabled;       // strat.c


/* ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
    _ioctlfnReadCurrentCPU()
    Reads the MSR value on the current processor.
   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ */

KEERET KEEENTRY _ioctlfnReadCurrentCPU(RP_GENIOCTL *ppkt,
                                       PRDMSRPARAM pPktIn)
{
  RDMSRVAL             stPktOut;
  ULONG                ulVal1, ulVal2 = 0;
  USHORT               usAPICPhysId = 0;

  if ( ppkt->DataLen < sizeof(stPktOut) )
    return ERROR_INVALID_PARAMETER;

  ulVal1 = pPktIn->ulReg;
  __asm
  {
    // Query given MSR.
    mov     ecx, ulVal1
    rdmsr
    mov     ulVal1, eax
    mov     ulVal2, edx

    // Query current CPU physical Id.
    mov     eax, 1
    cpuid
    shr     ebx, 18h
    and     bx, 0FFh
    mov     usAPICPhysId, bx
  }
  stPktOut.ulEAX        = ulVal1;
  stPktOut.ulEDX        = ulVal2;
  stPktOut.usAPICPhysId = usAPICPhysId;
  stPktOut.usTries      = 1;     // We keep compatibility with nickk's driver.

  if ( KernCopyOut( (void*)KernSelToFlat(ppkt->DataPacket),
                    &stPktOut, sizeof(stPktOut) ) != NO_ERROR )
    return RPERR_PARAMETER;

  return NO_ERROR;
}


/* ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
    _ioctlfnReadSpeCPU() : OS/4-related function.
    Reads the MSR value on the specified processor.
   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ */

// Data for callback function.
typedef struct _SPECBDATA {
  ULONG      ulReg;              // MSR no. to read.
  PRDMSRVAL  pVal;               // Pointer to values of given MSR.
} SPECBDATA, *PSPECBDATA;

// The call-back function will be executed on the specified processor.
static void KEEENTRY _speCallBack(void *pData)
{
  PSPECBDATA           pCBData = (PSPECBDATA)pData;
  ULONG                ulVal1  = pCBData->ulReg;
  ULONG                ulVal2  = 0;
  USHORT               usAPICPhysId = 0;

  __asm
  {
    // Query given MSR.
    mov     ecx, ulVal1
    rdmsr
    mov     ulVal1, eax
    mov     ulVal2, edx

    // Query current CPU physical Id.
    mov     eax, 1
    cpuid
    shr     ebx, 18h
    mov     usAPICPhysId, bx
  }

  // Store MSR value and CPU id to the output array.
  pCBData->pVal->ulEAX        = ulVal1;
  pCBData->pVal->ulEDX        = ulVal2;
  pCBData->pVal->usAPICPhysId = usAPICPhysId;
  pCBData->pVal->usTries      = 1;
}

KEERET KEEENTRY _ioctlfnReadSpeCPU(RP_GENIOCTL *ppkt, PRDMSRPARAM pPktIn)
{
  RDMSRVAL             stPktOut;
  SPECBDATA            stCBData;

  if ( (ULONG)&KernKEEVersion < 0x00010006 )
  {
    debug( "Unsupported kernel" );
    return ERROR_I24_GEN_FAILURE;
  }

  if ( ( ppkt->DataLen < sizeof(stPktOut) ) ||
       ( pPktIn->usCPU >= KernGetNumCpus() ) )
  {
    debug( "Not enough output buf. space or requested CPU %u is not present.",
           pPktIn->usCPU );
    return ERROR_INVALID_PARAMETER;
  }

  stCBData.ulReg = pPktIn->ulReg;
  stCBData.pVal  = &stPktOut;
  KernInvokeAtSpecificCpu( _speCallBack, &stCBData, pPktIn->usCPU );

  if ( KernCopyOut( (void*)KernSelToFlat(ppkt->DataPacket),
                    &stPktOut, sizeof(stPktOut) ) != NO_ERROR )
  {
    debug( "KernCopyOut() failed" );
    return RPERR_PARAMETER;
  }

  return NO_ERROR;
}


/* ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
    _ioctlfnReadAllCPU() : OS/4-related function.
    Reads the MSR value on each processor.
   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ */

#define _MAX_CPU       64

// In/out data for callback function.
typedef struct _RENDCBDATA {
  ULONG      ulReg;              // Input for _rendCallBack() - MSR no. to read.
  RDMSRVAL2  aVal[_MAX_CPU];     // Output - values of given MSR for each CPU.
} RENDCBDATA, *PRENDCBDATA;

// The call-back function will be executed on each processor.
static void KEEENTRY _rendCallBack(void *pData)
{
  PRENDCBDATA          pCBData = (PRENDCBDATA)pData;
  ULONG                ulVal1  = pCBData->ulReg;
  ULONG                ulVal2  = 0;
  UCHAR                ucAPICPhysId = 0;
  PRDMSRVAL2           pVal;
  UCHAR                ucCPU;

  __asm
  {
    // Query given MSR.
    mov     ecx, ulVal1
    rdmsr
    mov     ulVal1, eax
    mov     ulVal2, edx

    // Query current CPU physical Id.
    mov     eax, 1
    cpuid
    shr     ebx, 18h
    mov     ucAPICPhysId, bl
  }

  // Store MSR value and CPU id to the output array.
  ucCPU = KernGetCurrentCpuId();
  pVal  = &pCBData->aVal[ucCPU];
  pVal->ucCPU        = ucCPU;
  pVal->ulEAX        = ulVal1;
  pVal->ulEDX        = ulVal2;
  pVal->ucAPICPhysId = ucAPICPhysId;
}

static KEERET KEEENTRY _ioctlfnReadAllCPU(RP_GENIOCTL *ppkt,
                                          PRDMSRPARAM pPktIn)
{
  RENDCBDATA           stCBData;
  PRDMSRVAL2           pVal, pValPack;
  ULONG                ulIdx, cbData;

  if ( (ULONG)&KernKEEVersion < 0x00010006 )
  {
    debug( "Unsupported kernel" );
    return ERROR_I24_GEN_FAILURE;
  }

  stCBData.ulReg = pPktIn->ulReg;
  for( ulIdx = 0; ulIdx < _MAX_CPU; ulIdx++ )
    stCBData.aVal[ulIdx].ucCPU = 0xFF;   // "Not complited" flag.

  KernInvokeAtEachCpu( _rendCallBack, &stCBData,
                       IAECPU_IN_BARRIER_ON | IAECPU_OUT_BARRIER_ON );

  // Pack data: move all the completed records to the beginning of the array.
  pValPack = stCBData.aVal;
  for( ulIdx = 0, pVal = stCBData.aVal; ulIdx < _MAX_CPU; ulIdx++, pVal++ )
  {
    if ( pVal->ucCPU == 0xFF )
      // Record was not complited.
      continue;

    *pValPack = *pVal;
    pValPack++;
  }

  cbData = ((PUCHAR)pValPack) - ((PUCHAR)stCBData.aVal);
  if ( ppkt->DataLen < cbData )
  {
    debug( "Not enough space for the data packet" );
    ppkt->DataLen = cbData;
    return RPERR_PARAMETER;
  }

  if ( KernCopyOut( (void*)KernSelToFlat( ppkt->DataPacket ),
                    stCBData.aVal, cbData ) != NO_ERROR )
  {
    debug( "KernCopyOut() failed" );
    return RPERR_PARAMETER;
  }

  return NO_ERROR;
}


/* ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
    _ioctlfnCPUID() : OS/4-related function.
    Reads the CPUID information on the specified processor.
   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ */

// Data for callback function.
typedef struct _CPUIDCBDATA {
  ULONG        ulEAX;            // Category of information for CPUID.
  PRDMSRCPUID  pVal;             // CPUID putput information.
} CPUIDCBDATA, *PCPUIDCBDATA;

// The call-back function will be executed on the specified processor.
static void KEEENTRY _cpuidCallBack(void *pData)
{
  PCPUIDCBDATA         pCBData = (PCPUIDCBDATA)pData;
  ULONG                ulEAX = pCBData->ulEAX;
  ULONG                ulEBX = 0;
  ULONG                ulECX = 0;
  ULONG                ulEDX = 0;

  __asm
  {
    mov     eax, ulEAX
    cpuid
    mov     ulEAX, eax
    mov     ulEBX, ebx
    mov     ulECX, ecx
    mov     ulEDX, edx
  }
  // Store CPUID output data.
  pCBData->pVal->ulEAX = ulEAX;
  pCBData->pVal->ulEBX = ulEBX;
  pCBData->pVal->ulECX = ulECX;
  pCBData->pVal->ulEDX = ulEDX;
}

KEERET KEEENTRY _ioctlfnCPUID(RP_GENIOCTL *ppkt, PRDMSRPARAM pPktIn)
{
  RDMSRCPUID           stPktOut;
  CPUIDCBDATA          stCBData;

  if ( (ULONG)&KernKEEVersion < 0x00010006 )
  {
    debug( "Unsupported kernel" );
    return ERROR_I24_GEN_FAILURE;
  }

  if ( ( ppkt->DataLen < sizeof(stPktOut) ) ||
       ( pPktIn->usCPU >= KernGetNumCpus() ) )
  {
    debug( "Not enough output buf. space or requested CPU %u is not present.",
           pPktIn->usCPU );
    return ERROR_INVALID_PARAMETER;
  }

  if ( !fCPUIDEnabled )          // It was detected on RP_INIT_COMPLETE command.
  {
    debug( "CPUID not supported" );
    return ERROR_I24_GEN_FAILURE;
  }

  stCBData.ulEAX = pPktIn->ulReg;
  stCBData.pVal  = &stPktOut;
  KernInvokeAtSpecificCpu( _cpuidCallBack, &stCBData, pPktIn->usCPU );

  if ( KernCopyOut( (void*)KernSelToFlat(ppkt->DataPacket),
                    &stPktOut, sizeof(stPktOut) ) != NO_ERROR )
  {
    debug( "KernCopyOut() failed" );
    return RPERR_PARAMETER;
  }

  return NO_ERROR;
}


/* ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ */
KEERET KEEENTRY IOCtl(RP_GENIOCTL *ppkt)
{
  KEERET               rc;
  RDMSRPARAM           stPktIn;

  if ( ppkt->Category != IOCTL_RDMSR_CATECORY )
    // wrong category
    return ERROR_INVALID_CATEGORY;

  if ( KernCopyIn( &stPktIn, (uint8_t *)KernSelToFlat( ppkt->ParmPacket ),
                   sizeof(RDMSRPARAM) ) != NO_ERROR )
  {
    debug( "KernCopyIn() failed" );
    return ERROR_INVALID_PARAMETER;
  }

  switch( ppkt->Function )
  {
    case IOCTL_RDMSR_FN_READCURCPU:
      rc = _ioctlfnReadCurrentCPU( ppkt, &stPktIn );
      break;

    case IOCTL_RDMSR_FN_READSPECPU:
      rc = _ioctlfnReadSpeCPU( ppkt, &stPktIn );
      break;

    case IOCTL_RDMSR_FN_READALLCPU:
      rc = _ioctlfnReadAllCPU( ppkt, &stPktIn );
      break;

    case IOCTL_RDMSR_FN_CPUID:
      rc = _ioctlfnCPUID( ppkt, &stPktIn );
      break;

    default:
      debug( "Invalid function %u requested", ppkt->Function );
      rc = ERROR_I24_BAD_COMMAND;
 }

 return rc;
}
