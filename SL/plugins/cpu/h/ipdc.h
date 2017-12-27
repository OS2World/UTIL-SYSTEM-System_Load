/* $Id: 1$ */
/** @ipdc.h
 *
 * Inter PSD Device communication module
 *
 * netlabs.org confidential
 *
 * Copyright (c) 2005 netlabs.org
 * Copyright (c) 2011 Mensys, BV
 * Copyright (c) 2011-2014 David Azarewicz <david@88watts.net>
 *
 * Author: Pavel Shtemenko <pasha@paco.odessa.ua>
 * Modified by: David Azarewicz <david@88watts.net>
 *
 * All Rights Reserved
 */
#ifndef __IPDC_H__
#define __IPDC_H__

#include <bsekee.h>

#pragma pack(1)
#define ACPI_IOCTL_CATEGORY         0x88

//  Special function
#define READ_WRITE_EMBEDDED         0x80          // Direct access to embedded controller
//  Public & export function
#define EVALUATE_FUNCTION           0x81
#define GETSYSTEMIFO_FUNCTION       0x82
#define GETTABLEHEADER_FUNCTION     0x83
#define GETTABLE_FUNCTION           0x84
#define WALKNAMESPACE_FUNCTION      0x85
#define GETNAME_FUNCTION            0x86
#define GETOBJECTINFO_FUNCTION      0x87
#define RSGETMETHODDATA_FUNCTION    0x88
#define GETTYPE_FUNCTION            0x89
#define GETPARENT_FUNCTION          0x8A
#define GETNEXTOBJECT_FUNCTION      0x8B
#define WAIT_BUTTON                 0x8C
#define GET_DRIVER_VERSION          0x8D
#define ACPI_ENTER_SLEEP            0x8E
#define GETTIMER_FUNCTION           0x8F
#define GETTIMERRESOLUTION_FUNCTION 0x90
#define PMTIMER_UPTIME_FUNCTION     0x91
#define SET_THROTTLING_C_STATE      0x92
#define GET_THROTTLING              0x93
#define GET_STATISTICS              0x94
#define SET_CPU_POWER_STATE         0x95
#define ENTER_EVENT_THREAD_FUNCTION 0x96
#define WAIT_NOTIFY_FUNCTION        0x97
#define WAIT_EMBEDDED_FUNCTION      0x98
#define FIND_PCI_DEVICE             0x99
#define GETHANDLE_FUNCTION          0x9A
#define ACPI_INTERNAL_FUNCTION      0x9B
#define READ_WRITE_PCI              0x9C          // Direct PCI functions
#define NOTIFY_DEVICE_DRIVERS       0x9D
#define MEMORY_TYPE_SYNC            0x9E
#define GET_EXPORTED_FUNCTION_TABLE 0x9F
/* Function numbers above 0x00ff can only be called via the 32 bit interface */
// Set event flag
#define SET_POWERBUTTON_FLAG        0x100
#define SET_SLEEPBUTTON_FLAG        0x101
// Function for drivers only
#define GET_FUNCTION_ADDRESS        0x211
//NOT_USED #define SET_IDLE_FUNCTION           0x212
#define REGISTERED_NOTIFY           0x213
//NOT_USED #define DEREGISTER_NOTIFY           0x214
//NOT_USED #define SET_POWER_FUNCTION          0x215
//NOT_USED #define SET_THRTL_FUNCTION          0x216
//NOT_USED #define REGISTERED_CPUEXEC          0x217
//NOT_USED #define DEREGISTER_CPUEXEC          0x218
//NOT_USED #define UNSET_IDLE_FUNCTION         0x219

typedef struct _Get_Handle_
{
    ACPI_HANDLE             Parent;
    ACPI_STRING             Pathname;
    ACPI_HANDLE             *RetHandle;
    ACPI_STATUS             Status;
} GETHANDLEPar, *PGETHANDLEPar;

// Parameters for Read/Write Embedded
typedef struct _RW_Embedded_
{
    UINT32                  Number;             // Controller numbers
    UINT32                  Function;           // 0 - read, 1 write
    ACPI_PHYSICAL_ADDRESS   Address;
    UINT32                  BitWidth;
    ACPI_INTEGER            *Value;
    ACPI_STATUS             Status;
} RW_EMBEDDED, *PRW_EMBEDDED;

