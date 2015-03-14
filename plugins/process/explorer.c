#define INCL_WINWORKPLACE
#define INCL_DOSMISC
#define INCL_ERRORS
#define INCL_WINDIALOGS
#define INCL_WINSYS
#define INCL_WIN
#define INCL_DOSPROCESS		// PRTYC_*
#define INCL_DOSNLS        /* National Language Support values */
#include <os2.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <ds.h>
#include <sl.h>
#include "process.h"
#include "bldlevel.h"
#include "sysstate.h"
#include "debug.h"

extern HMODULE		hDSModule;	// Module handle, process.c.
extern HAB		hab;		// process.c.

// Helpers, process.c.
extern PHiniWriteULong			piniWriteULong;
extern PHiniWriteLong			piniWriteLong;
extern PHiniReadULong			piniReadULong;
extern PHiniReadLong			piniReadLong;
extern PHstrFromBytes			pstrFromBytes;
extern PHstrLoad			pstrLoad;
extern PHctrlDlgCenterOwner		pctrlDlgCenterOwner;
extern PHctrlSubclassNotebook		pctrlSubclassNotebook;

static PVOID		pPtrRec;
static PVOID		pPRec;
static PVOID		pProcLRec;
static SSPROCESS	stProcess;
static SSMODULE		stProcModule;

BOOL querySysState(PULONG pcbPtrRec, PVOID *ppPtrRec); // process.c


static VOID _cbCopyFName(PSZ pszPathName, BOOL fFullName)
{
  PSZ		pszDest;

  if ( !fFullName )
  {
    pszPathName = strrchr( pszPathName, '\\' );
    if ( pszPathName == NULL )
      return;

    pszPathName++;
  }

  DosAllocSharedMem( (PVOID)&pszDest, NULL, strlen( pszPathName ) + 1,
                     PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE );
  strcpy( pszDest, pszPathName );

  if ( !WinOpenClipbrd( hab ) )
  {
    DosFreeMem( (PVOID)&pszDest );
    return;
  }

  WinEmptyClipbrd( hab );
  WinSetClipbrdData( hab, (ULONG)pszDest, CF_TEXT, CFI_POINTER );
  WinCloseClipbrd ( hab );
}


//	Page: Files
//	===========

#define IS_DEVICE(name) ( memcmp( (name), "\\DEV\\", 5 ) == 0 )

typedef struct _FILERECORD {
  RECORDCORE		stRecCore;
  ULONG			ulHandle;
  PSZ			pszName;
  CDATE			stDate;
  CTIME			stTime;
  PSZ			pszAttr;
  PSZ			pszSize;
  PSZ			pszPathName;
  CHAR			acAttr[8];
  CHAR			acSize[16];
  ULLONG		ullSize;
} FILERECORD, *PFILERECORD;

typedef struct _PGFILESORT {
  BOOL			fDesc;
  ULONG			ulCmd;
} PGFILESORT, *PPGFILESORT;

static ULONG		_pgFile_aFields[] = { IDM_EXP_SORT_HANDLES,
                                              IDM_EXP_SORT_NAME,
                                              IDM_EXP_SORT_FULLNAME,
                                              IDM_EXP_SORT_SIZE };

static SHORT EXPENTRY _compFileRecords(PRECORDCORE p1, PRECORDCORE p2,
                                       PVOID pStorage)
{
  PPGFILESORT		pSort = (PPGFILESORT)pStorage;
  SHORT			sRes = 0;
  PSZ			pszName1, pszName2;

  switch( pSort->ulCmd )
  {
    case IDM_EXP_SORT_HANDLES:
      if ( ((PFILERECORD)p1)->ulHandle > ((PFILERECORD)p2)->ulHandle )
        sRes = 1;
      else if ( ((PFILERECORD)p1)->ulHandle < ((PFILERECORD)p2)->ulHandle )
        sRes = -1;
      break;

    case IDM_EXP_SORT_SIZE:
      if ( ((PFILERECORD)p1)->ullSize > ((PFILERECORD)p2)->ullSize )
      {
        sRes = 1;
        break;
      }
      else if ( ((PFILERECORD)p1)->ullSize < ((PFILERECORD)p2)->ullSize )
      {
        sRes = -1;
        break;
      }
      // Equal sizes - sort by pathname.

    default:
      if ( pSort->ulCmd == IDM_EXP_SORT_NAME )
      {
        pszName1 = ((PFILERECORD)p1)->pszName;
        pszName2 = ((PFILERECORD)p2)->pszName;
      }
      else
      {
        pszName1 = ((PFILERECORD)p1)->pszPathName;
        pszName2 = ((PFILERECORD)p2)->pszPathName;
      }

      switch( WinCompareStrings( hab, 0, 0, pszName1, pszName2, 0 ) )
      {
        case WCS_GT:
          sRes = 1;
          break;

        case WCS_LT:
          sRes = -1;
          break;
      }
      break;
  }

  return pSort->fDesc ? -sRes : sRes;
}

static VOID _pgFilesOpenPath(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  PFILERECORD		pRecord;
  HOBJECT		hObject;
  CHAR			szPath[CCHMAXPATH];
  PCHAR			pcSlash;
  ULONG			cPath;

  pRecord = (PFILERECORD)WinSendMsg( hwndCnt, CM_QUERYRECORDEMPHASIS,
                            MPFROMLONG(CMA_FIRST), MPFROMSHORT(CRA_SELECTED) );

  if ( ( pRecord == NULL ) && ( (LONG)pRecord == -1 ) )
    return;

  pcSlash = strrchr( pRecord->pszPathName, '\\' );
  if ( pcSlash == NULL )
    return;

  cPath = pcSlash - pRecord->pszPathName;
  memcpy( szPath, pRecord->pszPathName, cPath );
  szPath[cPath] = '\0';

  hObject = WinQueryObject( &szPath );

  if ( ( hObject != NULLHANDLE ) &&
       WinOpenObject( hObject, OPEN_DEFAULT, TRUE ) )
    WinOpenObject( hObject, OPEN_DEFAULT, TRUE ); // Move to the fore.
}

static VOID _pgFilesCopyName(HWND hwnd, BOOL fFullName)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  PFILERECORD		pRecord;

  pRecord = (PFILERECORD)WinSendMsg( hwndCnt, CM_QUERYRECORDEMPHASIS,
                            MPFROMLONG(CMA_FIRST), MPFROMSHORT(CRA_SELECTED) );

  if ( ( pRecord != NULL ) && ( (LONG)pRecord != -1 ) )
    _cbCopyFName( pRecord->pszPathName, fFullName );
}

