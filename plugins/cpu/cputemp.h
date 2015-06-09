#ifndef CPUTEMP_H
#define CPUTEMP_H

// Error codes for cputempQuery().

// CPUTEMP_OK : Success
#define CPUTEMP_OK                 0

// CPUTEMP_NO_DRIVER : The driver RDMSR.SYS is not accessible.
#define CPUTEMP_NO_DRIVER          2

// CPUTEMP_INIT_FAILED : The module was not initialized - not enough memory or
// other system resources.
#define CPUTEMP_INIT_FAILED        4

// CPUTEMP_OVERFLOW : Given buffer too small.
#define CPUTEMP_OVERFLOW	   5


// CPUTEMP.ulCode values.

#define CPU_OK                     0
// CPU_UNSUPPORTED : Not Intel CPU or have no temperature sensors.
#define CPU_UNSUPPORTED            1
// CPU_IOCTL_ERROR : The driver RDMSR.SYS is not accessible.
#define CPU_IOCTL_ERROR            2
// CPU_INVALID_VALUE : Invalid value for sensor received via RDMSR.
#define CPU_INVALID_VALUE          3
// CPU_OFFLINE : Processor OFFLINE status.
#define CPU_OFFLINE                4


typedef struct _CPUTEMP {
  unsigned long  ulId;
                 // Procesor ID numbered 1 through n-1, where there are n
                 // processors in total.

  unsigned long  ulCode;        // CPU_xxxxxxx
  unsigned long  ulAPICPhysId;
  unsigned long  ulMaxTemp;
  unsigned long  ulCurTemp;

  unsigned long  ulTimestamp;
                 // Last ulCurTemp update timestamp in milliseconds.

} CPUTEMP, *PCPUTEMP;


// cputempQuery()
//
// Fills the buffer pCPUInfo up to cCPUInfo records of CPUTEMP.
// Number of actual written items stored in pulActual.
// Returns error code CPUTEMP_xxxxxxx. When an error code CPUTEMP_OVERFLOW
// returned pulActual will contain the number of required records.

unsigned long APIENTRY cputempQuery(unsigned long cCPUInfo, PCPUTEMP pCPUInfo,
                                    unsigned long *pulActual);

#endif // CPUTEMP_H
