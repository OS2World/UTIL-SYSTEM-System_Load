#include "linkseq.h"

typedef struct _STSTR {
  PSZ		pszStr;
  ULONG		cbStr;
} STSTR, *PSTSTR;

// STOBJGROUP, STOBJITEM
//
// Contains a data from visible filter node necessary for graphical
// representation. Pointers to STOBJITEM records will be placed to SCNODE.pUser.

typedef struct _STOBJ {
  SEQOBJ	lsObj;
  BOOL		ulType;
  ULONG		ulMakeStamp;

  STSTR		stName;
  GRVAL		stGrValSent;
  GRVAL		stGrValRecv;
  ULLONG	ullSent;
  ULLONG	ullRecv;
} STOBJ, *PSTOBJ;

typedef struct _STOBJGROUP {
  STOBJ		stObj;

  ULLONG	ullNewSumSent;
  ULLONG	ullNewSumRecv;
  ULONG		ulMaxSendSpeed;
  ULONG		ulMaxRecvSpeed;

  BOOL		fExpanded;
  GRAPH		stGraph;
  LINKSEQ	lsItems;
} STOBJGROUP, *PSTOBJGROUP;

typedef struct _STOBJITEM {
  STOBJ		stObj;

  STSTR		stComment;
  PSTOBJGROUP	pStObjGroup;
} STOBJITEM, *PSTOBJITEM;

#define _stObj_pszName	stObj.stName.pszStr
#define _stObj_cbName	stObj.stName.cbStr


#define WM_NODETRACE		WM_USER + 1

// Methods for calculating the rates for groups.

#define GROUPRATE_MAX		0
#define GROUPRATE_SUM		1
#define GROUPRATE_AVERAGE	2

// Default values.

#define DEF_INTERVAL		1000
#define DEF_TIMEWIN		180
#define DEF_GRBGCOLOR		0x00FFFFFF
#define DEF_GRBORDERCOLOR	0
#define DEF_RXCOLOR		0x00E00520
#define DEF_TXCOLOR		0x00008000
#define DEF_GROUPRATES		GROUPRATE_MAX

// Filter dialogs.

#define ID_LAYER_MAIN		1000
#define ID_LAYER_NODEEDITOR	1001
#define ID_LAYER_BOTTOM		1002
#define ID_LAYER_RIGHT		1003
#define ID_EF_ADDR		1100

#define ID_DLG_FILTER		2000
// Container.
#define ID_CONT_NODES		2100
// Node editor layer.
#define ID_GB_NODE		2101
#define ID_ST_NAME		2104
#define ID_EF_NAME		2105
#define ID_CB_STOP		2106
#define ID_CB_IPHDR		2107
#define ID_ST_PROTO		2108
#define ID_CB_PROTO		2109
#define ID_ST_SRCADDR		2110
#define ID_LB_SRCADDR		2111
#define ID_ST_SRCPORT		2112
#define ID_EF_SRCPORT		2113
#define ID_ST_DSTADDR		2114
#define ID_LB_DSTADDR		2115
#define ID_ST_DSTPORT		2116
#define ID_EF_DSTPORT		2117
#define ID_ST_SENT		2118
#define ID_ST_SENTVAL		2119
#define ID_ST_RECV		2120
#define ID_ST_RECVVAL		2121
#define ID_PB_RESET		2122
#define ID_PB_APPLY		2123
#define ID_ST_COMMENTS		2124
#define ID_MLE_COMMENTS		2125
// Bottom layer (buttons).
#define ID_PB_NEW		2200
#define ID_PB_MOVE_UP		2201
#define ID_PB_MOVE_DOWN		2202
#define ID_PB_NESTING		2203
#define ID_PB_DELETE		2204
#define ID_PB_HELP		2205

#define ID_MENU_NEW		1100
#define ID_MI_NEW_TIF		1101
#define ID_MI_NEW_BEFORE	1102
#define ID_MI_NEW_AFTER		1103
#define ID_MI_NEW_AT		1104

#define ID_MENU_NESTING		1110
#define ID_MI_NESTING_TOPREV	1111
#define ID_MI_NESTING_TONEXT	1112
#define ID_MI_NESTING_UP	1113

#define ID_MENU_NODE		1120
#define ID_MI_NODE_TRACE	1121
#define ID_MI_NODE_RENAME	1122
#define ID_MI_NODE_DELETE	1123
#define ID_MI_NODE_RESET	1124

#define ID_DLG_NEW_TIF		3000
#define ID_CB_INTERFACE		3001
#define ID_PB_CREATE		3002
#define ID_PB_CANCEL		3003

// Strings

#define IDS_DS_TITLE		1
#define IDS_MI_COMPACT		2
#define IDS_MI_DETAILED		3
#define IDS_MI_EXPAND_ALL	4
#define IDS_MI_COLLAPSE_ALL	5
#define IDS_MI_FILTER		6
#define IDS_MI_TRACE		7
#define IDS_NODE_NAME		8
#define IDS_SENT_BYTES		9
#define IDS_SEND_RATIO		10
#define IDS_RECV_BYTES		11
#define IDS_RECV_RATIO		12
#define IDS_INDEX		13
#define IDS_TXRX		14
#define IDS_PROTOCOL		15
#define IDS_SOURCE		16
#define IDS_DESTINATION		17
#define IDS_PKT_SIZE		18
#define IDS_TRACING		19
#define IDS_PAUSE		20


// Messages

#define IDM_TIF_DELETE_CONF	1
#define IDM_NODE_DELETE_CONF	2
#define IDM_INVALID_NAME	3
#define IDM_INVALID_SRCADDR	4
#define IDM_INVALID_DSTADDR	5
#define IDM_INVALID_SRCPORT	6
#define IDM_INVALID_DSTPORT	7
#define IDM_APPLY_CHANGES_CONF	8
#define IDM_RESET_CONF		9
#define IDM_RESET_BRANCH_CONF	10
#define IDM_RESET_TIF_CONF	11

// Properties dialog

#define ID_DLG_DSPROPERTIES	4000
#define ID_GB_GROUPRATES	4120
#define ID_RB_GRRATES_MAX	4121
#define ID_RB_GRRATES_SUM	4122
#define ID_RB_GRRATES_AVERAGE	4123
#define ID_GB_COLORS		4001
#define ID_ST_BGCOL		4002
#define ID_ST_BORDERCOL		4003
#define ID_ST_RXCOL		4004
#define ID_ST_TXCOL		4005
#define ID_GB_GRAPHTIME		4006
#define ID_SLID_TIME		4007
#define ID_ST_GRAPHTIME		4008
#define ID_ST_INTERVAL		4009
#define ID_SB_INTERVAL		4010
#define ID_ST_SECONDS		4011

// Trace dialog

#define ID_DLG_TRACE		5000
#define ID_CONT_RECORDS		5001
#define ID_PB_PAUSE		5002
#define ID_PB_CLOSE		5003
