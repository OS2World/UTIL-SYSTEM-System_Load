// Properties of data source "os4irq"

// return to os4irq.c???
typedef struct _IRQ {
  ULONG		ulNum;
  ULONG		ulCounter;
  ULONG		ulIncrement;
  GRVAL		stGrVal;
} IRQ, *PIRQ;

#define IRQ_MAX			240
#define MAX_IRQNAME_LEN		128
#define FLD_NUM			0
#define FLD_COUNTER		1
#define FLD_LOAD		2
#define FLD_NAME		3
#define FLD_LED			4
#define FLD_INCREMENT		5
#define FLD_FL_HIDDEN		0x8000
#define FIELDS			6

typedef struct _IRQPROPERTIES {
  ULONG		ulUpdateInterval;	// in milliseconds
  ULONG		ulTimeWindow;		// in seconds
  ULONG		ulSort;
  BOOL		fSortDesc;
  ULONG		aulFields[FIELDS];	// fields order, values FLD_*
  BOOL		fNameFixWidth;		// fixed width of field IRQ name
  PSZ		apszNames[IRQ_MAX];	// IRQ names given by user
} IRQPROPERTIES, *PIRQPROPERTIES;

// Defaults

#define DEF_INTERVAL		1000
#define DEF_TIMEWIN		120
// Fields order in item's window
#define DEF_ITEMFLD_0		FLD_LED
#define DEF_ITEMFLD_1		FLD_NUM
#define DEF_ITEMFLD_2		FLD_NAME
#define DEF_ITEMFLD_3		FLD_COUNTER
#define DEF_ITEMFLD_4		FLD_LOAD
#define DEF_ITEMFLD_5		(FLD_INCREMENT | FLD_FL_HIDDEN)
#define DEF_ITEMNAMEFIXWIDTH	TRUE
// Colors
#define DEF_GRBGCOLOR		0x00000000
#define DEF_GRBORDERCOLOR	0x00FFFFFF
#define DEF_LINECOLOR		0x0020E010

// Icons

#define ID_ICON_ON			10100
#define ID_ICON_OFF			10101

// IRQ properties dialog

#define IDD_IRQ				1000

#define IDD_GB_NAMES			1101
#define IDD_ST_NO			1102
#define IDD_EF_NO			1103
#define IDD_ST_NAME			1104
#define IDD_EF_NAME			1105
#define IDD_PB_SET			1106
#define IDD_LB_NAMES			1107

#define IDD_GB_IRQLIST			1201
#define IDD_ST_INTERVAL			1202
#define IDD_SB_INTERVAL			1203
#define IDD_ST_SECONDS			1204
#define IDD_LB_FIELDS			1205
#define IDD_PB_UP			1206
#define IDD_PB_DOWN			1207
#define IDD_CB_VISIBLE			1208
#define IDD_CB_FIXEDWIDTH		1209

#define IDD_GB_COLORS			1300
#define IDD_ST_BGCOL			1301
#define IDD_ST_BORDERCOL		1302
#define IDD_ST_LINECOL			1303

#define IDD_GB_GRAPHTIME		1401
#define IDD_SLID_TIME			1402
#define IDD_ST_GRAPHTIME		1403

// IRQ name dialog

#define IDD_IRQNAME			2000

// Strings

#define IDS_FLD_NUM			16
#define IDS_FLD_COUNTER			17
#define IDS_FLD_LOAD			18
#define IDS_FLD_NAME			19
#define IDS_FLD_LED			20
#define IDS_FLD_INCREMENT		21
#define IDS_IPS				22
#define IDS_INVALID_NUM_TITLE		23
#define IDS_INVALID_NUM_TEXT		24

// trash
#define IDD_CB_LED 1234
#define IDD_CB_COUNTER 1234
#define IDD_CB_INCR 1234

