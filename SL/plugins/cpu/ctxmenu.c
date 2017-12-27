#define INCL_ERRORS
#define INCL_WIN
#define INCL_DOSSPINLOCK
#include <os2.h>
#include <ds.h>
#include <sl.h>
#include "cpu.h"
#include <debug.h>

extern PCPU		pCPUList;	// CPU objects, cpu.c
extern HMODULE		hDSModule;	// Module handle, cpu.c

static ULONG		ulCtxMenuItem;

VOID showFeatures(HWND hwndOwner);	// features.c

// Helper, cpu.c
extern PHstrLoad			pstrLoad;

DSEXPORT VOID APIENTRY dsFillMenu(HWND hwndMenu, ULONG ulIndex)
{
  MENUITEM		stMINew = { 0 };
  CHAR			szBuf[64];

  // Item "Features"
  pstrLoad( hDSModule, IDS_FEATURES, sizeof(szBuf), &szBuf );
  stMINew.afStyle = MIS_TEXT;
  stMINew.iPosition = MIT_END;
  stMINew.id = IDM_DSCMD_FIRST_ID;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  if ( ulIndex == DS_SEL_NONE || ulIndex == 0 )
    return;

  // Separator
  stMINew.afStyle = MIS_SEPARATOR;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );

  // Item "Online"
  pstrLoad( hDSModule, IDS_ONLINE, sizeof(szBuf), &szBuf );
  stMINew.afStyle = MIS_TEXT;
  stMINew.id = IDM_DSCMD_FIRST_ID + 1;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
  WinSendMsg( hwndMenu, MM_SETITEMATTR, MPFROM2SHORT( stMINew.id, TRUE ),
              MPFROM2SHORT( MIA_CHECKED,
                            pCPUList[ulIndex].fOnline ? MIA_CHECKED : 0 ) );

  // Item "Offline"
  pstrLoad( hDSModule, IDS_OFFLINE, sizeof(szBuf), &szBuf );
  stMINew.id = IDM_DSCMD_FIRST_ID + 2;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
  WinSendMsg( hwndMenu, MM_SETITEMATTR, MPFROM2SHORT( stMINew.id, TRUE ),
              MPFROM2SHORT( MIA_CHECKED,
                            pCPUList[ulIndex].fOnline ? 0 : MIA_CHECKED ) );

  ulCtxMenuItem = ulIndex;
}

DSEXPORT ULONG APIENTRY dsCommand(HWND hwndOwner, USHORT usCommand)
{
  ULONG		ulStatus;

  if ( usCommand == IDM_DSCMD_FIRST_ID )
  {
    showFeatures( hwndOwner );
    return DS_UPD_NONE;
  }

  // User has selected a new status for the CPU.
  ulStatus = usCommand == (IDM_DSCMD_FIRST_ID + 1) ?
                             PROCESSOR_ONLINE : PROCESSOR_OFFLINE;

  if ( ( (ulStatus == PROCESSOR_ONLINE) == pCPUList[ulCtxMenuItem].fOnline ) ||
       ( DosSetProcessorStatus( ulCtxMenuItem + 1, ulStatus ) != NO_ERROR ) )
    return DS_UPD_NONE;

  pCPUList[ulCtxMenuItem].fOnline = usCommand == (IDM_DSCMD_FIRST_ID + 1);

  return DS_UPD_DATA;
}
