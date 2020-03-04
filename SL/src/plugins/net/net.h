// Properties of data source "Net"

typedef struct _PROPERTIES {
  ULONG      ulSort;
  BOOL       fSortDesc;
  ULONG      ulUpdateInterval;   // msec.
  ULONG      ulTimeWindow;       // sec
} PROPERTIES, *PPROPERTIES;

// Defaults

#define DEF_GRBGCOLOR            0x00000000
#define DEF_GRBORDERCOLOR        0x00FFFFFF
#define DEF_RXCOLOR              0x00F00000
#define DEF_TXCOLOR              0x0030D020
#define DEF_THICKLINES           FALSE
#define DEF_INTERVAL             1000
#define DEF_TIMEWIN              120

// Properties dialog

#define IDD_DSPROPERTIES         1000

#define IDD_GB_COLORS            1100
#define IDD_ST_BGCOL             1101
#define IDD_ST_BORDERCOL         1102
#define IDD_ST_RXCOL             1103
#define IDD_ST_TXCOL             1104
#define IDD_CB_THICKLINES        1105

#define IDD_GB_GRAPHTIME         1200
#define IDD_SLID_TIME            1201
#define IDD_ST_GRAPHTIME         1202
#define IDD_ST_INTERVAL          1203
#define IDD_SB_INTERVAL          1204
#define IDD_ST_SECONDS           1205

// Strings

#define IDS_DS_TITLE   1
#define IDS_FLD_INDEX  2
#define IDS_FLD_DESCR  3
#define IDS_FLD_TYPE   4
#define IDS_FLD_MAC    5
#define IDS_FLD_SENT   6
#define IDS_FLD_RECV   7
#define IDS_FLD_TXSPD  8
#define IDS_FLD_RXSPD  9
#define IDS_SPEED     10
