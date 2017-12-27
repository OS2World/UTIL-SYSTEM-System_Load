#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSFILEMGR   /* File manager values */
#define INCL_DOSERRORS    /* DOS error values    */
#define INCL_DOSPROCESS     /* Process and thread values */
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSDATETIME
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <ds.h>
#include <sl.h>
#include "diskutil.h"
#include "drives.h"
#include <debug.h>

#define UPDATE_INTERVAL		60000	// msec.
#define SCAN_INTERVAL		1000	// Scan drives interval, msec.
#define SCAN_STACK		65535	// Scan thread's stack size.

#define PROGBAR_VAL_COLOR	0x005090F0
#define PROGBAR_REM_COLOR	0x00EAEAEA

#define SORT_FIELDS		5
#define FLD_LETTER		0
#define FLD_TOTAL		1
#define FLD_FREE		2
#define FLD_FILESYSTEM		3
#define FLD_LABEL		4

#define MAX_DRIVES		27

#define CHFL_DATA		1
#define CHFL_LIST		2

typedef struct _DRIVE {
  ULONG		ulDrv;
  ULONG		ulScanStamp;
  DISKINFO	stDI;
} DRIVE, *PDRIVE;


static ULONG		aulSortBy[SORT_FIELDS] =
{
  IDS_FLD_LETTER,	// 0 - FLD_LETTER
  IDS_FLD_TOTAL,	// 1 - FLD_TOTAL
  IDS_FLD_FREE,		// 2 - FLD_FREE
  IDS_FLD_FILESYSTEM,	// 3 - FLD_FILESYSTEM
  IDS_FLD_LABEL		// 4 - FLD_LABEL
};

// Data source information.
static DSINFO stDSInfo = {
  sizeof(DSINFO),		// Size of this data structure.
  (PSZ)IDS_DS_TITLE,		// Data source module's title (resource string
                    		// ID, see flag DS_FL_RES_MENU_TITLE).
  SORT_FIELDS,			// Number of "sort-by" strings.
  (PSZ *)&aulSortBy,		// "Sort-by" strings (array of resource string
                    		// IDs, see flag DS_FL_RES_SORT_BY).
  DS_FL_NO_BG_UPDATES |		// Flags DS_FL_*
  DS_FL_RES_MENU_TITLE |
  DS_FL_RES_SORT_BY,
  70,				// Items horisontal space, %Em
  50,				// Items vertical space, %Em
  0				// Help main panel index.
};

static ULONG		cDrives;	// Count of drives at apDrives[].
static PDRIVE		apDrives[MAX_DRIVES];	// Drives information records.
static ULONG		ulDrivesChFl;	// List change flags.
static ULONG		ulSortBy;	// Field on which list sorted (FLD_*).
static BOOL		fSortDesc;	// Direction of sort.
static HMTX		hmtxScan;	// Scan thread lock.
static HEV		hevStop;	// Stop signal for scan thread.
static HEV		hevScan;	// Scan all drives signal for thread.
static HEV		hevRefresh;	// Refresh drive signal for thread.
static HEV		hevEject;	// Refresh drive signal for thread.
static HMTX		hmuxScan;	// Mutex sem. for hevStop & hevScan.
static HTIMER		hTimer;		// Scan interval timer.
static TID		tidScan;	// Scan thread Id.
// ulSelDrv - selected drive (0 - A:, 1 - B:, e.t.c.).
static ULONG		ulSelDrv;
// ulMenuDrv - drive for which context menu was created.
static ULONG		ulMenuDrv;

static HPOINTER		hptrAudioCD;
static HPOINTER		hptrCD;
static HPOINTER		hptrFloppy;
static HPOINTER		hptrHDD;
static HPOINTER		hptrTape;
static HPOINTER		hptrUnknown;
static HPOINTER		hptrVDisk;
static HPOINTER		htprRemote;

static ULONG		ulItemHPad = 5;
static ULONG		ulItemVPad = 5;
static ULONG		ulLabelHOffs = 25;
static ULONG		ulLineCY = 10;
static ULONG		ulIconSize;

HMODULE			hDSModule;	// Module handle.
HAB			hab;
BOOL			fShowFloppy;	// Show floppies in list.


// Pointers to helper routines.
PHiniWriteULong			piniWriteULong;
PHiniReadULong			piniReadULong;
PHutilGetTextSize		putilGetTextSize;
PHutil3DFrame			putil3DFrame;
PHutilBox			putilBox;
PHutilMixRGB			putilMixRGB;
PHutilWriteResStrAt		putilWriteResStrAt;
PHutilWriteResStr		putilWriteResStr;
PHutilLoadInsertStr		putilLoadInsertStr;
PHupdateForce			pupdateForce;
PHstrFromBytes			pstrFromBytes;
PHstrLoad			pstrLoad;
PHstrFromULL			pstrFromULL;


// PDRIVE _getDrive(ULONG ulDrv, PULONG pulIndex)
//
// Returns object PDRIVE for ulDrv (0 - A:, 1 - B:, ...) and (optional) index
// of this object in the array apDrives[].
// Returns NULL if ulDrv not listed in apDrives[], *pulIndex == cDrives in this
// case.

static PDRIVE _getDrive(ULONG ulDrv, PULONG pulIndex)
{
  ULONG		ulIdx;
  PDRIVE	pDrive = NULL;

  for( ulIdx = 0; ulIdx < cDrives; ulIdx++ )
  {
    if ( apDrives[ulIdx]->ulDrv == ulDrv )
    {
      pDrive = apDrives[ulIdx];
      break;
    }
  }

  if ( pulIndex != NULL )
    *pulIndex = ulIdx;
  return pDrive;
}