// Parameters for PCI functions
typedef struct _PCI_Function_
{
    ACPI_PCI_ID             PciId;
    UINT32                  Register;
    UINT64                  Value;
    UINT32                  Width;
    UINT32                  Function;           // 0 - read, 1 write
    ACPI_STATUS             Status;
} PCI_FUNCTION, *PPCI_FUNCTION;
// Parameters for AppComm export
typedef struct _AppCommPar_
{
    ULONG                   NumberFun;                            // What is the function
    void                    *par;                                 // List of parameters for this funtion
} APPCOMMPAR, *PAPPCOMMPAR;
// For PM timer function
typedef struct _PMwakeup_
{
    UINT64                  When;                                // Count when wakeup
    struct _PMwakeup_       *Next;                               // Next struct for check
} PMTWakeUp, *PPMTWakeUp;

typedef struct _PMTimer_
{
    UINT64                  PMCounter;                           // PMTimer counter
    UINT32                  PMValue;
    PPMTWakeUp              Chain;                               // Chain for wakeup
} PMTimer, *PPMTimer;
// Parameters for IRQ hook
typedef struct _PSD_IRQ_HOOK_
{
    ULONG                   *IRQFlag;                             // Address (in PSD) Flag: 1 - handled, 0 - not handled
    ULONG                   IRQNum;                               // Number of IRQ level, 0 based
    PPMTimer                Work;                                 // Address PMTimer struct
    ULONG                   (*Debug)(UINT8 *,UINT32);             // Function for copy debug buffer to Read operation
    ULONG                   (*Handler)(void);                     // Address for IRQ handler ACPI interrupt
} PSDIRQHOOK, *PPSDIRQHOOK;
// Parameters for AcpiEvaluateObject
typedef struct _EvaPar_
{
    ACPI_HANDLE             Object;
    ACPI_STRING             Pathname;
    ACPI_OBJECT_LIST        *ParameterObjects;
    ACPI_BUFFER             *ReturnObjectBuffer;
    ACPI_STATUS             Status;                           // Where returned status from driver call
} EvaPar, *PEvaPar;
// Parameters for AcpiGetSystemInfo
typedef struct _SGIPar_
{
    ACPI_BUFFER             *RetBuffer;
    ACPI_STATUS             Status;                           // Where returned status from driver call
} SGIPar, *PSGIPar;

typedef struct _GHTPar_                                       // AcpiGetTableHeader
{
    ACPI_STRING             Signature;
    UINT32        Instance;
    ACPI_TABLE_HEADER       *OutTableHeader;
    ACPI_STATUS             Status;                           // Where returned status from driver call
} GTHPar, *PGTHPar;

typedef struct _GTPar_                                        // AcpiGetTableHeader
{
    ACPI_STRING             Signature;
    UINT32        Instance;
    ACPI_TABLE_HEADER       **OutTable;
    ACPI_STATUS             Status;                           // Where returned status from driver call
} GTPar, *PGTPar;

typedef struct _WNSPar_
{
    ACPI_OBJECT_TYPE        Type;
    ACPI_HANDLE             StartObject;
    UINT32                  MaxDepth;
    ACPI_WALK_CALLBACK      UserFunction;
    void                    *Context;
    void                    **ReturnValue;
    ACPI_STATUS             Status;                // Where returned status from driver call
} WNSPar, *PWNSPar;
// AcpiGetName
typedef struct _GNPar_
{
    ACPI_HANDLE             Handle;
    UINT32                  NameType;
    ACPI_BUFFER             *RetPathPtr;
    ACPI_STATUS             Status;                // Where returned status from driver call
} GNPar, *PGNPar;
// AcpiGetObjectInfo
typedef struct _GOIPar_
{
    ACPI_HANDLE             Handle;
    ACPI_DEVICE_INFO        *ReturnBuffer;
    ACPI_STATUS             Status;                // Where returned status from driver call
} GOIPar, *PGOIPar;

typedef struct _RSMDPar_
{
    ACPI_HANDLE             Handle;
    char                    *Path;
    ACPI_BUFFER             *RetBuffer;
    ACPI_STATUS             Status;                // Where returned status from driver call
} RSMDPar, *PRSMDPar;
typedef struct _GTyPar_
{
    ACPI_HANDLE             Object;
    ACPI_OBJECT_TYPE        *OutType;
    ACPI_STATUS             Status;                // Where returned status from driver call
} GTyPar, *PGTyPar;

typedef struct _GTpPar_
{
    ACPI_HANDLE             Object;
    ACPI_HANDLE             *OutType;
    ACPI_STATUS             Status;                // Where returned status from driver call
} GTpPar, *PGTpPar;