static BOOL _pgFilesGetSort(HWND hwndCtxMenu, PPGFILESORT pSort)
{
  ULONG			ulIdx;

  pSort->fDesc = 
     (SHORT)WinSendMsg( hwndCtxMenu, MM_QUERYITEMATTR,
                        MPFROM2SHORT( IDM_EXP_SORT_DESCN, TRUE ),
                        MPFROMSHORT( MIA_CHECKED ) ) != 0;

  for( ulIdx = 0; ulIdx < ARRAYSIZE(_pgFile_aFields); ulIdx++ )
  {
    if ( (SHORT)WinSendMsg( hwndCtxMenu, MM_QUERYITEMATTR,
                        MPFROM2SHORT( _pgFile_aFields[ulIdx], TRUE ),
                        MPFROMSHORT( MIA_CHECKED ) ) != 0 )
    {
      pSort->ulCmd = _pgFile_aFields[ulIdx];
      return TRUE;
    }
  }

  return FALSE;
}

static VOID _pgFilesSetSort(HWND hwndCnt, PPGFILESORT pSort)
{
  HWND			hwndCtxMenu = WinQueryWindowULong( hwndCnt, QWL_USER );
  ULONG			ulIdx;

  WinCheckMenuItem( hwndCtxMenu, IDM_EXP_SORT_ASCN, !pSort->fDesc );
  WinCheckMenuItem( hwndCtxMenu, IDM_EXP_SORT_DESCN, pSort->fDesc );

  for( ulIdx = 0; ulIdx < ARRAYSIZE(_pgFile_aFields); ulIdx++ )
    WinCheckMenuItem( hwndCtxMenu, _pgFile_aFields[ulIdx],
                      pSort->ulCmd == _pgFile_aFields[ulIdx] );

  WinSendMsg( hwndCnt, CM_SORTRECORD, MPFROMP( _compFileRecords ),
              MPFROMP( pSort ) );
}

static VOID _pgFilesWMInitDlg(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  PFIELDINFO		pFieldInfo;
  CNRINFO		stCnrInf = { 0 };
  PFIELDINFO		pFldInf;
  FIELDINFOINSERT	stFldInfIns;
  PFILERECORD		pRecords;
  PFILERECORD		pRecord;
  RECORDINSERT		stRecIns;
  PSSPFILE		pstFiles;
  PSSPFILE		pstFile;
  ULONG			cbFiles;
  ULONG			cFiles;
  BOOL			fDevice;
  FILESTATUS3L		stFStat;
  ULONG			ulRC;
  CHAR			szBuf[128];
  HWND			hwndCtxMenu;
  PGFILESORT		stSort;

  // Load context menu.
  hwndCtxMenu = WinLoadMenu( HWND_OBJECT, hDSModule, ID_EXPFILES_POPUP_MENU );
  WinSetWindowULong( hwndCnt, QWL_USER, hwndCtxMenu );

  // Insert fields.

  pFldInf = (PFIELDINFO)WinSendMsg( hwndCnt, CM_ALLOCDETAILFIELDINFO,
                                    MPFROMLONG( 7 ), NULL );
  if ( pFldInf == NULL )
  {
    debug( "CM_ALLOCDETAILFIELDINFO returns 0" );
    return;
  }
  pFieldInfo = pFldInf;

  pstrLoad( hDSModule, IDS_HANDLE, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_ULONG | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( FILERECORD, ulHandle );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_FILE, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_LEFT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( FILERECORD, pszName );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_SIZE, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( FILERECORD, pszSize );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_DATE, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_DATE | CFA_HORZSEPARATOR | CFA_RIGHT;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( FILERECORD, stDate );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_TIME, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_TIME | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( FILERECORD, stTime );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  stCnrInf.pFieldInfoLast = pFieldInfo;

  pstrLoad( hDSModule, IDS_ATTRIBUTES, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_CENTER | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( FILERECORD, pszAttr );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_FULLFNAME, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_LEFT;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( FILERECORD, pszPathName );

  stFldInfIns.cb = sizeof(FIELDINFOINSERT);
  stFldInfIns.pFieldInfoOrder = (PFIELDINFO)CMA_FIRST;
  stFldInfIns.cFieldInfoInsert = 7;
//  stFldInfIns.fInvalidateFieldInfo = TRUE;
  WinSendMsg( hwndCnt, CM_INSERTDETAILFIELDINFO, MPFROMP( pFldInf ),
              MPFROMP( &stFldInfIns ) );

  stCnrInf.cb = sizeof(CNRINFO);
  stCnrInf.flWindowAttr = CV_DETAIL | CA_DETAILSVIEWTITLES;
  stCnrInf.xVertSplitbar = piniReadLong( hDSModule, "expfilesSplitbar", 200 );
  WinSendMsg( hwndCnt, CM_SETCNRINFO, &stCnrInf,
              MPFROMLONG( CMA_PFIELDINFOLAST | CMA_XVERTSPLITBAR |
                          CMA_FLWINDOWATTR ) );

  // Insert records

  // Query list of files for process.
  cbFiles = ssGetPFiles( pPtrRec, pPRec, NULL, 0 );
  pstFiles = debugMAlloc( cbFiles );
  if ( pstFiles == NULL )
  {
    debug( "Not enough memory" );
    return;
  }
  cbFiles = ssGetPFiles( pPtrRec, pPRec, pstFiles, cbFiles );
  cFiles = cbFiles / sizeof(SSPFILE);
  stRecIns.cRecordsInsert = cFiles;

  // Allocate records.
  pRecords = (PFILERECORD)WinSendMsg( hwndCnt, CM_ALLOCRECORD,
                         MPFROMLONG( sizeof(FILERECORD) - sizeof(RECORDCORE) ),
                         MPFROMLONG( cFiles ) );
  if ( pRecords == NULL )
  {
    debug( "CM_ALLOCRECORD returns 0" );
    debugFree( pstFiles );
    return;
  }

  // Fill records.
  pRecord = pRecords;
  for( pstFile = pstFiles; cFiles > 0; cFiles--, pstFile++ )
  {
    fDevice = IS_DEVICE( pstFile->pszName );

    pRecord->stRecCore.cb = sizeof(RECORDCORE);
    pRecord->ulHandle = pstFile->ulHandle;
    pRecord->pszAttr = &pRecord->acAttr;
    pRecord->pszSize = &pRecord->acSize;

    pRecord->pszPathName = pstFile->pszName;

    if ( fDevice )
    {
      pRecord->pszName = pRecord->pszPathName;
    }
    else
    {
      pRecord->pszName = strrchr( pRecord->pszPathName, '\\' );
      if ( pRecord->pszName == NULL )
        pRecord->pszName = pRecord->pszPathName;
      else
        pRecord->pszName++;

      ulRC = DosQueryPathInfo( pstFile->pszName, FIL_STANDARDL,
                               &stFStat, sizeof(FILESTATUS3L) );
      if ( ulRC == NO_ERROR )
      {
        pRecord->ullSize = *((PULLONG)&stFStat.cbFile);

        pstrFromBytes( *((PULLONG)&stFStat.cbFile), sizeof(pRecord->acSize),
                       &pRecord->acSize, FALSE );

        pRecord->stDate.day   = *((PUSHORT)&stFStat.fdateCreation) & 0x1F;
        pRecord->stDate.month = (*((PUSHORT)&stFStat.fdateCreation) >> 5) & 0x0F;
        pRecord->stDate.year  = ((*((PUSHORT)&stFStat.fdateCreation) >> 9) & 0x7F) + 1980;

        pRecord->stTime.seconds = (*((PUSHORT)&stFStat.ftimeCreation) & 0x1F) * 2;
        pRecord->stTime.minutes = (*((PUSHORT)&stFStat.ftimeCreation) >> 5) & 0x3F;
        pRecord->stTime.hours   = ((*((PUSHORT)&stFStat.ftimeCreation) >> 11) & 0x1F);
      }

      pRecord->acAttr[0] = (pstFile->ulAttr & 0x20) == 0 ? '-' : 'A';
      pRecord->acAttr[1] = (pstFile->ulAttr & 0x10) == 0 ? '-' : 'D';
      pRecord->acAttr[2] = (pstFile->ulAttr & 0x08) == 0 ? '-' : 'L';
      pRecord->acAttr[3] = (pstFile->ulAttr & 0x04) == 0 ? '-' : 'S';
      pRecord->acAttr[4] = (pstFile->ulAttr & 0x02) == 0 ? '-' : 'H';
      pRecord->acAttr[5] = (pstFile->ulAttr & 0x01) == 0 ? '-' : 'R';
      pRecord->acAttr[6] = '\0';
    }

    pRecord = (PFILERECORD)pRecord->stRecCore.preccNextRecord;
  }

  debugFree( pstFiles );

  // Insert records to the container.
  stRecIns.cb = sizeof(RECORDINSERT);
  stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
  stRecIns.pRecordParent = NULL;
  stRecIns.zOrder = (USHORT)CMA_TOP;
//  stRecIns.fInvalidateRecord = TRUE;
  WinSendMsg( hwndCnt, CM_INSERTRECORD, (PRECORDCORE)pRecords, &stRecIns );

  // Sort records.

  stSort.ulCmd = piniReadULong( hDSModule, "expfilesSortCmd", IDM_EXP_SORT_FULLNAME );
  stSort.fDesc = (BOOL)piniReadULong( hDSModule, "expfilesSortDesc", FALSE );
  _pgFilesSetSort( hwndCnt, &stSort );
}

static VOID _pgFilesWMDestroy(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  HWND			hwndCtxMenu = WinQueryWindowULong( hwndCnt, QWL_USER );
  PFIELDINFO		pFieldInfo;
  CNRINFO		stCnrInf;
  PGFILESORT		stSort;

  // Store sorting: field and direction.
  if ( _pgFilesGetSort( hwndCtxMenu, &stSort ) )
  {
    piniWriteULong( hDSModule, "expfilesSortCmd", stSort.ulCmd );
    piniWriteULong( hDSModule, "expfilesSortDesc", (ULONG)stSort.fDesc );
  }

  // Destroy context menu.
  WinDestroyWindow( hwndCtxMenu );

  // Store split bar position to the ini-file.
  if ( (ULONG)WinSendMsg( hwndCnt, CM_QUERYCNRINFO, MPFROMP( &stCnrInf ),
                          MPFROMSHORT( sizeof(CNRINFO) ) ) != 0 )
    piniWriteLong( hDSModule, "expfilesSplitbar", stCnrInf.xVertSplitbar );

  // Free titles strings of details view.
  pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO, 0,
                                       MPFROMSHORT( CMA_FIRST ) );
  while( ( pFieldInfo != NULL ) && ( (LONG)pFieldInfo != -1 ) )
  {
    if ( pFieldInfo->pTitleData != NULL )
      debugFree( pFieldInfo->pTitleData );

    pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO,
                    MPFROMP( pFieldInfo ), MPFROMSHORT( CMA_NEXT ) );
  }
}

