// Presentation
// ------------

#define ITEM_VPAD		10
#define ITEM_HPAD		5

// INI-file
// --------

#define INI_FILE			"sl.ini"
#define INI_APP				"_$SL"
#define INI_KEY_WINDOW			"window"
#define INI_KEY_DETAILSFONT		"detailsFont"
#define INI_KEY_DETAILSFORECOL		"DetailsForeCol"
#define INI_KEY_DETAILSBACKCOL		"DetailsBackCol"
#define INI_KEY_MODULE			"Module"
#define INI_DSKEY_INDEX			"_$SLIndex"
#define INI_DSKEY_DISABLE		"_$SLDisable"
#define INI_DSKEY_FONT			"_$SLFont"
#define INI_DSKEY_LISTBACKCOL		"_$SLListBackCol"
#define INI_DSKEY_ITEMFORECOL		"_$SLItemForeCol"
#define INI_DSKEY_ITEMBACKCOL		"_$SLItemBackCol"
#define INI_DSKEY_ITEMHLFORECOL		"_$SLItemHlForeCol"
#define INI_DSKEY_ITEMHLBACKCOL		"_$SLItemHlBackCol"


// Default colors and fonts
// ------------------------

#define DEF_DSLISTBACKCOL	SYSCLR_WINDOW
#define DEF_DSITEMFORECOL	SYSCLR_MENUTEXT
#define DEF_DSITEMBACKCOL	SYSCLR_MENU
#define DEF_DSITEMHLFORECOL	SYSCLR_MENUHILITE
#define DEF_DSITEMHLBACKCOL	SYSCLR_MENUHILITEBGND

#define DEF_DETAILSFORECOL	SYSCLR_WINDOWSTATICTEXT
#define DEF_DETAILSBACKCOL	SYSCLR_DIALOGBACKGROUND


// Window messages
// ---------------
// Message IDs up to (WM_USER+99) reserved for internal using by windows.

// Main window messages

// WM_SL_UPDATE_LIST - Message for main window.
// Update list, mp1 not 0 - update data only.
#define WM_SL_UPDATE_LIST	( WM_USER + 100 )
// WM_SL_UPDATE_DETAILS - Message for main and details windows.
// Update detail window.
#define WM_SL_UPDATE_DETAILS	( WM_USER + 101 )

// List window messages

// WM_SL_UPDATE_DATA - Message for list window and from list window to all
//                     items windows (broadcast).
// Update data of existing items.
#define WM_SL_UPDATE_DATA	( WM_USER + 200 )
// WM_SL_SETDATASRC - Message for list window.
// Set data source given by (PDATASOURCE)mp1.
#define WM_SL_SETDATASRC	( WM_USER + 201 )
// WM_SL_QUERY - Message for list window.
// mp1: SLQUERY_HINT - return hint window handle,
//      SLQUERY_CTXMENU - return context (popup) menu window handle.
#define WM_SL_QUERY		( WM_USER + 202 )
#define  SLQUERY_HINT		0
#define  SLQUERY_CTXMENU	1
// WM_SL_CONTEXTMENU - Message for list window.
//   (POINTS)mp1 - mouse pointer coordinates,
//   (LONG)mp2 - data source item for which will be built menu or -1.
#define WM_SL_CONTEXTMENU	( WM_USER + 204 )
// SB_SL_SLIDEROFFSET - New command for WM_HSCROLL.
#define SB_SL_SLIDEROFFSET	10555

// Details window messages

// WM_SL_DETAILSSIZE - From details window to parent window.
// Informs that the details window's height was changed.
#define WM_SL_DETAILSSIZE	( WM_USER + 204 )

// Properties dialog messages

#define WM_SL_UNDO		( WM_USER + 305 )
#define WM_SL_DEFAULT		( WM_USER + 306 )


// Resources
// ---------

#define ID_RESOURCE             1
#define ID_POPUP_MENU		2
#define ID_ACCELTABLE		3

// Menu items
#define IDM_DATASRC			5000
// IDM_DATASRC_FIRST_ID - menu item id for first data source
#define  IDM_DATASRC_FIRST_ID		5100
#define  IDM_DATASRC_LAST_ID		5199
#define  IDM_SETTINGS			5200
#define  IDM_EXIT			5300
#define IDM_VIEW			6000
#define  IDM_VIEW_SORT			6100
#define   IDM_SORT_FIRST_ID		6200
#define   IDM_SORT_LAST_ID		6299
#define   IDM_SORT_ASCN			6310
#define   IDM_SORT_DESCN		6320
#define  IDM_PROPERTIES			6400
// IDs for data source context menu items
#define IDM_DSCMD_FIRST_ID		20000
#define IDM_DSCMD_LAST_ID		29999

// Strings
#define IDS_TITLE			16
#define IDS_HOUR_SHORT			17
#define IDS_MIN_SHORT			18
#define IDS_SEC_SHORT			19
#define IDS_BITS			20
#define IDS_KBIT			21
#define IDS_MBIT			22
#define IDS_GBIT			23
#define IDS_BYTES			24
#define IDS_KB				25
#define IDS_MB				26
#define IDS_GB				27
#define IDS_TB				28
#define IDS_PAGE_NO			29

// Properties dialog

#define IDD_PROPERTIES			100
#define IDD_NOTEBOOK			101
#define IDD_PB_UNDO			102
#define IDD_PB_DEFAULT			103

// Settings dialog

#define IDD_SETTINGS			200
#define IDD_LB_MODULES			201
#define IDD_PB_DSUP			202
#define IDD_PB_DSDOWN			203
#define IDD_CB_DSENABLE			204