static int _sortComp(const void *p1, const void *p2)
{
  PDRIVE	pDrive1 = *(PDRIVE *)p1;
  PDRIVE	pDrive2 = *(PDRIVE *)p2;
  int		iRes;

  switch( ulSortBy )
  {
    case FLD_LETTER:
      iRes = (LONG)pDrive1->ulDrv - (LONG)pDrive2->ulDrv;
      break;

    case FLD_TOTAL:
      if ( pDrive1->stDI.ullTotal > pDrive2->stDI.ullTotal )
      {
        iRes = 1;
        break;
      }
      else if ( pDrive1->stDI.ullTotal < pDrive2->stDI.ullTotal )
      {
        iRes = -1;
        break;
      }

    case FLD_FREE:
      if ( pDrive1->stDI.ullFree > pDrive2->stDI.ullFree )
      {
        iRes = 1;
        break;
      }
      else if ( pDrive1->stDI.ullFree < pDrive2->stDI.ullFree )
      {
        iRes = -1;
        break;
      }

    case FLD_FILESYSTEM:
      iRes = strcmp( &pDrive1->stDI.szFSDName, &pDrive2->stDI.szFSDName );
      if ( iRes != 0 )
        break;

    default:		// FLD_LABEL
      switch( WinCompareStrings( NULLHANDLE, 0, 0, &pDrive1->stDI.szLabel,
                                 &pDrive2->stDI.szLabel, 0 ) )
      {
        case WCS_LT:
          iRes = -1;
          break;
        case WCS_GT:
          iRes = 1;
          break;
        default: // WCS_EQ:
          iRes = 0;
      }
      break;
  }

  return fSortDesc ? -iRes : iRes;
}

// ULONG _refreshDrive(ULONG ulDrv, ULONG ulScanStamp)
//
// Updates drive ulDrv (0 - A:, 1 - B:, ...) information. Mutex semaphore
// hmtxScan must be locked!
// Returns: 0 - no chages, CHFL_DATA - information updated, CHFL_LIST -
//          new object PDRIVE created and pointer to it places to apDrives[].

static ULONG _refreshDrive(ULONG ulScanDrv, ULONG ulScanStamp)
{
  BOOL		fRes;
  PDRIVE	pDrive;
  DISKINFO	stInfo;
  ULONG		cbScanInfo;
  ULONG		ulDrvIdx;

  // Query drive information.
  DosReleaseMutexSem( hmtxScan );
  fRes = duDiskInfo( ulScanDrv, &stInfo );
  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

  if ( !fRes )
  {
    debug( "duDiskInfo(%u,,) fail", ulScanDrv );
    return 0;
  }

  pDrive = _getDrive( ulScanDrv, &ulDrvIdx );

  // "Real" size of DISKINFO structire. Last field DISKINFO.szFSDName
  // contains only DISKINFO.cbFSDName+1 bytes of data: string and '\0'.
  cbScanInfo = DiskInfoSize( &stInfo );

  if ( pDrive != NULL )
  {
    // Drive record for ulScanDrv found in list - refresh information.

    if ( cbScanInfo == DiskInfoSize( &pDrive->stDI ) )
    {
      pDrive->ulScanStamp = ulScanStamp;

      // Sizes of obtained current drive information in stInfo and
      // listed drive information equal - compare data.
      if ( memcmp( &pDrive->stDI, &stInfo, cbScanInfo ) != 0 )
      {
        // Datas is different - copy new data to the record of drive and
        // set flag "data changed".
        memcpy( &pDrive->stDI, &stInfo, cbScanInfo );
        return CHFL_DATA;
      }
      return 0;
    }
    // Size of drive record changed - a new record will be created.
    debugFree( pDrive );
  }
  else
  {
    // Drive not listed. New record will be created. lDrvIdx equal cDrives.
    if ( cDrives == MAX_DRIVES )
    {
      debug( "WTF?! Too many drives listed." );
      return 0;
    }
    cDrives++;
  }

  // Create new record for drive ulScanDrv and set pointer to it in array.
  pDrive = debugMAlloc( sizeof(DRIVE) - CCHMAXPATH + stInfo.cbFSDName + 1 );
  apDrives[ulDrvIdx] = pDrive;
  if ( pDrive == NULL )
  {
    debug( "Not enough memory" );
    return 0;
  }
  pDrive->ulDrv = ulScanDrv;
  pDrive->ulScanStamp = ulScanStamp;
  memcpy( &pDrive->stDI, &stInfo, cbScanInfo );

  if ( cDrives == 1 )
    ulSelDrv = ulScanDrv;

  // Return flag "list changed".
  return CHFL_LIST;
}

static BOOL _fillCtxMenu(HWND hwndMenu, PDRIVE pDrive)
{
  MENUITEM		stMINew = { 0 };
  CHAR			szBuf[64];

  if ( !pDrive->stDI.fPresent )
    return FALSE;

  stMINew.iPosition = MIT_END;
  stMINew.afStyle = MIS_SEPARATOR;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), 0 );

  pstrLoad( hDSModule, IDS_MI_OPEN, sizeof(szBuf), &szBuf );

  stMINew.afStyle = MIS_TEXT;
  stMINew.id = IDM_DSCMD_FIRST_ID;
  WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );

  if ( pDrive->stDI.fRemovable )
  {
    pstrLoad( hDSModule, IDS_MI_EJECT, sizeof(szBuf), &szBuf );
    stMINew.id = IDM_DSCMD_FIRST_ID + 1;
    WinSendMsg( hwndMenu, MM_INSERTITEM, MPFROMP( &stMINew ), MPFROMP( &szBuf ) );
  }

  return TRUE;
}