static VOID _pgFilesContextMenu(HWND hwnd, PFILERECORD pRecord)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  POINTL		ptPos;
  HWND			hwndCtxMenu = WinQueryWindowULong( hwndCnt, QWL_USER );
  SWP			swp;
  BOOL			fRecord = FALSE;

  WinQueryPointerPos( HWND_DESKTOP, &ptPos );
  WinMapWindowPoints( HWND_DESKTOP, hwnd, &ptPos, 1 );

  if ( WinWindowFromPoint( hwnd, &ptPos, TRUE ) == NULLHANDLE )
  {
    // The context menu is probably activated from the keyboard.
    WinQueryWindowPos( hwnd, &swp );
    ptPos.x = swp.cx >> 1;
    ptPos.y = swp.cy >> 1;

    pRecord = (PFILERECORD)WinSendMsg( hwndCnt, CM_QUERYRECORDEMPHASIS,
                            MPFROMLONG(CMA_FIRST), MPFROMSHORT(CRA_SELECTED) );
    fRecord = ( pRecord != NULL ) && ( (LONG)pRecord != -1 );
  }
  else if ( ( pRecord != NULL ) && ( (LONG)pRecord != -1 ) )
    fRecord = (BOOL)WinSendMsg( hwndCnt, CM_SETRECORDEMPHASIS,
                          MPFROMP( pRecord ), MPFROM2SHORT( 1, CRA_SELECTED ) );

  WinEnableMenuItem( hwndCtxMenu, IDM_EXP_COPYNAME, fRecord );
  WinEnableMenuItem( hwndCtxMenu, IDM_EXP_COPYFULLNAME, fRecord );
  WinEnableMenuItem( hwndCtxMenu, IDM_EXP_OPENPATH,
                     fRecord && !IS_DEVICE( pRecord->pszPathName ) );

  WinPopupMenu( hwnd, hwnd, hwndCtxMenu, ptPos.x, ptPos.y, 0,
                PU_NONE | PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1 |
                PU_MOUSEBUTTON2 | PU_KEYBOARD   );
}

static VOID _pgFilesSortCmd(HWND hwnd, ULONG ulCmd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  HWND			hwndCtxMenu = WinQueryWindowULong( hwndCnt, QWL_USER );
  BOOL			fDesc = ( ulCmd == IDM_EXP_SORT_DESCN );
  PGFILESORT		stSort;
  ULONG			ulIdx;
  BOOL			fSort = FALSE;

  _pgFilesGetSort( hwndCtxMenu, &stSort );

  if ( fDesc || ( ulCmd == IDM_EXP_SORT_ASCN ) )
  {
    stSort.fDesc = fDesc;
    fSort = TRUE;
  }
  else
  {
    for( ulIdx = 0; ulIdx < ARRAYSIZE(_pgFile_aFields); ulIdx++ )
      if ( ulCmd == _pgFile_aFields[ulIdx] )
      {
        stSort.ulCmd = ulCmd;
        fSort = TRUE;
        break;
      }
  }

  if ( fSort )
    _pgFilesSetSort( hwndCnt, &stSort );
}