typedef struct _GNOPar_
{
    ACPI_OBJECT_TYPE        Type;
    ACPI_HANDLE             Parent;
    ACPI_HANDLE             Child;
    ACPI_HANDLE             *OutHandle;
    ACPI_STATUS             Status;                // Where returned status from driver call
} GNOPar, *PGNOPar;

typedef struct _SetCPUPower_
{
    UINT32                  CPUid;                 // CPU for set
    UINT32                  State;                 // State number for ser
    ACPI_STATUS             Status;                // Where returned status from driver call
} CPUPowerPar, *PCPUPowerPar;

typedef struct _Throttling_
{
    UINT32                  Value;                 // Throtlling value
    UINT8                   C_State;               // Cx state
    UINT8                   ProcId;                // Processor Id from evaluate
    UINT8                   Enable;                // Info, enable throttling or no
    UINT8                   DutyWidth;             // Readonly, value DUTY_WIDTH from FADT
    ACPI_STATUS             Status;                // Where returned status from driver call
} THROTTLING_C_STATE, *PTHROTTLING_C_STATE;

typedef struct _EmmbeddedController_
{
    ACPI_GENERIC_ADDRESS    EcControl;      // Control register
    ACPI_GENERIC_ADDRESS    EcData;         // Data register
    UINT32                  Uid;            //
    UINT8                   GpeBit;         // Number of GPE
    UINT8                   *EcId;          // Name in acpi tree
    ACPI_HANDLE             Handle;         // Handle to EC object
    ACPI_MUTEX              EC_MTX;         // Mutex for accessing the EC registers
    unsigned                Initialized:1;  // The EC has been initialized
    unsigned                SciQueued:1;    // an SCI event has been queued
    unsigned                GlobalLockNeeded:1;
} EMBEDDED_CONTROLLER, *PEMBEDDED_CONTROLLER;

typedef struct _EvaluateName_
{
    ACPI_HANDLE             Handle;                         // Handle to root name
    UINT8                   Number;
    char                    EvalString[5];                  // Name for evaluate
} EvaluateName, *PEvaluateName;

typedef struct _EventNotify_
{
    ACPI_HANDLE             Handle; // Device was notification
    UINT16                  Value;  // Number of notification
    UINT16                  Type;   // Who System/Device
} EventNotify, *PEventNotify;

typedef struct _R3Notify_
{
    UINT32                  R3Counter;
    UINT32                  Timeout;
    ACPI_HANDLE             ReqHandle;
    UINT16                  ReqType;
    UINT16                  ReqValue;
    EventNotify             Notify;
} R3Notify, *PR3Notify;

typedef void  (ACPI_INTERNAL_VAR_XFACE *ACPINOTIFY)( PEventNotify Notify);

typedef struct _DrvNotify_
{
    ACPINOTIFY              Driver;
    UINT32                  Counter;
    struct _DrvNotify_      *Next;
} DriverNotify, *PDriverNotify;

typedef struct _OsExecute_
{
    ACPI_OSD_EXEC_CALLBACK  Function;
    void                    *Context;
} OSEXECUTE, *POSEXECUTE;

typedef void  (APIENTRY *ACPICPUEXEC)(UINT32 CPUNum, void *Context);

typedef struct _CpuExecute_
{
    ACPICPUEXEC             Function;
    void                    *Context;
} CPUEXECUTE, *PCPUEXECUTE;

typedef struct _GoToSleepState_
{
    UINT32                  State;
    ACPI_STATUS             Status;
} GoToSleepStatePar, *PGoToSleepStatePar;

typedef struct _NotifyDrivers_
{
    UINT32 Type;
    UINT32 Function;
} NotifyDrivers, *PNotifyDrivers;

#define NOTIFY_QUEUE_MASK 0xff
#define NOTIFY_QUEUE_SIZE 0x100

typedef struct _WaitTick_
{
    UINT32                  WaitEnd;                         // Flag "wait overflow counter"
    UINT32                  Start;                           // Start value of ticks
    UINT32                  Current;                         // Current value of ticks, need for clear WaitEnd
    UINT32                  Need;                            // Need value
} WAITTICK, *PWAITTICK;

ACPI_STATUS AcpiStartWaitPMTics(PWAITTICK w,UINT32 Tics);
UINT32 AcpiCheckWaitPMTics(PWAITTICK w);
UINT32 MicroSecondsToTics(UINT32 MicroSeconds);

#pragma pack()

#endif