VOID APIENTRY ScanThread(ULONG ulData)
{
  ULONG		ulDiskCur;
  ULONG		ulDiskMap;
  ULONG		ulRC;
  ULONG		ulScanDrv;
  LONG		lDrvIdx;
  PDRIVE	pDrive;
  ULONG		ulNewChFl;
  ULONG		ulSem;
  ULONG		ulScanStamp = 0;

  while( TRUE )
  {
    // Wait semaphores.
    ulRC = DosWaitMuxWaitSem( hmuxScan, SEM_INDEFINITE_WAIT, &ulSem );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosWaitMuxWaitSem(), rc = %u", ulRC );
      break;
    }

    if ( ulSem == (ULONG)hevStop )
    {
      debug( "Signal stop received" );
      break;
    }

    // Scan all drives, refresh selected drive or eject signal received.

    ulNewChFl = 0;
    ulRC = DosRequestMutexSem( hmtxScan, 2000 );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosRequestMutexSem(), rc = %u", ulRC );
      break;
    }

    if ( ulSem == (ULONG)hevScan )
    {
      //	Scan all drives.
      //	----------------

      ulScanStamp++; // New iteration of the scan.

      // Query system existing drives.
      ulRC = DosQueryCurrentDisk( &ulDiskCur, &ulDiskMap );
      if ( ulRC != NO_ERROR )
      {
        debug( "DosQueryCurrentDisk(), rc = %u\n", ulRC );
        break;
      }

      // Scan all present drive letters.
      for( ulScanDrv = fShowFloppy ? 0 : 2; ulScanDrv < 26; ulScanDrv++ )
      {
        if ( (( ulDiskMap << (31-ulScanDrv) ) >> 31) == 0 )
          continue;

        pDrive = _getDrive( ulScanDrv, NULL );
        // Do not refresh information about floppies, CD/DVD and remote drives.
        if ( ( pDrive != NULL ) && ( ( pDrive->stDI.ulType == DI_TYPE_FLOPPY )
             || ( pDrive->stDI.ulType == DI_TYPE_CDDVD )
             || ( pDrive->stDI.ulType == DI_TYPE_REMOTE ) ) )
        {
          pDrive->ulScanStamp = ulScanStamp;
          continue;
        }

        ulNewChFl |= _refreshDrive( ulScanDrv, ulScanStamp );

      } // for, ulScanDrv

      // Search and delete disappeared (untouched by during scanning) drives.
      for( lDrvIdx = cDrives - 1; lDrvIdx >= 0; lDrvIdx-- )
        if ( apDrives[lDrvIdx]->ulScanStamp != ulScanStamp )
        {
          // Choose other selected drive if drive that must be removed was selected.
          if ( apDrives[lDrvIdx]->ulDrv == ulSelDrv && cDrives > 1 )
            ulSelDrv =
              apDrives[ lDrvIdx + ( lDrvIdx < (cDrives-1) ? 1 : -1 ) ]->ulDrv;

          // Destroy record and remove from the list.
          debugFree( apDrives[lDrvIdx] );
          cDrives--;
          apDrives[lDrvIdx] = apDrives[cDrives];
/*          memcpy( &apDrives[lDrvIdx], &apDrives[lDrvIdx + 1],
                  (cDrives - lDrvIdx) * sizeof(PDRIVE) );*/
          // List was changed.
          ulNewChFl |= CHFL_LIST;
        }
    }
    else //if ( ulSem == (ULONG)hevRefresh || ulSem == (ULONG)hevEject )
    {
      //	Refresh or eject selected drive.
      //	--------------------------------

      ulScanDrv = ulSelDrv;

      if ( ulSem == (ULONG)hevEject )
      {
        BOOL		fScanList;

        // After removing not CD/DVD devices we'll send a signal to update list.
        pDrive = _getDrive( ulScanDrv, (PULONG)&lDrvIdx );
        fScanList = pDrive != NULL && pDrive->stDI.ulType != DI_TYPE_CDDVD;
        
        DosReleaseMutexSem( hmtxScan );
        duEject( ulScanDrv, TRUE );
        DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

        if ( fScanList )
        {
          DosPostEventSem( hevScan );
          ulScanDrv = ((ULONG)(-1));
        }
      }

      if ( ulScanDrv != ((ULONG)(-1)) )
        ulNewChFl = _refreshDrive( ulScanDrv, 0 );
    }
/*  else { debug( "Unknown signal received." ); } */

    if ( ulNewChFl != 0 )
    {
      // List or listed data changed - sort list and update data source.
      qsort( &apDrives, cDrives, sizeof(PDRIVE), _sortComp );
      ulDrivesChFl |= ulNewChFl;
      pupdateForce( hDSModule );
    }

    DosResetEventSem( (HEV)ulSem, &ulSem );
    DosReleaseMutexSem( hmtxScan );

  } // while, hevStop
}


static VOID _vertGradBox(HPS hps, PRECTL pRect, LONG lColor1, LONG lColor2,
                         LONG lColor3)
{
  LONG		lColor = GpiQueryColor( hps );
  POINTL	pt1, pt2;
  ULONG		ulLine;
  LONG		lYMid = ( pRect->yTop - pRect->yBottom ) / 2;
  LONG		lLastLine;

  if ( WinIsRectEmpty( NULLHANDLE, pRect ) )
    return;

  pt1.x = pRect->xLeft;
  pt1.y = pRect->yTop - 1;
  pt2.x = pRect->xRight - 1;
  lLastLine = pRect->yBottom + lYMid;

  while( TRUE )
  {
    for( ulLine = 0; pt1.y >= lLastLine; ulLine++, pt1.y-- )
    {
      pt2.y = pt1.y;

      GpiSetColor( hps, putilMixRGB( lColor1, lColor2, ulLine * 255 / lYMid ) );
      GpiMove( hps, &pt1 );
      GpiLine( hps, &pt2 );
    }

    if ( pt1.y < pRect->yBottom )
      break;

    lColor1 = lColor2;
    lColor2 = lColor3;
    lLastLine = pRect->yBottom;
  }

  GpiSetColor( hps, lColor );
}

