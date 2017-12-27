/*
    OS/4 RDMSR$

    Copyright (c) OS/4 Team 2017
    All Rights Reserved

   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ

*/

#include <ddk4/os4types.h>
#include <os2.h>
#include <ddk4/bsekee.h>
#include <ddk4/reqpkt4.h>

extern KEERET KEEENTRY IOCtl(RP_GENIOCTL *rp);       // ioctl.c

// Public flag. It will be set on command RP_INIT_COMPLETE and will be checked
// in ioctl (see function IOCTL_RDMSR_FN_CPUID handler).
BOOL fCPUIDEnabled = FALSE;

/* ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
   InitComplete()
   Will be called in R0 when all drvers are loaded.
   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ */

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
"    not     ecx           " \
"no_cpuid:                 " \
  value [ecx] modify[eax ebx];

KEERET KEEENTRY InitComplete(RP_GENIOCTL *ppkt)
{
  // Check that the processor supports command CPUID.
  fCPUIDEnabled = _haveCPUID();

  return 0;
}


/* ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
   Strategy routine for RDMSR
   ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ */

void KEEENTRY DriverEntry(Far16Ptr_t reqpack)
{
  KEERET     rc = 0;
  REQP_ANY   *rp;

  KernThunkStackTo32();

  //  ª®­¢¥àâ¨àã¥¬ 16:16 ­  0:32
  rp = (REQP_ANY*)KernSelToFlat( reqpack );

  //KernPrintf("RDMSR command 0x%2x\n",rp->header.Cmd);

  switch( rp->header.Cmd )
  {
    //  ¨­¨â áî¤  ­¥ ¯®¯ ¤ ¥â  - ®­ ®áâ ¥âáï ¢ 16 ¡¨â ª®¤¥
    case RP_INIT_COMPLETE:
      rc = InitComplete( (RP_GENIOCTL *)rp );
      break;

    case RP_IOCTL:
      rc = IOCtl( (RP_GENIOCTL *)rp );
      break;

    default:
      rc = RPERR | RPERR_COMMAND;
  }

  if ( rc != 0 )
    rc |= RPERR;

  rp->header.Status = rc | RPDONE;

  KernThunkStackTo16();
}
