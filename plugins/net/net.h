// Properties of data source "Net"

typedef struct _PROPERTIES {
  ULONG			ulSort;
  BOOL			fSortDesc;
  ULONG			ulUpdateInterval;	// msec.
  ULONG			ulTimeWindow;		// sec
} PROPERTIES, *PPROPERTIES;

// Defaults

#define DEF_GRBGCOLOR		0x00000000
#define DEF_GRBORDERCOLOR	0x00FFFFFF
#define DEF_RXCOLOR		0x00F00000
#define DEF_TXCOLOR		0x0030D020
#define DEF_INTERVAL		1000
#define DEF_TIMEWIN		120

// Properties dialog

#define IDD_DSPROPERTIES	1000

#define IDD_GB_COLORS		1100
#define IDD_ST_BGCOL		1101
#define IDD_ST_BORDERCOL	1102
#define IDD_ST_RXCOL		1103
#define IDD_ST_TXCOL		1104

#define IDD_GB_GRAPHTIME	1200
#define IDD_SLID_TIME		1201
#define IDD_ST_GRAPHTIME	1202
#define IDD_ST_INTERVAL		1203
#define IDD_SB_INTERVAL		1204
#define IDD_ST_SECONDS		1205

// Strings

#define IDS_FLD_INDEX		100
#define IDS_FLD_DESCR		101
#define IDS_FLD_TYPE		102
#define IDS_FLD_MAC		103
#define IDS_FLD_SENT		104
#define IDS_FLD_RECV		105
#define IDS_FLD_TXSPD		106
#define IDS_FLD_RXSPD		107
#define IDS_SPEED		200