static VOID _spaceBar(HPS hps, PRECTL pRect, ULONG ulMax, ULONG ulVal)
{
  RECTL		rect;

  putil3DFrame( hps, pRect, 0x00FFFFFF, 0x00A0A0A0 );
  WinInflateRect( NULLHANDLE, pRect, -1, -1 );
  putil3DFrame( hps, pRect, 0x00A0A0A0, 0x00FFFFFF );
  WinInflateRect( NULLHANDLE, pRect, -1, -1 );

  rect.yBottom = pRect->yBottom;
  rect.yTop = pRect->yTop;
  rect.xLeft = pRect->xLeft;
  rect.xRight = pRect->xLeft +
                ( ( (pRect->xRight - pRect->xLeft) * ulVal ) / ulMax );
  _vertGradBox( hps, &rect, putilMixRGB( PROGBAR_VAL_COLOR, 0x00FFFFFF, 120 ),
                            PROGBAR_VAL_COLOR,
                            putilMixRGB( PROGBAR_VAL_COLOR, 0x00000000, 100 ) );

  rect.xLeft = rect.xRight;
  rect.xRight = pRect->xRight;
  _vertGradBox( hps, &rect, putilMixRGB( PROGBAR_REM_COLOR, 0x00000000, 20 ),
                            PROGBAR_REM_COLOR,
                            putilMixRGB( PROGBAR_REM_COLOR, 0x00FFFFFF, 40 ) );
}

static VOID _gradArc(HPS hps, ULONG ulMult, ULONG ulStart,
                     ULONG ulSweep, LONG lColor)
{
#define GRAD_ARC_STEP	7
  FIXED		fxMult = MAKEFIXED( ulMult, 0 );
  FIXED		fxStart, fxSweep;
  ULONG		ulStep = ulStart;
  ULONG		ulEnd = ulStart + ulSweep;
  POINTL	ptlPos;
  LONG		lLightColor = putilMixRGB( lColor, 0x00FFFFFF, 80 );
  LONG		lShadowColor = putilMixRGB( lColor, 0x00000000, 80 );
  LONG		lSectColor;

  if ( ulEnd > 360 )
    ulEnd = 360;

  GpiQueryCurrentPosition( hps, &ptlPos );
  fxSweep = MAKEFIXED( GRAD_ARC_STEP, 0 );
  while( ulStep < ulEnd )
  {
    if ( (ulStep + GRAD_ARC_STEP) > ulEnd )
      fxSweep = MAKEFIXED( (ulEnd - ulStep), 0 );

    fxStart = MAKEFIXED( ulStep, 0 );

    {
      LONG		lL = ulStep + ( GRAD_ARC_STEP / 2 );

      if ( ( lL < 45 ) || ( lL > 225 ) )
      {
        if ( ( lL > 225 ) && ( lL < 315 ) )
          lL -= 225;
        else if ( lL >= 315 )
          lL = (315 + 90) - lL;
        else
          lL = 45 - lL;

        lSectColor = putilMixRGB( lColor, lShadowColor, lL * 255 / 90 );
      }
      else
      {
        lL = lL <= 135 ? 135 - lL : lL - 135;
        lSectColor = putilMixRGB( lLightColor, lColor, lL * 255 / 90 );
      }
    }

    GpiSetColor( hps, lSectColor );
    GpiBeginArea( hps, BA_NOBOUNDARY );
    GpiPartialArc( hps, &ptlPos, fxMult, fxStart, fxSweep );
    GpiEndArea( hps );
    GpiMove( hps, &ptlPos );

    ulStep += GRAD_ARC_STEP;
  }
  GpiMove( hps, &ptlPos );

  fxMult = MAKEFIXED( ulMult - (ulMult >> 2), 0 );
  fxStart = MAKEFIXED( ulStart, 0 );
  fxSweep = MAKEFIXED( ulSweep, 0 );
  GpiSetColor( hps, lColor );
  GpiBeginArea( hps, BA_NOBOUNDARY );
  GpiPartialArc( hps, &ptlPos, fxMult, fxStart, fxSweep );
  GpiEndArea( hps );
  GpiMove( hps, &ptlPos );
}

