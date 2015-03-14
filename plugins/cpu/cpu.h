// System API

#define PROCESSOR_OFFLINE               0
#define PROCESSOR_ONLINE                1

#define CMD_PERF_INFO           0x41
#define CMD_KI_ENABLE           0x60
#define CMD_KI_RDCNT            0x63

typedef APIRET APIENTRY (*PDOSPERFSYSCALL)(ULONG ulCommand, ULONG ulParm1,
                                           ULONG ulParm2, ULONG ulParm3);
typedef APIRET APIENTRY (*PDOSGETPROCESSORSTATUS)(ULONG ulProcNum,
                                                  PULONG pulStatus);
typedef APIRET APIENTRY (*PDOSSETPROCESSORSTATUS)(ULONG ulProcNum,
                                                  ULONG ulStatus);

// DosPerfSysCall(CMD_PERF_INFO,*list,,) returns list of items:
typedef struct _CPUCNT {
  ULLONG		ullTime;	// Time stamp.
  ULLONG		ullIdle;	// Idle time.
  ULLONG		ullBusy;	// Busy time.
  ULLONG		ullIntr;	// Interrupt time.
} CPUCNT, *PCPUCNT;


// Record of CPU list

typedef struct _CPU {
  ULLONG		ullTime;	// Last update time stamp.
  ULLONG		ullBusy;	// Last update busy counter value.
  ULLONG		ullIntr;	// Last update interrupts counter value.
  ULONG			ulLoadIntr;	// Last update interrupt load (0..999).
  GRVAL			stGrVal;	// Graph values storage (busy+intr.).
  BOOL			fStatus;	// Processor status available.
  BOOL			fOnline;	// Processor status.
} CPU, *PCPU;


// Separete rates

#define CPU_SR_NONE		0
#define CPU_SR_CPU0		1
#define CPU_SR_ALL		2

// Defaults

#define DEF_SEPARATERATES	CPU_SR_NONE
// Show real frequency.
#define DEF_REALFREQ		TRUE
// Graph time window, sec.
#define DEF_TIMEWINDOW		(3 * 60)
// Graph background color.
#define DEF_GRBGCOLOR		0x00000000
// Graph frame color.
#define DEF_GRBORDERCOLOR	0x00FFFFFF
// IRQ graph line colors.
#define DEF_IRQ_COLOR		0x00FE0202
// CPU graph lines colors.
#define DEF_COLORS		4
#define DEF_COLOR_1		0x00AAFE9A
#define DEF_COLOR_2		0x00FEFEB6
#define DEF_COLOR_3		0x0000C2FF
#define DEF_COLOR_4		0x00E6C6FE


// Resource

// Icon
#define ID_ICON_CPU		100
#define ID_ICON_CPUOFF		101

// Strings
#define IDS_CPU			16
#define IDS_LOAD		17
#define IDS_USER		18
#define IDS_IRQ			19
#define IDS_FEATURES		20
#define IDS_ONLINE		21
#define IDS_OFFLINE		22
#define IDS_PATHNAME_FIRST_ID	23 // must be higest

// Properties dialog
#define IDD_DSPROPERTIES1	1000
#define IDD_GB_TEMPERATURE	1050
#define IDD_ST_PATHNAME		1051
#define IDD_CB_PATHNAME		1052
#define IDD_RB_TEMP_C		1053
#define IDD_RB_TEMP_F		1054
#define IDD_PB_TEST		1055
#define IDD_CB_REALFREQ		1056
#define IDD_ST_BGCOL		1102
#define IDD_ST_BORDERCOL	1103
#define IDD_GB_GRAPHTIME	1103
#define IDD_SLID_TIME		1104
#define IDD_ST_GRAPHTIME	1105
#define IDD_GB_COLORS		1106
#define IDD_ST_IRQCOL		1200

#define IDD_DSPROPERTIES2	1300
#define IDD_GB_LOADRATES	1301
#define IDD_RB_SRNONE		1302
#define IDD_RB_SRCPU0		1303
#define IDD_RB_SRALL		1304

// Successful determination of the temperature dialog
#define IDD_SUCCESSTEMP		2000
#define IDD_PB_CLOSE		2001
#define IDD_ST_TEMP		2002
#define IDD_EF_TEMP		2003
#define IDD_ST_EMAIL		2004
#define IDD_EF_EMAIL		2005
#define IDD_EF_PATHNAME		2006

// Features dialog
#define IDD_FEATURES		3000
#define IDD_MLE_FEATURES	3001
#define IDD_PB_SELALL		3002
#define IDD_PB_COPY		3003


// Messages
#define IDMSG_PT_OK			1
#define IDMSG_PT_BAD_PATHNAME		2
#define IDMSG_PT_NOT_FOUND		3
#define IDMSG_PT_INVALID_TYPE		4
#define IDMSG_PT_ACPI_INCOMP_VER	5
#define IDMSG_PT_INVALID_VALUE		6