static VOID _pgFileResize(HWND hwnd, PSWP pswp)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  CNRINFO		stCnrInf;
  ULONG			ulRSpace;

  WinSetWindowPos( hwndCnt, 0, 0, 0, pswp->cx, pswp->cy, SWP_SIZE );

  // Stay split bar in visible area.
  if ( (ULONG)WinSendMsg( hwndCnt, CM_QUERYCNRINFO, MPFROMP( &stCnrInf ),
                          MPFROMSHORT( sizeof(CNRINFO) ) ) != 0 )
  {
    ulRSpace = WinQuerySysValue( HWND_DESKTOP, SV_CXVSCROLL ) + 6;
    if ( stCnrInf.xVertSplitbar < pswp->cx - ulRSpace )
      return;

    stCnrInf.xVertSplitbar = pswp->cx - ulRSpace;
    WinSendMsg( hwndCnt, CM_SETCNRINFO, MPFROMP( &stCnrInf ),
                            MPFROMSHORT( CMA_XVERTSPLITBAR ) );
  }
}

static MRESULT EXPENTRY wpfPageFiles(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _pgFilesWMInitDlg( hwnd );
      break;

    case WM_DESTROY:
      _pgFilesWMDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP(mp1) )
      {
        case IDM_EXP_COPYNAME:
          _pgFilesCopyName( hwnd, FALSE );
          return (MRESULT)FALSE;

        case IDM_EXP_COPYFULLNAME:
          _pgFilesCopyName( hwnd, TRUE );
          return (MRESULT)FALSE;

        case IDM_EXP_OPENPATH:
          _pgFilesOpenPath( hwnd );
          return (MRESULT)FALSE;

        default:
          _pgFilesSortCmd( hwnd, SHORT1FROMMP(mp1) );
      }
      return (MRESULT)FALSE;

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_CONT_FILES:
          if ( SHORT2FROMMP( mp1 ) == CN_CONTEXTMENU )
            _pgFilesContextMenu( hwnd, (PFILERECORD)PVOIDFROMMP( mp2 ) );
          break;
      }
      return (MRESULT)FALSE;

    case WM_WINDOWPOSCHANGED:
      if ( ((PSWP)PVOIDFROMMP(mp1))->fl & SWP_SIZE )
        _pgFileResize( hwnd, (PSWP)PVOIDFROMMP(mp1) );
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


//	Page: Modules
//	=============

typedef struct _MODULERECORD {
  RECORDCORE		stRecCore;
  struct _MODULERECORD  *pParent;
  BOOL			fSubTrees;	// Was expanded,subtree scaned.
  PVOID			pLRec;
} MODULERECORD, *PMODULERECORD;

static VOID _pgModulesAddModules(HWND hwndCnt, BOOL fSubLevel,
                                 PMODULERECORD pParent, PVOID pLRec)
{
  ULONG			ulIdx;
  PMODULERECORD		pRecord;
  PMODULERECORD		pRecords;
  PMODULERECORD		pUpScan;
  RECORDINSERT		stRecIns;
  SSMODULE		stModule;

  if ( pLRec == NULL )
    return;

  ssGetModule( pLRec, &stModule );

  // Allocate record.
  pRecords = (PMODULERECORD)WinSendMsg( hwndCnt, CM_ALLOCRECORD,
                   MPFROMLONG( sizeof(MODULERECORD) - sizeof(RECORDCORE) ),
                   MPFROMLONG( stModule.cModules ) );
  if ( pRecord == NULL )
  {
    debug( "CM_ALLOCRECORD returns 0" );
    return;
  }

  stRecIns.cb = sizeof(RECORDINSERT);
  stRecIns.cRecordsInsert = 1;
  stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
  stRecIns.zOrder = (USHORT)CMA_TOP;

  pRecord = pRecords;
  for( ulIdx = 0; ulIdx < stModule.cModules; ulIdx++ )
  {
    // Avoid recursion. Search module's handle in all parent records.
    for( pUpScan = pParent;
         ( pUpScan != NULL ) &&
         ( ssGetModuleHandle( pUpScan->pLRec ) != *stModule.pusModules );
         pUpScan = pUpScan->pParent );

    if ( pUpScan == NULL )
    {
      pRecord->pLRec = ssFindModule( pPtrRec, *stModule.pusModules );
      if ( pRecord->pLRec == NULL )
        debug( "WTF?! Module %u not found", *stModule.pusModules );
      else
      {
        SSMODULE		stModule;

        ssGetModule( pRecord->pLRec, &stModule );

        // Fill record.
        pRecord->stRecCore.cb = sizeof(RECORDCORE);
        pRecord->stRecCore.flRecordAttr = CRA_COLLAPSED;
        pRecord->stRecCore.pszTree = stModule.pszName;
        pRecord->pParent = pParent;

        // Insert record to the container.
        stRecIns.pRecordParent = (PRECORDCORE)pParent;
        WinSendMsg( hwndCnt, CM_INSERTRECORD, (PRECORDCORE)pRecord, &stRecIns );

        if ( fSubLevel )
          _pgModulesAddModules( hwndCnt, FALSE, pRecord, pRecord->pLRec );
        pRecord = (PMODULERECORD)pRecord->stRecCore.preccNextRecord;
      }
    } // pUpScan != NULL

    stModule.pusModules++; // Handle of the next module.
  }
}

static VOID _pgModulesWMInitDlg(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_MODULES );
  HWND			hwndCtxMenu;
  CNRINFO		stCnrInf = { 0 };

  // Load context menu.
  hwndCtxMenu = WinLoadMenu( HWND_OBJECT, hDSModule, ID_EXPMOD_POPUP_MENU );
  WinSetWindowULong( hwndCnt, QWL_USER, hwndCtxMenu );

  stCnrInf.cb = sizeof(CNRINFO);
  stCnrInf.flWindowAttr = CV_TREE | CV_TEXT | CA_TREELINE;
  stCnrInf.slTreeBitmapOrIcon.cx = 14;
  stCnrInf.slTreeBitmapOrIcon.cy = 14;
  WinSendMsg( hwndCnt, CM_SETCNRINFO, &stCnrInf,
              MPFROMLONG( CMA_PFIELDINFOLAST | CMA_XVERTSPLITBAR |
                          CMA_FLWINDOWATTR | CMA_SLTREEBITMAPORICON ) );

  // Add top level modules pf process and modules of one sublevel.
  _pgModulesAddModules( hwndCnt, TRUE, NULL, pProcLRec );
}