static VOID _Chart(HPS hps, PRECTL pRect, ULONG ulMax, ULONG ulVal)
{
#define CHART_VALUE_COLOR	0x00B92F42
#define CHART_REMAINDER_COLOR	0x0000D1AF
#define CHART_TEXT_COLOR	0x00FFFFFF
#define CHART_SHADOW_COLOR	0x00000000
  ARCPARAMS	arcp = { 1, 1, 0, 0 };	// Structure for arc parameters
  POINTL	ptlPos;
  FIXED		fxMult, fxSweep;
  ULONG		ulR = min( min( (pRect->xRight - pRect->xLeft) >> 1,
                              (pRect->yTop - pRect->yBottom) >> 1 ), 255 );
  CHAR		szBuf[128];
  ULONG		cbBuf;
  SIZEL		sizeText;
  ULONG		ulValGr = 360 * ulVal / ulMax;
  LONG		lSaveColor = GpiQueryColor( hps );

  // Set center.
  ptlPos.x = (pRect->xLeft + pRect->xRight) / 2;
  ptlPos.y = (pRect->yBottom + pRect->yTop) / 2;
  // Set arc parameters.
  GpiSetArcParams( hps, &arcp );

  GpiSetLineWidth( hps, 2 * LINEWIDTH_NORMAL );
  ptlPos.x += 1;
  ptlPos.y -= 1;
  GpiMove( hps, &ptlPos );
  ptlPos.x -= 1;
  ptlPos.y += 1;
  GpiSetColor( hps, 0x00FFFFFF );
  GpiFullArc( hps, DRO_OUTLINE, MAKEFIXED( ulR, 0 ) );
  GpiSetLineWidth( hps, 1 * LINEWIDTH_NORMAL );

  // Value part of chart.
  GpiMove( hps, &ptlPos );
  _gradArc( hps, ulR, 0, ulValGr, CHART_VALUE_COLOR );

  // Remind part of chart.
  GpiMove( hps, &ptlPos );
  _gradArc( hps, ulR, ulValGr, 360 - ulValGr, CHART_REMAINDER_COLOR );

  // Caption.

  fxMult = MAKEFIXED( ulR >> 1, 0 );
  fxSweep = MAKEFIXED( ulValGr >> 1, 0 );
  GpiSetLineType( hps, LINETYPE_INVISIBLE );
  GpiPartialArc( hps, &ptlPos, fxMult, MAKEFIXED( 0, 0 ), fxSweep );
  GpiSetLineType( hps, LINETYPE_DEFAULT );
  GpiQueryCurrentPosition( hps, &ptlPos );

  cbBuf = sprintf( &szBuf, "%u %%", 100 * ulVal / ulMax );

  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );
  ptlPos.x -= sizeText.cx >> 1;
  ptlPos.y -= sizeText.cy >> 1;
  GpiSetColor( hps, CHART_SHADOW_COLOR );
  GpiCharStringAt( hps, &ptlPos, cbBuf, &szBuf );
  ptlPos.x--;
  ptlPos.y++;
  GpiSetColor( hps, CHART_TEXT_COLOR );
  GpiCharStringAt( hps, &ptlPos, cbBuf, &szBuf );

  GpiSetColor( hps, lSaveColor );
}


// Interface routines of data source
// ---------------------------------

DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  debugInit();
  // Store module handle of data source.
  hDSModule = hMod;
  // Store anchor block handle.
  hab = pSLInfo->hab;

  // Query pointers to helper routines.

  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  putilGetTextSize = (PHutilGetTextSize)pSLInfo->slQueryHelper( "utilGetTextSize" );
  putil3DFrame = (PHutil3DFrame)pSLInfo->slQueryHelper( "util3DFrame" );
  putilBox = (PHutilBox)pSLInfo->slQueryHelper( "utilBox" );
  putilMixRGB = (PHutilMixRGB)pSLInfo->slQueryHelper( "utilMixRGB" );
  putilWriteResStrAt = (PHutilWriteResStrAt)pSLInfo->slQueryHelper( "utilWriteResStrAt" );
  putilWriteResStr = (PHutilWriteResStr)pSLInfo->slQueryHelper( "utilWriteResStr" );
  putilLoadInsertStr = (PHutilLoadInsertStr)pSLInfo->slQueryHelper( "utilLoadInsertStr" );
  pupdateForce = (PHupdateForce)pSLInfo->slQueryHelper( "updateForce" );
  pstrFromBytes = (PHstrFromBytes)pSLInfo->slQueryHelper( "strFromBytes" );
  pstrLoad = (PHstrLoad)pSLInfo->slQueryHelper( "strLoad" );
  pstrFromULL = (PHstrFromULL)pSLInfo->slQueryHelper( "strFromULL" );

  // Return data source information for main program
  return &stDSInfo;
}

DSEXPORT VOID APIENTRY dsUninstall()
{
  debugDone();
}

DSEXPORT BOOL APIENTRY dsInit()
{
  ULONG		ulRC;
  SEMRECORD	aSemRec[4];

  cDrives = 0;
  ulDrivesChFl = 0;
  ulSelDrv = ((ULONG)(-1));
  ulMenuDrv = ((ULONG)(-1));

  fShowFloppy = (BOOL)piniReadULong( hDSModule, "ShowFloppy",
                                                (ULONG)DEF_SHOWFLOPPY );
  ulSortBy = piniReadULong( hDSModule, "SortBy", DEF_SORTBY );
  fSortDesc = (BOOL)piniReadULong( hDSModule, "SortDesc", (LONG)DEF_SORTDESC );

  ulRC = DosCreateMutexSem( NULL, &hmtxScan, 0, FALSE );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosCreateMutexSem(), rc = %u", ulRC );
    return FALSE;
  }

  ulRC = DosCreateEventSem( NULL, &hevStop, 0, FALSE );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosCreateEventSem(), rc = %u", ulRC );
    DosCloseMutexSem( hmtxScan );
    return FALSE;
  }
  DosCreateEventSem( NULL, &hevScan, DC_SEM_SHARED, TRUE );
  DosCreateEventSem( NULL, &hevRefresh, DC_SEM_SHARED, TRUE );
  DosCreateEventSem( NULL, &hevEject, DC_SEM_SHARED, TRUE );

  aSemRec[0].hsemCur = (HSEM)hevScan;
  aSemRec[0].ulUser  = (ULONG)hevScan;
  aSemRec[1].hsemCur = (HSEM)hevRefresh;
  aSemRec[1].ulUser  = (ULONG)hevRefresh;
  aSemRec[2].hsemCur = (HSEM)hevEject;
  aSemRec[2].ulUser  = (ULONG)hevEject;
  aSemRec[3].hsemCur = (HSEM)hevStop;
  aSemRec[3].ulUser  = (ULONG)hevStop;
  ulRC = DosCreateMuxWaitSem( NULL, &hmuxScan, ARRAYSIZE( aSemRec ), &aSemRec,
                              DCMW_WAIT_ANY );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosCreateMuxWaitSem(), rc = %u", ulRC );
    DosCloseMutexSem( hmtxScan );
    DosCloseEventSem( hevStop );
    DosCloseEventSem( hevScan );
    DosCloseEventSem( hevRefresh );
    DosCloseEventSem( hevEject );
    return FALSE;
  }

  // We use a separate thread to scan drives because the request for
  // information may block current thread for a few seconds (floppy, CD).
  ulRC = DosCreateThread( &tidScan, ScanThread, 0, CREATE_READY, SCAN_STACK );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosCreateThread(), rc = %u", ulRC );
    DosCloseMutexSem( hmtxScan );
    DosCloseEventSem( hevStop );
    DosCloseEventSem( hevScan );
    DosCloseEventSem( hevRefresh );
    DosCloseEventSem( hevEject );
    DosCloseMuxWaitSem( hmuxScan );
  }

  hptrAudioCD = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_AUDIOCD );
  hptrCD = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_CD );
  hptrFloppy = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_FLOPPY );
  hptrHDD = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_HDD );
  hptrTape = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_TAPE );
  hptrUnknown = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_UNKNOWN );
  hptrVDisk = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_VDISK );
  htprRemote = WinLoadPointer( HWND_DESKTOP, hDSModule, IDI_REMOTE );

  ulRC = DosStartTimer( SCAN_INTERVAL, (HSEM)hevScan, &hTimer );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosStartTimer(), rc = %u", ulRC );
  }

  return TRUE;
}

