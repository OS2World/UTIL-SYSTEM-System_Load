#ifndef SRCLIST_H
#define SRCLIST_H

#include <os2.h>
#include "ds.h"

// srclstSetColor(, ulColorType, )
#define SLCT_LISTBACK		0
#define SLCT_ITEMFORE		1
#define SLCT_ITEMBACK		2
#define SLCT_ITEMHLFORE		3
#define SLCT_ITEMHLBACK		4

typedef struct _DATASOURCE {
  HMODULE	hModule;		// DLL (module) handle.
  CHAR		szModule[13];
  ULONG		ulMenuItemId;
  ULONG		ulIndex;		// Order index.
  PDSINFO	pDSInfo;		// Pointer to module information.
  BOOL		fDisable;
  BOOL		fInitialized;		// fnInit() result.
  ULONG		ulLastUpdate;
  BOOL		fOverdueUpdate;
  BOOL		fForceUpdate;
  PSZ		pszFont;
  LONG		lListBackCol;
  LONG		lItemForeCol;
  LONG		lItemBackCol;
  LONG		lItemHlForeCol;
  LONG		lItemHlBackCol;
  SIZEL		sizeEm;

  // Functions of the data source
  PDSINFO APIENTRY (*fnInstall)(HMODULE hMod, PSLINFO pSLInfo);
  VOID APIENTRY (*fnUninstall)(VOID);				// optional
  BOOL APIENTRY (*fnInit)(VOID);
  VOID APIENTRY (*fnDone)(VOID);
  ULONG APIENTRY (*fnGetUpdateDelay)(VOID);			// optional
  ULONG APIENTRY (*fnGetCount)(VOID);
  VOID APIENTRY (*fnSetWndStart)(HPS hps, PSIZEL pSize);	// optional
  VOID APIENTRY (*fnSetWnd)(ULONG ulIndex, HWND hwnd, HPS hps);	// optional
  ULONG APIENTRY (*fnUpdate)(ULONG ulTime);
  BOOL APIENTRY (*fnSetSel)(ULONG ulIndex);			// optional
  ULONG APIENTRY (*fnGetSel)(VOID);				// optional
  BOOL APIENTRY (*fnIsSelected)(ULONG ulIndex);			// optional
  VOID APIENTRY (*fnPaintItem)(ULONG ulIndex, HPS hps,
                               PSIZEL psizeWnd);
  VOID APIENTRY (*fnPaintDetails)(HPS hps, PSIZEL psizeWnd);	// optional
  VOID APIENTRY (*fnGetHintText)(ULONG ulIndex, PCHAR pcBuf,
                        ULONG cbBuf);				// optional
  ULONG APIENTRY (*fnSortBy)(ULONG ulNewSort);			// optional
  ULONG APIENTRY (*fnLoadDlg)(HWND hwndOwner, PHWND pahWnd);	// optional
  VOID APIENTRY (*fnFillMenu)(HWND hwndMenu, ULONG ulIndex);	// optional
  ULONG APIENTRY (*fnCommand)(HWND hwndOwner,
                              USHORT usCommand);		// optional
  ULONG APIENTRY (*fnEnter)(HWND hwndOwner);			// optional

} DATASOURCE, *PDATASOURCE;

VOID srclstInit(HAB hab);
VOID srclstDone();
PDATASOURCE srclstGetByMenuItemId(ULONG ulMenuItemId);
ULONG srclstGetCount();
PDATASOURCE srclstGetByIndex(ULONG ulIndex);
BOOL srclstMove(ULONG ulIndex, BOOL fForward);
VOID srclstStoreOrder();
VOID srclstEnable(ULONG ulIndex, BOOL fEnable);
BOOL srclstIsEnabled(ULONG ulIndex);
VOID srclstSetFont(PDATASOURCE pDataSrc, PSZ pszFont);
VOID srclstSetColor(PDATASOURCE pDataSrc, ULONG ulColorType, LONG lColor);

#endif // SRCLIST_H