static VOID _pgModulesWMDestroy(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_FILES );
  HWND			hwndCtxMenu = WinQueryWindowULong( hwndCnt, QWL_USER );

  // Destroy context menu.
  WinDestroyWindow( hwndCtxMenu );
}

static VOID _pgModulesExpand(HWND hwnd, PMODULERECORD pRecord)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_MODULES );

  if ( pRecord->fSubTrees )
    return;

  // Scan subtree, add modules of one sublevel for each item.

  pRecord->fSubTrees = TRUE;
  pRecord = (PMODULERECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD,
            MPFROMP( pRecord ), MPFROM2SHORT( CMA_FIRSTCHILD, CMA_ITEMORDER ) );

  while( ( pRecord != NULL ) && ( (LONG)pRecord != -1 ) )
  {
    _pgModulesAddModules( hwndCnt, FALSE, pRecord, pRecord->pLRec );

    pRecord = (PMODULERECORD)WinSendMsg( hwndCnt, CM_QUERYRECORD,
                 MPFROMP( pRecord ), MPFROM2SHORT( CMA_NEXT, CMA_ITEMORDER ) );
  }
}

static VOID _pgModulesContextMenu(HWND hwnd, PMODULERECORD pRecord)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_MODULES );
  POINTL		ptPos;
  HWND			hwndCtxMenu = WinQueryWindowULong( hwndCnt, QWL_USER );
  SWP			swp;
  BOOL			fRecord = FALSE;

  WinQueryPointerPos( HWND_DESKTOP, &ptPos );
  WinMapWindowPoints( HWND_DESKTOP, hwnd, &ptPos, 1 );

  if ( WinWindowFromPoint( hwnd, &ptPos, TRUE ) == NULLHANDLE )
  {
    // The context menu is probably activated from the keyboard.
    WinQueryWindowPos( hwnd, &swp );
    ptPos.x = swp.cx >> 1;
    ptPos.y = swp.cy >> 1;

    pRecord = (PMODULERECORD)WinSendMsg( hwndCnt, CM_QUERYRECORDEMPHASIS,
                            MPFROMLONG(CMA_FIRST), MPFROMSHORT(CRA_SELECTED) );
    fRecord = ( pRecord != NULL ) && ( (LONG)pRecord != -1 );
  }
  else if ( ( pRecord != NULL ) && ( (LONG)pRecord != -1 ) )
    fRecord = (BOOL)WinSendMsg( hwndCnt, CM_SETRECORDEMPHASIS,
                          MPFROMP( pRecord ), MPFROM2SHORT( 1, CRA_SELECTED ) );

  if ( fRecord )
    WinPopupMenu( hwnd, hwnd, hwndCtxMenu, ptPos.x, ptPos.y, 0,
                  PU_NONE | PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1 |
                  PU_MOUSEBUTTON2 | PU_KEYBOARD   );
}

static VOID _pgModulesCopyName(HWND hwnd, BOOL fFullName)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_MODULES );
  PMODULERECORD		pRecord;

  pRecord = (PMODULERECORD)WinSendMsg( hwndCnt, CM_QUERYRECORDEMPHASIS,
                            MPFROMLONG(CMA_FIRST), MPFROMSHORT(CRA_SELECTED) );

  if ( ( pRecord != NULL ) && ( (LONG)pRecord != -1 ) )
    _cbCopyFName( pRecord->stRecCore.pszTree, fFullName );
}