#define IDMSG_PF_CPUSTRING		7
#define IDMSG_PF_STEPPINGID		8
#define IDMSG_PF_MODEL			9
#define IDMSG_PF_FAMILY			10
#define IDMSG_PF_PROCESSORTYPE		11
#define IDMSG_PF_EXTENDEDMODEL		12
#define IDMSG_PF_EXTENDEDFAMILY		13
#define IDMSG_PF_BRANDINDEX		14
#define IDMSG_PF_CLFLUSHCACHELINESIZE	15
#define IDMSG_PF_LOGICALPROCESSORS	16
#define IDMSG_PF_APICPHYSICALID		17
#define IDMSG_PF_FEATURESTITLE		18
#define IDMSG_PF_SSE3INSTRUCTIONS	19
#define IDMSG_PF_MONITORMWAIT		20
#define IDMSG_PF_CPLQUALIFIEDDBGSTORE	21
#define IDMSG_PF_VIRTUALMACHINEEXT	22
#define IDMSG_PF_ENHINTELSPEEDSTEP	23
#define IDMSG_PF_THERMALMONITOR2	24
#define IDMSG_PF_SUPPLEMENTALSSE3	25
#define IDMSG_PF_L1CONTEXTID		26
#define IDMSG_PF_CMPXCHG16B		27
#define IDMSG_PF_XTPRUPDATECONTROL	28
#define IDMSG_PF_PERFDBGCAPABILITYMSR	29
#define IDMSG_PF_SSE41EXTENSIONS	30
#define IDMSG_PF_SSE42EXTENSIONS	31
#define IDMSG_PF_POPCNT			32
#define IDMSG_PF_FEATURE1		33
#define IDMSG_PF_FEATURE2		34
#define IDMSG_PF_FEATURE3		35
#define IDMSG_PF_FEATURE4		36
#define IDMSG_PF_FEATURE5		37
#define IDMSG_PF_FEATURE6		38
#define IDMSG_PF_FEATURE7		39
#define IDMSG_PF_FEATURE8		40
#define IDMSG_PF_FEATURE9		41
#define IDMSG_PF_FEATURE10		42
#define IDMSG_PF_FEATURE11		43
#define IDMSG_PF_FEATURE12		44
#define IDMSG_PF_FEATURE13		45
#define IDMSG_PF_FEATURE14		46
#define IDMSG_PF_FEATURE15		47
#define IDMSG_PF_FEATURE16		48
#define IDMSG_PF_FEATURE17		49
#define IDMSG_PF_FEATURE18		50
#define IDMSG_PF_FEATURE19		51
#define IDMSG_PF_FEATURE20		52
#define IDMSG_PF_FEATURE21		53
#define IDMSG_PF_FEATURE22		54
#define IDMSG_PF_FEATURE23		55
#define IDMSG_PF_FEATURE24		56
#define IDMSG_PF_FEATURE25		57
#define IDMSG_PF_FEATURE26		58
#define IDMSG_PF_FEATURE27		59
#define IDMSG_PF_FEATURE28		60
#define IDMSG_PF_FEATURE29		61
#define IDMSG_PF_FEATURE30		62
#define IDMSG_PF_FEATURE31		63
#define IDMSG_PF_FEATURE32		64

#define IDMSG_PF_LAHF_SAHFAVAILABLE	65
#define IDMSG_PF_CMPLEGACY		66
#define IDMSG_PF_SVM			67
#define IDMSG_PF_EXTAPICSPACE		68
#define IDMSG_PF_ALTMOVCR8		69
#define IDMSG_PF_LZCNT			70
#define IDMSG_PF_SSE4A			71
#define IDMSG_PF_MISALIGNEDSSE		72
#define IDMSG_PF_PREFETCH		73
#define IDMSG_PF_SKINITANDDEV		74
#define IDMSG_PF_SYSCALL_SYSRET		75
#define IDMSG_PF_EXECUTEDISABLEBIT	76
#define IDMSG_PF_MMXEXTENSIONS		77
#define IDMSG_PF_FFXSR			78
#define IDMSG_PF_1GBSUPPORT		79
#define IDMSG_PF_RDTSCP			80
#define IDMSG_PF_64AVAILABLE		81
#define IDMSG_PF_3DNOWEXT		82
#define IDMSG_PF_3DNOW			83
#define IDMSG_PF_NESTEDPAGING		84
#define IDMSG_PF_LBRVISUALIZATION	85
#define IDMSG_PF_FP128			86
#define IDMSG_PF_MOVOPTIMIZATION	87
#define IDMSG_PF_CPUBRANDSTRING		88
#define IDMSG_PF_CACHELINESIZE		89
#define IDMSG_PF_L2ASSOCIATIVITY	90
#define IDMSG_PF_CACHESIZE		91
