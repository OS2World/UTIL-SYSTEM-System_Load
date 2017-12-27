// Data sources functions headers.
//
// Headers of these functions are used in file srclist.c for a list of
// data sources.

#ifndef DS_H
#define DS_H

#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include "graph.h"
#include "utils.h"	// type ULLONG

#define DSEXPORT	__declspec(dllexport)

#ifndef OPEN_DEFAULT
#define OPEN_DEFAULT		0
#endif

// Helper routines.

// INI-file

typedef BOOL (*PHiniWriteLong)(HMODULE hMod, PSZ pszKey, LONG lData);
typedef BOOL (*PHiniWriteULong)(HMODULE hMod, PSZ pszKey, ULONG ulData);
typedef BOOL (*PHiniWriteStr)(HMODULE hMod, PSZ pszKey, PSZ pszData);
typedef BOOL (*PHiniWriteData)(HMODULE hMod, PSZ pszKey, PVOID pBuf, ULONG cbBuf);
typedef LONG (*PHiniReadLong)(HMODULE hMod, PSZ pszKey, LONG lDefault);
typedef ULONG (*PHiniReadULong)(HMODULE hMod, PSZ pszKey, ULONG ulDefault);
typedef ULONG (*PHiniReadStr)(HMODULE hMod, PSZ pszKey, PCHAR pcBuf, ULONG cbBuf,
                            PSZ pszDefault);
typedef BOOL (*PHiniReadData)(HMODULE hMod, PSZ pszKey, PVOID pBuf, PULONG pcbBuf);
typedef BOOL (*PHiniGetSize)(HMODULE hMod, PSZ pszKey, PULONG pcbData);


// Graph

typedef BOOL (*PHgrInit)(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow,
                       ULONG ulMin, ULONG ulMax);
typedef VOID (*PHgrDone)(PGRAPH pGraph);
typedef BOOL (*PHgrSetTimeScale)(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow);
typedef VOID (*PHgrNewTimestamp)(PGRAPH pGraph, ULONG ulTimestamp);
typedef BOOL (*PHgrGetTimestamp)(PGRAPH pGraph, PULONG pulTimestamp);
typedef VOID (*PHgrInitVal)(PGRAPH pGraph, PGRVAL pGrVal);
typedef VOID (*PHgrDoneVal)(PGRVAL pGrVal);
typedef VOID (*PHgrResetVal)(PGRAPH pGraph, PGRVAL pGrVal);
typedef VOID (*PHgrSetValue)(PGRAPH pGraph, PGRVAL pGrVal, ULONG ulValue);
typedef BOOL (*PHgrGetValue)(PGRAPH pGraph, PGRVAL pGrVal, PULONG pulValue);
typedef VOID (*PHgrDraw)(PGRAPH pGraph, HPS hps, PRECTL prclGraph, ULONG cGrVal,
                       PGRVAL *ppGrVal, PGRPARAM pParam);

// Utils

typedef BOOL (*PHutilGetTextSize)(HPS hps, ULONG cbText, PSZ pszText, PSIZEL pSize);
typedef VOID (*PHutil3DFrame)(HPS hps, PRECTL pRect, LONG lLTColor, LONG lRBColor);
typedef VOID (*PHutilBox)(HPS hps, PRECTL pRect, LONG lColor);
typedef BOOL (*PHutilWriteResStr)(HPS hps, HMODULE hMod, ULONG ulId,
                                  ULONG cVal, PSZ *ppszVal);
typedef BOOL (*PHutilWriteResStrAt)(HPS hps, PPOINTL pptPos, HMODULE hMod,
                                    ULONG ulId, ULONG cVal, PSZ *ppszVal);
typedef VOID (*PHutilCharStringRect)(HPS hps, PRECTL pRect, ULONG cbBuf,
                                     PCHAR pcBuf, ULONG ulFlags);
typedef BOOL (*PHutilSetFontFromPS)(HPS hpsDst, HPS hpsSrc, LONG llcid);
typedef LONG (*PHutilMixRGB)(LONG lColor1, LONG lColor2, ULONG ulMix);
typedef PSIZEL (*PHutilGetEmSize)(HMODULE hMod);
typedef LONG (*PHutilGetColor)(HMODULE hMod, ULONG ulColor);
typedef ULONG (*PHutilLoadInsertStr)(HMODULE hMod, BOOL fStrMsg, ULONG ulId,
                                   ULONG cVal, PSZ *ppszVal, ULONG cbBuf, PCHAR pcBuf);
typedef ULONG (*PHutilQueryProgPath)(ULONG cbBuf, PCHAR pcBuf);

// Lock/unlock updates

typedef VOID (*PHupdateLock)(VOID);
typedef VOID (*PHupdateUnlock)(VOID);
typedef VOID (*PHupdateForce)(HMODULE hMod);

// Strings