static MRESULT EXPENTRY wpfPageModules(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _pgModulesWMInitDlg( hwnd );
      break;

    case WM_DESTROY:
      _pgModulesWMDestroy( hwnd );
      break;

    case WM_COMMAND:
      switch( SHORT1FROMMP(mp1) )
      {
        case IDM_EXP_COPYNAME:
          _pgModulesCopyName( hwnd, FALSE );
          return (MRESULT)FALSE;

        case IDM_EXP_COPYFULLNAME:
          _pgModulesCopyName( hwnd, TRUE );
          return (MRESULT)FALSE;
      }
      return (MRESULT)FALSE;

    case WM_CONTROL:
      switch( SHORT1FROMMP( mp1 ) )
      {
        case IDD_CONT_MODULES:
          if ( SHORT2FROMMP( mp1 ) == CN_CONTEXTMENU )
            _pgModulesContextMenu( hwnd, (PMODULERECORD)PVOIDFROMMP( mp2 ) );
          else if ( SHORT2FROMMP( mp1 ) == CN_EXPANDTREE )
            _pgModulesExpand( hwnd, (PMODULERECORD)PVOIDFROMMP( mp2 ) );
          break;
      }
      return (MRESULT)FALSE;

    case WM_WINDOWPOSCHANGED:
      if ( ((PSWP)PVOIDFROMMP(mp1))->fl & SWP_SIZE )
        WinSetWindowPos( WinWindowFromID( hwnd, IDD_CONT_MODULES ), 0, 0, 0,
                         ((PSWP)PVOIDFROMMP(mp1))->cx,
                         ((PSWP)PVOIDFROMMP(mp1))->cy, SWP_SIZE );
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


//	Page: Threads
//	=============

typedef struct _THREADRECORD {
  RECORDCORE		stRecCore;
  TID			tid;
  PSZ			pszPriority;
  PSZ			pszPriorityClass;
  PSZ			pszState;
  CHAR			acPriority[8];
  CHAR			acPriorityClass[32];
  CHAR			acState[32];
} THREADRECORD, *PTHREADRECORD;

static VOID _pgThreadsWMInitDlg(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_THREADS );
  PFIELDINFO		pFieldInfo;
  CNRINFO		stCnrInf = { 0 };
  PFIELDINFO		pFldInf;
  FIELDINFOINSERT	stFldInfIns;
  PTHREADRECORD		pRecords;
  PTHREADRECORD		pRecord;
  RECORDINSERT		stRecIns;
  PSSTHREAD		pstThreads;
  PSSTHREAD		pstThread;
  ULONG			cThreads;
  ULONG			ulStrId;
  CHAR			szBuf[128];

  // Insert fields.

  pFldInf = (PFIELDINFO)WinSendMsg( hwndCnt, CM_ALLOCDETAILFIELDINFO,
                                    MPFROMLONG( 4 ), NULL );
  if ( pFldInf == NULL )
  {
    debug( "CM_ALLOCDETAILFIELDINFO returns 0" );
    return;
  }
  pFieldInfo = pFldInf;

  pstrLoad( hDSModule, IDS_TID, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_ULONG | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( THREADRECORD, tid );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_PRIORITY, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_RIGHT;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( THREADRECORD, pszPriority );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_PRIORITY_CLASS, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_LEFT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( THREADRECORD, pszPriorityClass );
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pstrLoad( hDSModule, IDS_STATE, sizeof(szBuf), &szBuf );
  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_RIGHT | CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = debugStrDup( &szBuf );
  pFieldInfo->offStruct = FIELDOFFSET( THREADRECORD, pszState );

  stFldInfIns.cb = sizeof(FIELDINFOINSERT);
  stFldInfIns.pFieldInfoOrder = (PFIELDINFO)CMA_FIRST;
  stFldInfIns.cFieldInfoInsert = 4;
  WinSendMsg( hwndCnt, CM_INSERTDETAILFIELDINFO, MPFROMP( pFldInf ),
              MPFROMP( &stFldInfIns ) );

  stCnrInf.cb = sizeof(CNRINFO);
  stCnrInf.flWindowAttr = CV_DETAIL | CA_DETAILSVIEWTITLES;
  WinSendMsg( hwndCnt, CM_SETCNRINFO, &stCnrInf, MPFROMLONG(CMA_FLWINDOWATTR) );

  // Insert records

  // Query list of threads.
  cThreads = ssGetThreadsCount( pPRec );
  pstThreads = debugMAlloc( cThreads * sizeof(SSTHREAD) );
  if ( pstThreads == NULL )
  {
    debug( "Not enough memory" );
    return;
  }
  stRecIns.cRecordsInsert = ssGetThreads( pPRec, pstThreads );

  // Allocate records.
  pRecords = (PTHREADRECORD)WinSendMsg( hwndCnt, CM_ALLOCRECORD,
                       MPFROMLONG( sizeof(THREADRECORD) - sizeof(RECORDCORE) ),
                       MPFROMLONG( cThreads ) );
  if ( pRecords == NULL )
  {
    debug( "CM_ALLOCRECORD returns 0" );
    debugFree( pstThreads );
    return;
  }

  // Fill records.
  pRecord = pRecords;
  for( pstThread = pstThreads; cThreads > 0; cThreads--, pstThread++ )
  {
    switch( pstThread->ulState )
    {
      case SSTHREAD_READY:
        ulStrId = IDS_STATE_READY;
        break;

      case SSTHREAD_BLOCKED:
        ulStrId = IDS_STATE_BLOCKED;
        break;

      case SSTHREAD_RUNNING:
        ulStrId = IDS_STATE_RUNNING;
        break;

      case SSTHREAD_LOADED:
        ulStrId = IDS_STATE_LOADED;
        break;

      default:
        ulStrId = 0;
    }

    if ( ulStrId != 0 )
      pstrLoad( hDSModule, ulStrId, sizeof(pRecord->acState), &pRecord->acState );

    switch( (pstThread->ulPriority >> 8) & 0xFF )
    {
      case PRTYC_IDLETIME:
        ulStrId = IDS_PRTYC_IDLETIME;
        break;

      case PRTYC_REGULAR:
        ulStrId = IDS_PRTYC_REGULAR;
        break;

      case PRTYC_TIMECRITICAL:
        ulStrId = IDS_PRTYC_TIMECRIT;
        break;

      case PRTYC_FOREGROUNDSERVER:
        ulStrId = IDS_PRTYC_FORESERV;
        break;

      default:
        ulStrId = 0;
    }

    if ( ulStrId != 0 )
      pstrLoad( hDSModule, ulStrId, sizeof(pRecord->acPriorityClass),
                &pRecord->acPriorityClass );
    sprintf( &pRecord->acPriority, "0x%X", (LONG)pstThread->ulPriority );

    pRecord->stRecCore.cb = sizeof(RECORDCORE);
    pRecord->tid = pstThread->tid;
    pRecord->pszPriorityClass = &pRecord->acPriorityClass;
    pRecord->pszPriority = &pRecord->acPriority;
    pRecord->pszState = &pRecord->acState;

    pRecord = (PTHREADRECORD)pRecord->stRecCore.preccNextRecord;
  }

  debugFree( pstThreads );

  // Insert records to the container.
  stRecIns.cb = sizeof(RECORDINSERT);
  stRecIns.pRecordOrder = (PRECORDCORE)CMA_END;
  stRecIns.pRecordParent = NULL;
  stRecIns.zOrder = (USHORT)CMA_TOP;
  WinSendMsg( hwndCnt, CM_INSERTRECORD, (PRECORDCORE)pRecords, &stRecIns );
}

static VOID _pgThreadsWMDestroy(HWND hwnd)
{
  HWND			hwndCnt = WinWindowFromID( hwnd, IDD_CONT_THREADS );
  PFIELDINFO		pFieldInfo;

  // Free titles strings of details view.
  pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO, 0,
                                       MPFROMSHORT( CMA_FIRST ) );
  while( ( pFieldInfo != NULL ) && ( (LONG)pFieldInfo != -1 ) )
  {
    if ( pFieldInfo->pTitleData != NULL )
      debugFree( pFieldInfo->pTitleData );

    pFieldInfo = (PFIELDINFO)WinSendMsg( hwndCnt, CM_QUERYDETAILFIELDINFO,
                    MPFROMP( pFieldInfo ), MPFROMSHORT( CMA_NEXT ) );
  }
}

