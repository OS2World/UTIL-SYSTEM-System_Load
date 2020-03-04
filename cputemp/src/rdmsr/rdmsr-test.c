#define INCL_DOSDEVICES
#define INCL_DOSERRORS
//#define INCL_DOSMISC
#include <os2.h>
#include <string.h>
#include <stdio.h>
#include "rdmsr.h"

int main(int argc, char* argv[])
{
  ULONG      ulRC;
  HFILE      hDriver;
  ULONG      ulStatus;
  ULONG      cbParam;
  ULONG      cbVal;
  RDMSRPARAM stParam   = { 0 };
  RDMSRVAL   stVal;
  RDMSRVAL2  aVal[65];
  RDMSRCPUID stCPUID;
  ULONG      ulIdx;

  ulRC = DosOpen( "RDMSR$", &hDriver, &ulStatus, 0, 0, FILE_OPEN,
                  OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE, NULL );
  if ( ulRC != NO_ERROR )
  {
    puts( "RDMSR$ open failed" );
    return 1;
  }


  // Read the MSR value on current processor.
  // It shuld work on systems with any kernel.

  puts( "Test RDMSR$ driver functions." );

  stParam.ulReg = MSR_IA32_THERM_STATUS;
  printf( "\nMSR 0x%X - Current processor (IOCTL_RDMSR_FN_READCURCPU)\n  ",
          stParam.ulReg );
  ulRC = DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY, IOCTL_RDMSR_FN_READCURCPU,
                      &stParam, sizeof(RDMSRPARAM), &cbParam,
                      &stVal, sizeof(RDMSRVAL), &cbVal );
  if ( ulRC != NO_ERROR )
    puts( "Failed" );
  else
    printf( "Ok: EDX:EAX = 0x%X:0x%X\n", stVal.ulEDX, stVal.ulEAX );


  // Read the MSR value on specified processor.
  // This function is available only on kernel with "rendezvous" support.

  stParam.ulReg = 0x19C;
  stParam.usCPU = 0;
  printf( "\nMSR 0x%X - Processor %u (IOCTL_RDMSR_FN_READSPECPU)\n  ",
          stParam.ulReg, stParam.usCPU );
  ulRC = DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY, IOCTL_RDMSR_FN_READSPECPU,
                      &stParam, sizeof(RDMSRPARAM), &cbParam,
                      &stVal, sizeof(RDMSRVAL), &cbVal );
  if ( ulRC != NO_ERROR )
    puts( "Failed (kernel without \"rendezvous\" support?)" );
  else
    printf( "Ok: EDX:EAX = 0x%X:0x%X\n", stVal.ulEDX, stVal.ulEAX );


  // Read the MSR value on all processors.
  // This function is available only on kernel with "rendezvous" support.

  memset( aVal, 0xFF, sizeof(aVal) );
  stParam.ulReg = 0x19C;
  printf( "\nMSR 0x%X - All processors (IOCTL_RDMSR_FN_READALLCPU)\n",
          stParam.ulReg, stParam.usCPU );
  ulRC = DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY, IOCTL_RDMSR_FN_READALLCPU,
                      &stParam, sizeof(RDMSRPARAM), &cbParam,
                      aVal, sizeof(aVal), &cbVal );
  if ( ulRC != NO_ERROR )
    puts( "  Failed (kernel without \"rendezvous\" support?)" );
  else
  {
    for( ulIdx = 0; ulIdx < 64; ulIdx++ )
    {
      if ( aVal[ulIdx].ucCPU == 0xFF )
        break;

      printf( "  Ok: CPU %u, EDX:EAX = 0x%X:0x%X\n",
              aVal[ulIdx].ucCPU, aVal[ulIdx].ulEDX, aVal[ulIdx].ulEAX );
    }
  }


  // Read CPUID information.
  // This function is available only on kernel with "rendezvous" support.

  stParam.ulReg = 0; // EAX for CPUID
  stParam.usCPU = 0; // CPU No
  printf( "\nCPUID (CPU string) - Processor %u (IOCTL_RDMSR_FN_GETCPUID)\n  ",
          stParam.ulReg, stParam.usCPU );
  ulRC = DosDevIOCtl( hDriver, IOCTL_RDMSR_CATECORY, IOCTL_RDMSR_FN_CPUID,
                      &stParam, sizeof(RDMSRPARAM), &cbParam,
                      &stCPUID, sizeof(RDMSRCPUID), &cbVal );
  if ( ulRC != NO_ERROR )
    puts( "Failed (kernel without \"rendezvous\" support or cpuid is not supported by \n"
          "          the installed processor?)" );
  else
  {
    CHAR     acCPUString[0x20];

    memset( acCPUString, 0, sizeof(acCPUString) );
    *((PULONG)&acCPUString[0]) = stCPUID.ulEBX;
    *((PULONG)&acCPUString[4]) = stCPUID.ulEDX;
    *((PULONG)&acCPUString[8]) = stCPUID.ulECX;
    printf( "Ok: %s\n", acCPUString );
  }


  DosClose( hDriver );
  return 0;
}
