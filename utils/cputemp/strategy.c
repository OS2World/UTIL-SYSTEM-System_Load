#include "devdefs.h"
#include "devhelp.h"
#include "devreqp.h"
#include "debug.h"

#define IOCTL_CATECORY		0x92
#define IOCTL_FN_READ		0x01

#define PTR_SEL(ptr) (USHORT)(((ULONG)ptr >> 16) & 0x0000FFFF)
#define PTR_OFFS(ptr) (ULONG)ptr & 0x0000FFFF

PFN Device_Help;  // DevHelp Interface Address

extern VOID drvInit(REQP_INIT FAR *rp);

// VOID drvIOCtl(REQP_IOCTL FAR *rp)
//
// IOCtl requests ( DosDevIOCtl() ) handler. I tried to maintain compatibility
// with nickk's driver.

static VOID drvIOCtl(REQP_IOCTL FAR *rp)
{
  ULONG		ulVal1, ulVal2 = 0;
  USHORT	usCPU = 0;

  if ( rp->category != IOCTL_CATECORY ||
       rp->function != IOCTL_FN_READ )
  {
    rp->header.status = RPDONE | RPERR | RPERR_COMMAND;
    return;
  }

  if ( ( rp->paramlen < (2 * sizeof(ULONG)) ) ||
       DevHelp_VerifyAccess( PTR_SEL(rp->param), rp->paramlen,
                             PTR_OFFS(rp->param), VERIFY_READONLY ) )
  {
    rp->header.status = RPDONE | RPERR | RPERR_PARAMETER;
    return;
  }

  if ( ( rp->datalen < (3 * sizeof(ULONG)) ) ||
       DevHelp_VerifyAccess( PTR_SEL(rp->data), rp->datalen,
                             PTR_OFFS(rp->data), VERIFY_READWRITE ) )
  {
    rp->header.status = RPDONE | RPERR | RPERR_GENERAL;
    return;
  }

  ulVal1 = ((PULONG)rp->param)[0];
  __asm {
    // Query given MSR.
    mov     ecx, ulVal1
    rdmsr
    mov     ulVal1, eax
    mov     ulVal2, edx
    // Query current CPU No.
    mov     eax, 1
    cpuid
    shr     ebx, 18h
    and     bx, 0FFh
    mov     usCPU, bx
  };
  ((PULONG)rp->data)[0] = ulVal1;   // ulEAX
  ((PULONG)rp->data)[1] = ulVal2;   // ulEDX
  ((PUSHORT)rp->data)[4] = usCPU;   // usCPU
  ((PUSHORT)rp->data)[5] = 1;       // usTries

  rp->header.status = RPDONE;
  return;
}

// Strategy entry point.

#pragma aux STRATEGY far parm [es bx];
#pragma aux (STRATEGY) Strategy;

VOID Strategy(REQP_ANY FAR *rp)
{
  if ( rp->header.command < RP_END )
  {
    switch( rp->header.command )
    {
      case RP_INIT:
        drvInit( (REQP_INIT FAR *)rp );
        break;

      case RP_IOCTL:
        drvIOCtl( (REQP_IOCTL FAR *)rp );
        break;

      case RP_READ:
      case RP_WRITE:
        rp->header.status = RPERR_COMMAND | RPDONE;
        break;

      case RP_READ_NO_WAIT:
      case RP_INPUT_STATUS:
      case RP_INPUT_FLUSH:
      case RP_WRITE_VERIFY:
      case RP_OUTPUT_STATUS:
      case RP_OUTPUT_FLUSH:
      case RP_OPEN:
      case RP_CLOSE:
        rp->header.status = RPDONE;
        break;

      default:
        rp->header.status = RPERR_COMMAND | RPDONE;
    }
  }
  else
    rp->header.status = RPERR_COMMAND | RPDONE;
}