DSEXPORT VOID APIENTRY dsDone()
{
  ULONG		ulIdx;
  ULONG		ulRC;

  DosStopTimer( hTimer );
  ulRC = DosPostEventSem( hevStop );

  if ( ulRC != NO_ERROR )
    debug( "DosPostEventSem(), rc = %u", ulRC );
  else
  {
    ulRC = DosWaitThread( &tidScan, DCWW_WAIT );
    if ( ulRC != NO_ERROR )
      debug( "DosWaitThread(), rc = %u", ulRC );
  }

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );
  DosCloseMutexSem( hmtxScan );
  DosCloseMuxWaitSem( hmuxScan );
  DosCloseEventSem( hevStop );
  DosCloseEventSem( hevScan );
  DosCloseEventSem( hevRefresh );
  DosCloseEventSem( hevEject );

  debug( "Destroy drives..." );
  for( ulIdx = 0; ulIdx < cDrives; ulIdx++ )
    debugFree( apDrives[ulIdx] );

  debug( "Destroy pointers..." );
  WinDestroyPointer( hptrAudioCD );
  WinDestroyPointer( hptrCD );
  WinDestroyPointer( hptrFloppy );
  WinDestroyPointer( hptrHDD );
  WinDestroyPointer( hptrTape );
  WinDestroyPointer( hptrUnknown );
  WinDestroyPointer( hptrVDisk );
  WinDestroyPointer( htprRemote );
}

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return UPDATE_INTERVAL;
}

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return cDrives;
}

DSEXPORT ULONG APIENTRY dsSortBy(ULONG ulNewSort)
{
  ULONG		ulResult;

  if ( ulNewSort != DSSORT_QUERY )
  {
    DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

    ulSortBy = ulNewSort & DSSORT_VALUE_MASK;
    fSortDesc = (ulNewSort & DSSORT_FL_DESCENDING) != 0;
    // Sort interfaces
    qsort( &apDrives, cDrives, sizeof(PDRIVE), _sortComp );
    // Store new sort type to the ini-file
    piniWriteULong( hDSModule, "SortBy", ulSortBy );
    piniWriteULong( hDSModule, "SortDesc", fSortDesc );

    DosReleaseMutexSem( hmtxScan );
  }

  ulResult = ulSortBy;
  if ( fSortDesc )
    ulResult |= DSSORT_FL_DESCENDING;

  return ulResult;
}

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  ULONG		ulRC;
  ULONG		ulFlags;

  ulRC = DosRequestMutexSem( hmtxScan, 2000 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosRequestMutexSem(), rc = %u", ulRC );
    return DS_UPD_NONE;
  }
  
  ulFlags = ulDrivesChFl;
  ulDrivesChFl = 0;

  DosReleaseMutexSem( hmtxScan );

  if ( ulFlags == 0 )	 // No changes in the list of drives.
    return DS_UPD_NONE;

  // We must return code DS_UPD_LIST if flag CHFL_LIST is set, or both flags
  // CHFL_LIST and CHFL_DATA are set.
  return (ulFlags & CHFL_LIST) != 0 ? DS_UPD_LIST : DS_UPD_DATA;
}

DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex)
{
  BOOL		fSuccess = FALSE;

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

  if ( ulIndex < cDrives )
  {
    fSuccess = TRUE;
    ulSelDrv = apDrives[ulIndex]->ulDrv;

    // Refresh information when floppy, CD/DVD or remote drive selected.
    switch( apDrives[ulIndex]->stDI.ulType )
    {
      case DI_TYPE_FLOPPY:
      case DI_TYPE_CDDVD:
      case DI_TYPE_REMOTE:
        DosPostEventSem( hevRefresh );
    }
  }

  DosReleaseMutexSem( hmtxScan );
  return fSuccess;
}

DSEXPORT ULONG APIENTRY dsGetSel()
{
  ULONG		ulIdx;
  ULONG		ulSelIdx = DS_SEL_NONE;

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

  for( ulIdx = 0; ulIdx < cDrives; ulIdx++ )
  {
    if ( apDrives[ulIdx]->ulDrv == ulSelDrv )
    {
      ulSelIdx = ulIdx;
      break;
    }
  }

  DosReleaseMutexSem( hmtxScan );
  return ulSelIdx;
}

