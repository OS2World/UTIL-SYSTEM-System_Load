#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include "sl.h"
#include "srclist.h"

BOOL itemsInit();
VOID itemsDone();
VOID itemsSetDataSrc(HWND hwnd, PDATASOURCE pDataSrc);
PDATASOURCE itemsGetDataSrc();
VOID itemsSetVisible(BOOL fFlag);
VOID itemsSelect(HWND hwndListClient, BOOL fAbsolute, LONG lItem);
VOID itemsGetSpace(PSIZEL psizeSpace);
VOID itemsPaintDetails(HPS hps, SIZEL sizeWnd);
VOID itemsFillContextMenu(HWND hwndMenu, ULONG ulItem);
VOID itemsCommand(HWND hwndOwner, USHORT usCommand);
// itemsSortBy(ulNewSort), ulNewSort - sort type or DSSORT_QUERY
ULONG itemsSortBy(ULONG ulNewSort);
VOID itemsEnter(HWND hwnd);
BOOL itemsHelp(BOOL fShow);
