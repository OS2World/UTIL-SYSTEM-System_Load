// Properties of data source "os4irq"

typedef struct _IRQ {
  ULONG      ulIRQLevel;
  ULONG      ulCounter;
  ULONG      ulIncrement;
  PSZ        pszAutodetectedName;
  GRVAL      stGrVal;
} IRQ, *PIRQ;

#define IRQ_MAX                  240

#define MAX_IRQNAME_LEN          128

/* MAX_NOT_SHORT_NAME_LEN - For automatically detected names longer than this
   value, a shortened version will be displayed.  */
#define MAX_NOT_SHORT_NAME_LEN   40

#define FLD_NUM                  0
#define FLD_COUNTER              1
#define FLD_LOAD                 2
#define FLD_NAME                 3
#define FLD_LED                  4
#define FLD_INCREMENT            5
#define FLD_FL_HIDDEN            0x8000
#define FIELDS                   6

#define PRESFL_SHOWAUTODETECT    1

typedef struct _IRQPRES {
  PSZ        pszName;            // User defined name
  ULONG      ulFlags;            // PRESFL_xxxxx
} IRQPRES, *PIRQPRES;

typedef struct _IRQPROPERTIES {
  ULONG      ulUpdateInterval;   // In milliseconds
  ULONG      ulTimeWindow;       // In seconds
  ULONG      ulSort;
  BOOL       fSortDesc;
  ULONG      aulFields[FIELDS];  // Fields order, values FLD_*
  BOOL       fNameFixWidth;      // Fixed width of field IRQ name
  IRQPRES    aPres[IRQ_MAX];     // IRQ names and flags
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

#define DEF_THICKLINES          FALSE


/*
    Resource
*/

// Icons

#define ID_ICON_ON			10100
#define ID_ICON_OFF			10101

// IRQ properties dialog

// Page 1
#define IDD_IRQ1			1000
#define IDD_SB_INTERVAL			1001
#define IDD_LB_FIELDS			1002
#define IDD_PB_UP			1003
#define IDD_PB_DOWN			1004
#define IDD_CB_VISIBLE			1005
#define IDD_CB_FIXEDWIDTH		1006
#define IDD_ST_BGCOL			1007
#define IDD_ST_BORDERCOL		1008
#define IDD_ST_LINECOL			1009
#define IDD_CB_THICKLINES		1010
#define IDD_SLID_TIME			1011
#define IDD_ST_GRAPHTIME		1012

// Page 2
#define IDD_IRQ2			1500
#define IDD_CN_NAMES    		1501
#define IDD_SB_IRQ                      1502
#define IDD_CB_SHOW_NAME                1503
#define IDD_EF_USER_DEF_NAME            1504
#define IDD_PB_SET                      1505

// IRQ name dialog

#define IDD_IRQNAME			2000
#define IDD_RB_AUTODETECT               2001
#define IDD_RB_USERDEFINED              2002
#define IDD_EF_AUTODETECT               2003
#define IDD_EF_USERDEFINED              2004

// Strings

#define IDS_FLD_NUM			16
#define IDS_FLD_COUNTER			17
#define IDS_FLD_LOAD			18
#define IDS_FLD_NAME			19
#define IDS_FLD_LED			20
#define IDS_FLD_INCREMENT		21
#define IDS_IPS				22
#define IDS_FLD_SHOW_NAME               25
#define IDS_FLD_USER_DEF_NAME           26
#define IDS_AUTO_DETECTED               27
#define IDS_USER_DEFINED                28
#define IDS_PROP_OPTIONS                29
#define IDS_PROP_NAMES                  30