BOOL APIENTRY dsIsSelected(ULONG ulIndex)
{
  BOOL		fSelected;

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );
  fSelected = ( ulIndex < cDrives ) && ( apDrives[ulIndex]->ulDrv == ulSelDrv );
  DosReleaseMutexSem( hmtxScan );

  return fSelected;
}

DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  SIZEL		sizeText;
  PSZ		apszVal[2];
  CHAR		szBuf[128];
  ULONG		cBuf;

  ulIconSize = WinQuerySysValue( HWND_DESKTOP, SV_CXICON );

  putilGetTextSize( hps, 8, "[ W: ]  ", &sizeText );
  ulLabelHOffs = sizeText.cx;
  ulLineCY = sizeText.cy;
  ulItemVPad = ulLineCY >> 1;
  putilGetTextSize( hps, 1, "W", &sizeText );
  ulItemHPad = sizeText.cx;

  apszVal[0] = apszVal[1] = "1000 bytes";
  cBuf = putilLoadInsertStr( hDSModule, TRUE, IDS_BAR_CAPTION,
                                         2, &apszVal, sizeof(szBuf), &szBuf );
  putilGetTextSize( hps, cBuf, &szBuf, &sizeText );
  pSize->cx = ulItemHPad * 3 + ulIconSize + sizeText.cx;

  pSize->cy = (ulItemVPad << 1) + max( ulIconSize, ulLineCY << 2 );
}

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  PDRIVE	pDrive;
  POINTL	pt;
  CHAR		szBuf[128];
  HPOINTER	hIcon;
  RECTL		rectBar;

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

  pDrive = apDrives[ulIndex];

  switch( pDrive->stDI.ulType )
  {
    case DI_TYPE_HDD:
      hIcon = hptrHDD;
      break;

    case DI_TYPE_CDDVD:
      hIcon = hptrCD;
      break;

    case DI_TYPE_TAPE:
      hIcon = hptrTape;
      break;

    case DI_TYPE_FLOPPY:
      hIcon = hptrFloppy;
      break;

    case DI_TYPE_VDISK:
      hIcon = hptrVDisk;
      break;

    case DI_TYPE_REMOTE:
      hIcon = htprRemote;
      break;

    default: // DI_TYPE_UNKNOWN
      hIcon = hptrUnknown;
      break;
  }

  WinDrawPointer( hps, ulItemHPad, ( psizeWnd->cy - ulIconSize ) >> 1,
                  hIcon, DP_NORMAL );

  pt.x = ulItemHPad + ulIconSize + ulItemHPad;
  pt.y = psizeWnd->cy - ulItemVPad - ulLineCY;
  strcpy( &szBuf, "[ C: ]" );
  szBuf[2] = 'A' + pDrive->ulDrv;
  GpiCharStringAt( hps, &pt, 6, &szBuf );

  pt.x += ulLabelHOffs;
  GpiCharStringAt( hps, &pt, pDrive->stDI.cbLabel, &pDrive->stDI.szLabel );

  if ( pDrive->stDI.ullTotal != 0 )
  {
    ULLONG	ullUsed = pDrive->stDI.ullTotal - pDrive->stDI.ullFree;
    CHAR	szFree[16];
    CHAR	szTotal[16];
    PSZ		apszVal[2];
 
    pt.x = ulItemHPad + ulIconSize + ulItemHPad;
    pt.y -= ulLineCY << 1;

    rectBar.xLeft = pt.x;
    rectBar.yBottom = pt.y + (ulLineCY >> 1);
    rectBar.xRight = psizeWnd->cx - ulItemHPad;
    rectBar.yTop = rectBar.yBottom + ulLineCY;
    _spaceBar( hps, &rectBar, 500, 500 * ullUsed / pDrive->stDI.ullTotal );

    pstrFromBytes( pDrive->stDI.ullFree,
                              sizeof(szFree), &szFree, FALSE );
    pstrFromBytes( pDrive->stDI.ullTotal,
                              sizeof(szTotal), &szTotal, FALSE );
    apszVal[0] = &szFree;
    apszVal[1] = &szTotal;
    pt.y -= ulLineCY;
    putilWriteResStrAt( hps, &pt, hDSModule, IDS_BAR_CAPTION, 2, &apszVal );
  }

  DosReleaseMutexSem( hmtxScan );
}

DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  ULONG		ulIdx;
  PDRIVE	pDrive = NULL;
  POINTL	pt, ptCur, ptVal;
  SIZEL		sizeEm;
  ULONG		ulLineHeight;
  SIZEL		sizeText1;
  SIZEL		sizeText2;
  CHAR		szBuf1[32];
  CHAR		szBuf2[32];
  ULONG		ulWidth;
  static ULONG	aulStrResId[5] = { IDS_DTL_LETTER, IDS_DTL_LABEL,
                                   IDS_DTL_FILESYSTEM, IDS_DTL_TOTAL,
                                   IDS_DTL_FREE };

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

  // Find selected drive.
  pDrive = _getDrive( ulSelDrv, NULL );

  if ( pDrive == NULL )
  {
    // No selected drive. Exit.
    DosReleaseMutexSem( hmtxScan );
    return;
  }

  // Chart.

  if ( pDrive->stDI.ullTotal != 0 )
  {
    RECTL		rectChart;
    ULLONG		ullUsed = pDrive->stDI.ullTotal - pDrive->stDI.ullFree;

    rectChart.yBottom = 20;
    rectChart.xRight = psizeWnd->cx - 20;
    rectChart.xLeft = rectChart.xRight - psizeWnd->cy - 40;
    rectChart.yTop = psizeWnd->cy - 20;
    if ( rectChart.xLeft < 20 )
      rectChart.xLeft = 20;
    _Chart( hps, &rectChart, 500, 500 * ullUsed / pDrive->stDI.ullTotal );
  }

  putilGetTextSize( hps, 2, "Em", &sizeEm );
  ulLineHeight = sizeEm.cy << 1;

  // Textual information.

  // Write captions of information lines.
  pt.x = sizeEm.cx << 1;
  pt.y = psizeWnd->cy - ulLineHeight;
  ptVal = pt;
  for( ulIdx = 0; ulIdx < sizeof(aulStrResId) / sizeof(ULONG); ulIdx++ )
  {
    putilWriteResStrAt( hps, &pt, hDSModule, aulStrResId[ulIdx], 0, NULL );
    pt.y -= ulLineHeight;

    GpiQueryCurrentPosition( hps, &ptCur );
    if ( ptVal.x < ptCur.x )
      ptVal.x = ptCur.x;
  }
  ptVal.x += sizeEm.cx;

  // Write values of information lines.
  GpiSetColor( hps, 0x00000000 );
  // Line 1 - drive letter (and "(boot)" mark).
  *((PULONG)&szBuf1) = pDrive->ulDrv + (ULONG)'  :A';
  GpiCharStringAt( hps, &ptVal, 4, &szBuf1 );
  if ( pDrive->stDI.fBoot )
    putilWriteResStr( hps, hDSModule, IDS_BOOT, 0, NULL );
  ptVal.y -= ulLineHeight;
  // Line 2 - drive label.
  GpiCharStringAt( hps, &ptVal, pDrive->stDI.cbLabel, &pDrive->stDI.szLabel );
  ptVal.y -= ulLineHeight;
  // Line 3 - file system name.
  GpiCharStringAt( hps, &ptVal, pDrive->stDI.cbFSDName, &pDrive->stDI.szFSDName );
  ptVal.y -= ulLineHeight;

  // Lines 4 and 5 - total/free bytes on drive.
  if ( pDrive->stDI.ullTotal != 0 )
  {
    pstrFromULL( &szBuf1, sizeof(szBuf1), pDrive->stDI.ullTotal );
    pstrFromULL( &szBuf2, sizeof(szBuf2), pDrive->stDI.ullFree );
    putilGetTextSize( hps, strlen( &szBuf1 ), &szBuf1, &sizeText1 );
    putilGetTextSize( hps, strlen( &szBuf2 ), &szBuf2, &sizeText2 );

    ulWidth = max( sizeText1.cx, sizeText2.cx );

    pt = ptVal;
    pt.x += ulWidth - sizeText1.cx; 
    GpiCharStringAt( hps, &pt, strlen( &szBuf1 ), &szBuf1 );
    ptVal.y -= ulLineHeight;

    pt = ptVal;
    pt.x += ulWidth - sizeText2.cx; 
    GpiCharStringAt( hps, &pt, strlen( &szBuf2 ), &szBuf2 );
    ptVal.y -= ulLineHeight;
  }

  DosReleaseMutexSem( hmtxScan );
}

DSEXPORT VOID APIENTRY dsFillMenu(HWND hwndMenu, ULONG ulIndex)
{
  PDRIVE		pDrive;

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

  ulMenuDrv = (ULONG)(-1);

  if ( ulIndex < cDrives )
  {
    pDrive = apDrives[ulIndex];

    // For floppies/CD/DVD we need update information (media present or not?).
    if ( ( ( pDrive->stDI.ulType == DI_TYPE_FLOPPY ) ||
           ( pDrive->stDI.ulType == DI_TYPE_CDDVD ) )
         &&
           ( _refreshDrive( pDrive->ulDrv, 0 ) != 0 ) )
    {
      // Have changes on floppy/CD/DVD drive. Reset number of drives and send
      // signal "scan" to trigger visible update.
      cDrives = 0;
      DosPostEventSem( hevScan );
      // Drive record can be recreated by _refreshDrive(). Get pointer again.
      pDrive = apDrives[ulIndex];
    }

    if ( _fillCtxMenu( hwndMenu, pDrive ) )
      ulMenuDrv = pDrive->ulDrv;
  }

  DosReleaseMutexSem( hmtxScan );
}

DSEXPORT ULONG APIENTRY dsCommand(HWND hwndOwner, USHORT usCommand)
{
  PDRIVE	pDrive;
  HOBJECT	hObject;
  ULONG		ulDrv;
  CHAR		szBuf[16];

  DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );

  pDrive = _getDrive( ulMenuDrv, NULL );
  ulMenuDrv = (ULONG)(-1);

  if ( ( pDrive == NULL ) || !pDrive->stDI.fPresent )
  {
    DosReleaseMutexSem( hmtxScan );
    return DS_UPD_NONE;
  }
  ulDrv = pDrive->ulDrv;

  DosReleaseMutexSem( hmtxScan );

  switch( usCommand )
  {
    case IDM_DSCMD_FIRST_ID:		// Open
      sprintf( &szBuf, "<WP_DRIVE_%c>", ulDrv + 'A' );
      hObject = WinQueryObject( &szBuf );

      if ( hObject != NULLHANDLE &&
           WinOpenObject( hObject, OPEN_DEFAULT, TRUE ) )
        WinOpenObject( hObject, OPEN_DEFAULT, TRUE ); // Move to the fore.
      break;

    case IDM_DSCMD_FIRST_ID + 1:	// Eject
      {
        DosRequestMutexSem( hmtxScan, SEM_INDEFINITE_WAIT );
        ulSelDrv = ulDrv;
        DosPostEventSem( hevEject );
        DosReleaseMutexSem( hmtxScan );
      }
      return DS_UPD_DATA;
  }

  return DS_UPD_NONE;
}

DSEXPORT ULONG APIENTRY dsEnter(HWND hwndOwner)
{
  ulMenuDrv = ulSelDrv;
  return dsCommand( hwndOwner, IDM_DSCMD_FIRST_ID );
}