static MRESULT EXPENTRY wpfPageThreads(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _pgThreadsWMInitDlg( hwnd );
      break;

    case WM_DESTROY:
      _pgThreadsWMDestroy( hwnd );
      break;

    case WM_COMMAND:
      return (MRESULT)FALSE;

    case WM_WINDOWPOSCHANGED:
      if ( ((PSWP)PVOIDFROMMP(mp1))->fl & SWP_SIZE )
        WinSetWindowPos( WinWindowFromID( hwnd, IDD_CONT_THREADS ), 0, 0, 0,
                         ((PSWP)PVOIDFROMMP(mp1))->cx,
                         ((PSWP)PVOIDFROMMP(mp1))->cy, SWP_SIZE );
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


//	Page: Program
//	=============

static VOID _ullToStr(PCHAR pcBuf, ULLONG ullVal)
{
  PCHAR		pCh;
  ULONG		ulLen = 1; // '\0'

  ulltoa( ullVal, pcBuf, 10 );
  pCh = strchr( pcBuf, '\0' );

  while( TRUE )
  {
    pCh -= 3;
    if ( pCh <= pcBuf )
      break;

    ulLen += 3;
    memmove( pCh + 1, pCh, ulLen );
    *pCh = ' ';
    ulLen++;
  }
}

static VOID _fileDateTimeToStr(PCOUNTRYINFO pCtryInfo, PFDATE pDate,
                               PFTIME pTime, PCHAR pcBuf)
{
  ULONG		cBytes;
  ULONG		ulDay = *((PUSHORT)pDate) & 0x1F;
  ULONG		ulMonth = (*((PUSHORT)pDate) >> 5) & 0x0F;
  ULONG		ulYear  = ((*((PUSHORT)pDate) >> 9) & 0x7F) + 1980;
  ULONG		ulSec = (*((PUSHORT)pTime) & 0x1F) * 2;
  ULONG		ulMin = (*((PUSHORT)pTime) >> 5) & 0x3F;
  ULONG		ulHours = ((*((PUSHORT)pTime) >> 11) & 0x1F);

  switch( pCtryInfo->fsDateFmt )
  {
    case 1:	// dd/mm/yy
      cBytes = sprintf( pcBuf, "%.2u%s%.2u%s%u", ulDay,
                        pCtryInfo->szDateSeparator,
                        ulMonth, pCtryInfo->szDateSeparator, ulYear );
      break;

    case 2:	// yy/mm/dd
      cBytes = sprintf( pcBuf, "%.2u%s%.2u%s%u", ulYear,
                        pCtryInfo->szDateSeparator,
                        ulMonth, pCtryInfo->szDateSeparator, ulDay );
      break;

    default:	// mm/dd/yy
      cBytes = sprintf( pcBuf, "%.2u%s%.2u%s%u", ulMonth,
                        pCtryInfo->szDateSeparator,
                        ulDay, pCtryInfo->szDateSeparator, ulYear);
      break;
  }

  sprintf( &pcBuf[cBytes], "  %.2u%s%.2u%s%.2u",
           ulHours, pCtryInfo->szTimeSeparator,
           ulMin, pCtryInfo->szTimeSeparator, ulSec );
}

static VOID _pgProgramWMInitDlg(HWND hwnd)
{
  LONG			lColor = SYSCLR_DIALOGBACKGROUND;
  HENUM			hEnum = WinBeginEnumWindows( hwnd );
  HWND			hwndScan;
  CHAR			szBuf[128];
  FILESTATUS3L		stFStat;
  ULONG			ulRC;
  BLDLEVELINFO		stVerInfo;

  // Set background color for entry fields.
  while( TRUE )
  {
    hwndScan = WinGetNextWindow( hEnum );
    if ( hwndScan == NULLHANDLE )
      break;

    if ( ( WinQueryClassName( hwndScan, sizeof(szBuf), szBuf ) != 0 ) &&
         ( strcmp( &szBuf, "#6" ) == 0 ) )
      WinSetPresParam( hwndScan, PP_BACKGROUNDCOLORINDEX, sizeof(LONG), &lColor );
  }
  WinEndEnumWindows( hEnum );

  if ( strcmp( stProcModule.pszName, "SYSINIT" ) == 0 )
    return;

  // Set pathname of program.
  WinSetDlgItemText( hwnd, IDD_EF_FILE, stProcModule.pszName );

  ulRC = DosQueryPathInfo( stProcModule.pszName, FIL_STANDARDL,
                           &stFStat, sizeof(FILESTATUS3L) );
  if ( ulRC == NO_ERROR )
  {
    COUNTRYCODE		stCountry = { 0 };
    COUNTRYINFO		stCtryInfo;
    ULONG		cbCtryInfo;

    _ullToStr( &szBuf, *((PULLONG)&stFStat.cbFile) );
    // Set size in bytes.
    WinSetDlgItemText( hwnd, IDD_EF_SIZE, &szBuf );

    // Set date/time.
    ulRC = DosQueryCtryInfo( sizeof(COUNTRYINFO), &stCountry,
                             &stCtryInfo, &cbCtryInfo );
    if ( ulRC == NO_ERROR )
    {
      CHAR		szDateTime[32];

      _fileDateTimeToStr( &stCtryInfo, &stFStat.fdateCreation,
                          &stFStat.ftimeCreation, &szDateTime );
      WinSetDlgItemText( hwnd, IDD_EF_CREATION, &szDateTime );

      _fileDateTimeToStr( &stCtryInfo, &stFStat.fdateLastWrite,
                          &stFStat.ftimeLastWrite, &szDateTime );
      WinSetDlgItemText( hwnd, IDD_EF_LASTWRITE, &szDateTime );

      _fileDateTimeToStr( &stCtryInfo, &stFStat.fdateLastAccess,
                          &stFStat.ftimeLastAccess, &szDateTime );
      WinSetDlgItemText( hwnd, IDD_EF_LASTACCESS, &szDateTime );
    }
  }

  // Version information (BLDLEVEL).

  ulRC = blGetFromFile( stProcModule.pszName, &stVerInfo );

  if ( ulRC != NO_ERROR )
  {
    if ( ulRC != ERROR_BAD_FORMAT )
      debug( "blGetFromFile(), rc = %u", ulRC );
    return;
  }

  if ( stVerInfo.cbBuild != 0 )
  {
    _snprintf( &szBuf, sizeof(szBuf), "%s build %s",
               &stVerInfo.acRevision, &stVerInfo.acBuild );
    WinSetDlgItemText( hwnd, IDD_EF_REVISION, &szBuf );
  }
  else
    WinSetDlgItemText( hwnd, IDD_EF_REVISION, &stVerInfo.acRevision );

  WinSetDlgItemText( hwnd, IDD_EF_VENDOR, &stVerInfo.acVendor );
  WinSetDlgItemText( hwnd, IDD_EF_DATETIME, &stVerInfo.acDateTime );
  WinSetDlgItemText( hwnd, IDD_EF_BLDMACHINE, &stVerInfo.acBuildMachine );
  WinSetDlgItemText( hwnd, IDD_EF_LANGCODE, &stVerInfo.acLanguageCode );
  WinSetDlgItemText( hwnd, IDD_EF_CNTRCODE, &stVerInfo.acCountryCode );
  WinSetDlgItemText( hwnd, IDD_EF_FIXPACK, &stVerInfo.acFixPackVer );
  WinSetDlgItemText( hwnd, IDD_EF_DESCR, &stVerInfo.acDescription );
}

static VOID _pgProgramResize(HWND hwnd, PSWP pswp)
{
  // Controls that will change their width along with the parent window.
  static ULONG	aulDynWidthItems[] = {
    IDD_EF_FILE, IDD_EF_SIZE, IDD_GB_DATETIME, IDD_EF_CREATION,
    IDD_EF_LASTWRITE, IDD_EF_LASTACCESS, IDD_GB_SIGNATURE, IDD_EF_REVISION,
    IDD_EF_VENDOR, IDD_EF_DATETIME, IDD_EF_BLDMACHINE, IDD_EF_FIXPACK,
    IDD_EF_DESCR, IDD_EF_CNTRCODE
  };
  HWND		hwndScan;
  ULONG		ulIdx;
  SWP		swpScan;
  ULONG		ulROffs;
  CHAR		szBuf[128];

  for( ulIdx = 0; ulIdx < ARRAYSIZE(aulDynWidthItems); ulIdx++ )
  {
    hwndScan = WinWindowFromID( hwnd, aulDynWidthItems[ulIdx] );
    if ( hwndScan == NULLHANDLE )
      continue;

    // Set right border offset.

    WinQueryWindowPos( hwndScan, &swpScan );
    if ( ( WinQueryClassName( hwndScan, sizeof(szBuf), szBuf ) != 0 ) &&
         ( strcmp( &szBuf, "#6" ) == 0 ) )
      ulROffs = 20;	// Enter field
    else
      ulROffs = 10;	// Group box
    // Set new size for control.
    WinSetWindowPos( hwndScan, 0, swpScan.x, swpScan.y,
                     pswp[0].cx - ulROffs - swpScan.x,
                     swpScan.cy, SWP_SIZE );
  }
}

static MRESULT EXPENTRY wpfPageProgram(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _pgProgramWMInitDlg( hwnd );
      break;

    case WM_COMMAND:
      return (MRESULT)FALSE;

    case WM_WINDOWPOSCHANGED:
      if ( (((PSWP)PVOIDFROMMP(mp1))->fl & SWP_SIZE) != 0 )
        _pgProgramResize( hwnd, (PSWP)PVOIDFROMMP(mp1) );
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


//	Explorer dialog main window
//	===========================

static VOID _wpfWMInitDlg(HWND hwnd)
{
  ULONG		ulWidth;
  ULONG		ulHeight;

  ulWidth = piniReadULong( hDSModule, "expWidth", 0 );
  ulHeight = piniReadULong( hDSModule, "expHeight", 0 );
  if ( ( ulWidth > 100 ) && ( ulHeight > 100 ) )
    WinSetWindowPos( hwnd, 0, 0, 0, ulWidth, ulHeight, SWP_SIZE );
}

static VOID _wpfWMDestroy(HWND hwnd)
{
  SWP		swp;

  WinQueryWindowPos( hwnd, &swp );
  piniWriteULong( hDSModule, "expWidth", swp.cx );
  piniWriteULong( hDSModule, "expHeight", swp.cy );
}

static MRESULT EXPENTRY wpfDialog(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  switch( msg )
  {
    case WM_INITDLG:
      _wpfWMInitDlg( hwnd );
      break;

    case WM_DESTROY:
      _wpfWMDestroy( hwnd );
      break;
  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

static VOID _addPage(HWND hwndNB, ULONG ulDlgId, ULONG ulTitleStrId,
                     PFNWP pfnDlgProc)
{
  LONG		lPageId;
  HWND		hwndPage;
  CHAR		szBuf[64];

  hwndPage = WinLoadDlg( hwndNB, hwndNB, pfnDlgProc, hDSModule, ulDlgId, NULL );
  if ( hwndPage == NULLHANDLE )
  {
    debug( "Cannot load dialog page %u", ulDlgId );
    return;
  }

  lPageId = (LONG)WinSendMsg( hwndNB, BKM_INSERTPAGE, NULL,
                    MPFROM2SHORT( BKA_AUTOPAGESIZE | BKA_STATUSTEXTON |
                                  BKA_MAJOR, BKA_LAST ) );

  if ( pstrLoad( hDSModule, ulTitleStrId, sizeof(szBuf), &szBuf ) != 0 )
    WinSendMsg( hwndNB, BKM_SETTABTEXT, MPFROMLONG( lPageId ),
                MPFROMP( &szBuf ) );

  WinSendMsg( hwndNB, BKM_SETPAGEWINDOWHWND, MPFROMLONG( lPageId ),
              MPFROMLONG( hwndPage ) );
  WinSetOwner( hwndPage, hwndNB );

  if ( (SHORT)WinSendMsg( hwndNB, BKM_QUERYPAGECOUNT, 0,
                          MPFROMSHORT( BKA_END ) ) == 1 )
    WinSendMsg( hwndNB, BKM_TURNTOPAGE, MPFROMLONG( lPageId ), NULL );
}

VOID showExplorer(HWND hwndOwner, PID pid)
{
  HWND		hwndDlg, hwndNB;
  ULONG		cbPtrRec = 1024 * 60;
  CHAR		szBuf[128];
  PCHAR		pcSlash;

  pPtrRec = debugMAlloc( cbPtrRec );
  if ( pPtrRec == NULL )
  {
    debug( "Not enough memory" );
    return;
  }

  if ( !querySysState( &cbPtrRec, &pPtrRec ) )
    return;

  pPRec = ssFindProcess( pPtrRec, pid );
  if ( pPRec == NULL )
  {
    debug( "pid %u not found", pid );
    debugFree( pPtrRec );
    return;
  }

  ssGetProcess( pPRec, &stProcess );
  pProcLRec = ssFindModule( pPtrRec, stProcess.hModule );
  if ( pProcLRec == NULL )
  {
    debug( "Module for pid %u not found", pid );
    debugFree( pPtrRec );
    return;
  }
  ssGetModule( pProcLRec, &stProcModule );

  hwndDlg = WinLoadDlg( HWND_DESKTOP, hwndOwner, wpfDialog, hDSModule,
                        IDD_EXPLORER, NULL );
  if ( hwndDlg == NULLHANDLE )
  {
    debug( "WinLoadDlg() fail" );
    debugFree( pPtrRec );
    return;
  }
  pctrlSubclassNotebook( hwndDlg );

  // Add ": " + pszProcess to the title of window.
  WinQueryWindowText( hwndDlg, sizeof(szBuf), &szBuf );
  strlcat( &szBuf, ": ", sizeof(szBuf) );
  pcSlash = strrchr( stProcModule.pszName, '\\' );
  strlcat( &szBuf, pcSlash == NULL ? stProcModule.pszName : pcSlash + 1,
           sizeof(szBuf) );
  WinSetWindowText( hwndDlg, &szBuf );

  hwndNB = WinWindowFromID( hwndDlg, IDD_NOTEBOOK );
  if ( hwndNB == NULLHANDLE )
  {
    debug( "Cannot find notebook, id: %u", IDD_NOTEBOOK );
    WinDestroyWindow( hwndDlg );
    debugFree( pPtrRec );
    return;
  }

  WinSendMsg( hwndNB, BKM_SETNOTEBOOKCOLORS,
              MPFROMLONG ( SYSCLR_FIELDBACKGROUND ),
              MPFROMSHORT( BKA_BACKGROUNDPAGECOLORINDEX ));

  // Insert pages
  _addPage( hwndNB, IDD_EXPLORER_FILES, IDS_EXPLORER_FILES, wpfPageFiles );
  _addPage( hwndNB, IDD_EXPLORER_MODULES, IDS_EXPLORER_MODULES, wpfPageModules );
  _addPage( hwndNB, IDD_EXPLORER_THREADS, IDS_EXPLORER_THREADS, wpfPageThreads );
  _addPage( hwndNB, IDD_EXPLORER_PROGRAM, IDS_EXPLORER_PROGRAM, wpfPageProgram );

  WinSetFocus( HWND_DESKTOP, hwndNB );
  WinProcessDlg( hwndDlg );
  WinDestroyWindow( hwndDlg );

  debugFree( pPtrRec );
}