typedef LONG (*PHstrFromSec)(ULONG ulVal, ULONG cbBuf, PCHAR pcBuf);
typedef LONG (*PHstrFromBits)(ULONG ulVal, ULONG cbBuf, PCHAR pcBuf, BOOL fSlashSec);
typedef LONG (*PHstrFromBytes)(ULLONG ullVal, ULONG cbBuf, PCHAR pcBuf, BOOL fSlashSec);
typedef ULONG (*PHstrLoad)(HMODULE hModule, ULONG ulStrId, ULONG cbBuf, PCHAR pcBuf);
typedef ULONG (*PHstrLoad2)(HMODULE hModule, ULONG ulStrId, PULONG pcbBuf,
                            PCHAR *ppcBuf);
typedef ULONG (*PHstrFromULL)(PCHAR pcBuf, ULONG cbBuf, ULLONG ullVal);

// Controls

typedef BOOL (*PHctrlStaticToColorRect)(HWND hwndStatic, LONG lColor);
typedef VOID (*PHctrlDlgCenterOwner)(HWND hwnd);
typedef ULONG (*PHctrlQueryText)(HWND hwnd, ULONG cbBuf, PCHAR pcBuf);
typedef BOOL (*PHctrlSubclassNotebook)(HWND hwndDlg);

// Help

typedef VOID (*PHhelpShow)(HMODULE hMod, ULONG ulIndex);


// DS_* : ds*Update(), ds*Command() result codes
// DS_UPD_NONE - no data/items changed
#define DS_UPD_NONE		0
// DS_UPD_DATA - data of existing items changed
#define DS_UPD_DATA		1
// DS_UPD_LIST - data changed and/or added or deleted items, items reordered
#define DS_UPD_LIST		2

// DS_SEL_NONE - dsIRQGetSel() resilt code when no selected items
#define DS_SEL_NONE		((ULONG)(-1))

// Constants of sorting.

// DSSORT_QUERY - dsIRQSortBy() argument to query current sort type
#define DSSORT_QUERY		((ULONG)(-1))
// DSSORT_VALUE_MASK - dsIRQSortBy() result/argument low word is a sort type
//                     index: from 0 to (DSINFO.cSortBy - 1)
#define DSSORT_VALUE_MASK	0x0000FFFF
// DSSORT_FL_DESCENDING - dsIRQSortBy() result/argument sort direction flag
#define DSSORT_FL_DESCENDING	0x00010000

// DSINFO.ulFlags

// DS_FL_ITEM_BG_LIST - Fill not selected item in list's background color.
#define DS_FL_ITEM_BG_LIST	0x00000001
// DS_FL_SELITEM_BG_LIST - Fill selected item in list's background color.
#define DS_FL_SELITEM_BG_LIST	0x00000002
// DS_FL_NO_BG_UPDATES - Do not update data source while data unvisible.
#define DS_FL_NO_BG_UPDATES	0x00000004
// DS_FL_RES_MENU_TITLE - Use resource string with ID
// DSINFO._title.ulResId as module's title. Without this flag string
// DSINFO._title.pszStr used.
#define DS_FL_RES_MENU_TITLE	0x00000008
// DS_FL_RES_SORT_BY - Use array's items apszSortBy as resource strings IDs.
#define DS_FL_RES_SORT_BY	0x00000010

// Constants for helper utilGetColor(,ulColor)
// ( see srclist.c / dshGetColor() )

#define DS_COLOR_LISTBG		0
#define DS_COLOR_ITEMFORE	1
#define DS_COLOR_ITEMBACK	2
#define DS_COLOR_ITEMHIFORE	3
#define DS_COLOR_ITEMHIBACK	4


// System Load information for data sources.

typedef struct _SLINFO {
  ULONG		ulVerMajor;
  ULONG		ulVerMinor;
  ULONG		ulVerRevision;
  HAB		hab;

  PFN (*slQueryHelper)(PSZ pszFunc);
} SLINFO, *PSLINFO;

// Data source information for System Load.

typedef struct _DSINFO {
  ULONG		cbDSInfo;
  union {
    PSZ		pszStr;			// Flag DS_FL_RES_MENU_TITLE is NOT set.
    ULONG	ulResId;		// Flag DS_FL_RES_MENU_TITLE is set.
  } _title;
  ULONG		cSortBy;
  union {
    PSZ		*apszStr;		// Flag DS_FL_RES_SORT_BY is NOT set.
    PULONG	aulResId;		// Flag DS_FL_RES_SORT_BY is set.
  } _sortBy;
  ULONG		ulFlags;		// DS_FL_*
  ULONG		ulHorSpace;		// Horisontal items space scale, %
  ULONG		ulVertSpace;		// Vertical items space scale, %
  ULONG		ulHelpId;		// Help panel index in <module>.hlp
} DSINFO, *PDSINFO;

#endif	// DS_H
