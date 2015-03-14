#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <sys\socket.h>
#include <sys\ioctl.h>
#include <net\route.h>
#include <net\if.h>
#include <net\if_arp.h>
#include <arpa\inet.h>
#include <unistd.h>
#include <ds.h>
#include <sl.h>
#include "net.h"
#include "debug.h"


#define MAX_INTERFACES		IFMIB_ENTRIES
#define MAX_STAT_ADDR		64

#define FIELDS			8
#define FLD_INDEX		0
#define FLD_DESCRIPTION		1
#define FLD_TYPE		2
#define FLD_MAC			3
#define FLD_BYTESSENT		4
#define FLD_BYTESRECV		5
#define FLD_TXSPEED		6
#define FLD_RXSPEED		7


typedef struct _INTERFACE {
  struct iftable	stIFT;
    // http://www.net-snmp.org/docs/mibs/interfaces.html
    // iftType - http://www.net-snmp.org/docs/mibs/interfaces.html
    //           https://www.iana.org/assignments/ianaiftype-mib/ianaiftype-mib
    // iftOperStatus - http://www.alvestrand.no/objectid/1.3.6.1.2.1.2.2.1.8.html
  GRVAL			stGVRecv;
  GRVAL			stGVSent;
  ULONG			ulUpdate;
  BOOL			fSelected;
} INTERFACE, *PINTERFACE;

// os2_ioctl() for SIOSTATAT cmd result

#pragma pack(1)

typedef struct _IOSTATATADDR {
  ULONG		ulIP;
  USHORT	usIFIdx;
  ULONG		ulMask;
  ULONG		ulBroadcast;
} IOSTATATADDR, *PIOSTATATADDR;

typedef struct _IOSTATAT {
  USHORT		cAddr;
  IOSTATATADDR		aAddr[MAX_STAT_ADDR];
} IOSTATAT, *PIOSTATAT;

#pragma pack()


// Module information

static PSZ		 apszFields[FIELDS] = { NULL };

static DSINFO stDSInfo = {
  sizeof(DSINFO),			// Size of this data structure.
  "~Network",				// Menu item title
  FIELDS,				// Number of "sort-by" strings
  &apszFields,				// "Sort-by" strings.
  0,					// Flags DS_FL_*
  70,					// Items horisontal space, %Em
  50					// Items vertical space, %Em
};

// Graph Options

static LONG grValToStr(ULONG ulVal, PCHAR pcBuf, ULONG cbBuf);

static GRVALPARAM	aGrValParam[2] =
{
  // RX graph
  {
    DEF_RXCOLOR,	// clrGraph
    2			// lLineWidth
  },
  // TX graph
  {
    DEF_TXCOLOR,	// clrGraph
    2			// lLineWidth
  }
};

GRPARAM		stGrParam =
{
  GRPF_MIN_LABEL | GRPF_MAX_LABEL | GRPF_TIME_LABEL/* | GRPF_LEFT_TOP_CAPTION*/,
  NULL,			// pszAbscissaTopCaption
  NULL,			// pszAbscissaBottomCaption
  NULL,			// pszOrdinateCaption
  grValToStr,		// fnValToStr
  1,			// ulBorderCX
  1,			// ulBorderCY
  DEF_GRBORDERCOLOR,	// clrBorder
  DEF_GRBGCOLOR,	// clrBackground
  2,			// cParamVal
  &aGrValParam		// pParamVal
};

HMODULE			hDSModule;	// Module handle
static PINTERFACE	apInterfaces[MAX_INTERFACES];
static ULONG		cInterfaces;
// cUpdate - A simple updates counter. To determine disappeared interfaces.
static ULONG		cUpdate = 0;
static int		iSock;
GRAPH			stGraph;
static PIOSTATAT	pstStatAT;
PROPERTIES		stProperties =
{
  FLD_INDEX,		// ulSort
  FALSE,		// fSortDesc
  DEF_INTERVAL,		// ulUpdateInterval, msec.
  DEF_TIMEWIN		// ulTimeWindow, sec.
};
static PSZ		apszOperStatus[7] =
  { "Up", "Down", "Testing", "Unknown", "Dormant", "Not present",
    "Lower layer down" };
// Resource IDs for fields names.
static ULONG aulFldStrId[] = {
  IDS_FLD_INDEX,	// 0 - FLD_INDEX
  IDS_FLD_DESCR,	// 1 - FLD_DESCRIPTION
  IDS_FLD_TYPE,		// 2 - FLD_TYPE
  IDS_FLD_MAC,		// 3 - FLD_MAC
  IDS_FLD_SENT,		// 4 - FLD_BYTESSENT
  IDS_FLD_RECV,		// 5 - FLD_BYTESRECV
  IDS_FLD_TXSPD,	// 6 - FLD_TXSPEED
  IDS_FLD_RXSPD		// 7 - FLD_RXSPEED
};


// ulIdxWidth
//    | ulLPad        ulInfWidth      ulRPad
//    |    |              |              |
// |<-+->|<+>|<-----------+----------->|<+>|
//
// ----------------------------------------- --,
// |     |                                 |   |- ulTPad
// | Idx |   Up Ethernet-Csmacd 100 Mb/s   | -<
// |     |   00:01:02:03:04:05 MTU: 1500   |   |- ulLineHeight
// |     |   - RX: 1234 Mb  1.2 Kbit/sec   | --
// |     |   - TX: 1234 Mb  0.5 Kbit/sec   | --,
// |     |                                 |   |- ulBPad
// ----------------------------------------- --'
//
//           | |<+>|       |
//           |   |         |
//           | ulXLabWidth |
//           |<-----+----->|
//                  |
//              ulSpdLOffs

// These values are calculated in the function dsSetWndStart()
static ULONG		ulIdxWidth = 60;
static ULONG		ulLPad = 10;
static ULONG		ulRPad = 10;
static ULONG		ulTPad = 10;
static ULONG		ulBPad = 10;
static ULONG		ulInfWidth = 240;
static ULONG		ulXLabWidth = 30;
static ULONG		ulSpdLOffs = 130;
static ULONG		ulLineHeight = 20;
static ULONG		ulCharHeight = 15;
static ULONG		ulCharSpace = 10;
static ULONG		ulLedWidth = 10;
static ULONG		ulLedHeight = 5;

#define ULCMP(v1,v2) ( (v1) > (v2) ? 1 : ( (v1) < (v2) ? -1 : 0 ) )

// Pointers to helper routines.

PHiniWriteULong			piniWriteULong;
PHiniReadULong			piniReadULong;
PHgrInit			pgrInit;
PHgrDone			pgrDone;
PHgrSetTimeScale		pgrSetTimeScale;
PHgrNewTimestamp		pgrNewTimestamp;
PHgrGetTimestamp		pgrGetTimestamp;
PHgrInitVal			pgrInitVal;
PHgrDoneVal			pgrDoneVal;
PHgrSetValue			pgrSetValue;
PHgrGetValue			pgrGetValue;
PHgrDraw			pgrDraw;
PHutilGetTextSize		putilGetTextSize;
PHupdateLock			pupdateLock;
PHupdateUnlock			pupdateUnlock;
PHstrFromSec			pstrFromSec;
PHstrFromBits			pstrFromBits;
PHstrFromBytes			pstrFromBytes;
PHstrLoad			pstrLoad;
PHctrlStaticToColorRect		pctrlStaticToColorRect;


static PINTERFACE _getSelInterface()
{
  ULONG			ulIdx;

  for( ulIdx = 0; ulIdx < cInterfaces; ulIdx++ )
    if ( apInterfaces[ulIdx]->fSelected )
      return apInterfaces[ulIdx];

  return NULL;
}

static VOID _destroyIterface(PINTERFACE pInterface)
{
  pgrDoneVal( &pInterface->stGVSent );
  pgrDoneVal( &pInterface->stGVRecv );
  debugFree( pInterface );
}

static int _sortComp(const void *p1, const void *p2)
{
  PINTERFACE	pIF1 = *(PINTERFACE *)p1;
  PINTERFACE	pIF2 = *(PINTERFACE *)p2;
  int		iRes;
  ULONG		ulSpeed1, ulSpeed2;

  switch( stProperties.ulSort )
  {
    case FLD_DESCRIPTION:
      iRes = strcmp( &pIF1->stIFT.iftDescr, &pIF2->stIFT.iftDescr );
      break;

    case FLD_TYPE:
      iRes = pIF1->stIFT.iftType - pIF2->stIFT.iftType;
      break;

    case FLD_MAC:
      iRes = memcmp( pIF1->stIFT.iftPhysAddr, pIF2->stIFT.iftPhysAddr, 6 );
      break;

    case FLD_BYTESSENT:
      iRes = ULCMP( pIF1->stIFT.iftOutOctets, pIF2->stIFT.iftOutOctets );
      break;

    case FLD_BYTESRECV:
      iRes = ULCMP( pIF1->stIFT.iftInOctets, pIF2->stIFT.iftInOctets );
      break;

    case FLD_TXSPEED:
      if ( pgrGetValue( &stGraph, &pIF1->stGVSent, &ulSpeed1 ) &&
           pgrGetValue( &stGraph, &pIF2->stGVSent, &ulSpeed2 ) )
      {
        iRes = ulSpeed1 - ulSpeed2;
      }
      break;

    case FLD_RXSPEED:
      if ( pgrGetValue( &stGraph, &pIF1->stGVRecv, &ulSpeed1 ) &&
           pgrGetValue( &stGraph, &pIF2->stGVRecv, &ulSpeed2 ) )
      {
        iRes = ulSpeed1 - ulSpeed2;
      }
      break;

    default: // FLD_INDEX
      iRes = pIF1->stIFT.iftIndex - pIF2->stIFT.iftIndex;
      break;
  }

  return stProperties.fSortDesc ? -iRes : iRes;
}

static VOID _drawLed(HPS hps, POINTL pt, LONG lColor, BOOL fOn)
{
  RECTL		rect;
  HRGN		hrgn;
  LONG		lSaveColor;
  SIZEL		size = { 1, 1 };

  rect.xLeft   = pt.x;
  rect.yBottom = pt.y + ( ulCharHeight - ulLedHeight ) / 2;
  rect.xRight  = rect.xLeft + ulLedWidth;
  rect.yTop    = rect.yBottom + ulLedHeight;
  hrgn = GpiCreateRegion( hps, 1, &rect );

  lColor = fOn ? lColor : ( ( lColor >> 1 ) & 0x007F7F7F );

  lSaveColor = GpiQueryColor( hps );
  GpiSetColor( hps, lColor );
  GpiPaintRegion( hps, hrgn );
  GpiSetColor( hps, ( lColor >> 1 ) & 0x007F7F7F );
  GpiFrameRegion( hps, hrgn, &size );
  GpiSetColor( hps, lSaveColor );
  GpiDestroyRegion( hps, hrgn );

  pt.x += ulLedWidth;
  GpiMove( hps, &pt );
}

static LONG grValToStr(ULONG ulVal, PCHAR pcBuf, ULONG cbBuf)
{
  return pstrFromBytes( ulVal, cbBuf, pcBuf, TRUE );
}


// Interface routines of data source
// ---------------------------------

DSEXPORT PDSINFO APIENTRY dsInstall(HMODULE hMod, PSLINFO pSLInfo)
{
  ULONG		ulIdx;
  CHAR		szBuf[128];

  debugInit();

  // Store module handle of data source
  hDSModule = hMod;

  // Query pointers to helper routines.

  piniWriteULong = (PHiniWriteULong)pSLInfo->slQueryHelper( "iniWriteULong" );
  piniReadULong = (PHiniReadULong)pSLInfo->slQueryHelper( "iniReadULong" );
  pgrInit = (PHgrInit)pSLInfo->slQueryHelper( "grInit" );
  pgrDone = (PHgrDone)pSLInfo->slQueryHelper( "grDone" );
  pgrSetTimeScale = (PHgrSetTimeScale)pSLInfo->slQueryHelper( "grSetTimeScale" );
  pgrNewTimestamp = (PHgrNewTimestamp)pSLInfo->slQueryHelper( "grNewTimestamp" );
  pgrGetTimestamp = (PHgrGetTimestamp)pSLInfo->slQueryHelper( "grGetTimestamp" );
  pgrInitVal = (PHgrInitVal)pSLInfo->slQueryHelper( "grInitVal" );
  pgrDoneVal = (PHgrDoneVal)pSLInfo->slQueryHelper( "grDoneVal" );
  pgrSetValue = (PHgrSetValue)pSLInfo->slQueryHelper( "grSetValue" );
  pgrGetValue = (PHgrGetValue)pSLInfo->slQueryHelper( "grGetValue" );
  pgrDraw = (PHgrDraw)pSLInfo->slQueryHelper( "grDraw" );
  putilGetTextSize = (PHutilGetTextSize)pSLInfo->slQueryHelper( "utilGetTextSize" );
  pupdateLock = (PHupdateUnlock)pSLInfo->slQueryHelper( "updateLock" );
  pupdateUnlock = (PHupdateUnlock)pSLInfo->slQueryHelper( "updateUnlock" );
  pstrFromSec = (PHstrFromSec)pSLInfo->slQueryHelper( "strFromSec" );
  pstrFromBits = (PHstrFromBits)pSLInfo->slQueryHelper( "strFromBits" );
  pstrFromBytes = (PHstrFromBytes)pSLInfo->slQueryHelper( "strFromBytes" );
  pstrLoad = (PHstrLoad)pSLInfo->slQueryHelper( "strLoad" );
  pctrlStaticToColorRect = (PHctrlStaticToColorRect)pSLInfo->slQueryHelper( "ctrlStaticToColorRect" );

  // Load names of fields
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    pstrLoad( hMod, aulFldStrId[ulIdx], sizeof(szBuf), &szBuf );
    apszFields[ulIdx] = debugStrDup( &szBuf );
  }

  // Return data source information for main program
  return &stDSInfo;
}

DSEXPORT VOID APIENTRY dsUninstall()
{
  ULONG		ulIdx;

  debug( "Free names of fields..." );
  for( ulIdx = 0; ulIdx < FIELDS; ulIdx++ )
  {
    if ( apszFields[ulIdx] != NULL )
      debugFree( apszFields[ulIdx] );
  }

  debugDone();
}

DSEXPORT BOOL APIENTRY dsInit()
{
  ULONG		ulTime;
  ULONG		ulValWindow;
  CHAR		szBuf[128];

  cInterfaces = 0;

  // Load graph ordinate's caption ("Speed")
  pstrLoad( hDSModule, IDS_SPEED, sizeof(szBuf), &szBuf );
  stGrParam.pszOrdinateCaption = debugStrDup( &szBuf );

  pstStatAT = debugMAlloc( sizeof(IOSTATAT) );
  if ( pstStatAT == NULL )
  {
    debug( "Not enough memory" );
    return FALSE;
  }

  ulTime = stProperties.ulTimeWindow * 1000; /* Time window, msec. */
  ulValWindow = ulTime / stProperties.ulUpdateInterval;

  if ( !pgrInit( &stGraph, ulValWindow, ulTime, 0, 1024 ) )
  {
    debug( "grInit() fail" );
    debugFree( pstStatAT );
    return FALSE;
  }

  sock_init();
  iSock = socket( AF_INET, SOCK_RAW, 0 );
  if ( iSock == -1 )
  {
    debug( "Cannot create a socket" );
    debugFree( pstStatAT );
    pgrDone( &stGraph );
    return FALSE;
  }

  // Read properties from the ini-file

  stGrParam.clrBackground =
    piniReadULong( hDSModule, "GrBgColor", DEF_GRBGCOLOR );
  stGrParam.clrBorder =
    piniReadULong( hDSModule, "GrBorderColor", DEF_GRBORDERCOLOR );
  stGrParam.pParamVal[0].clrGraph =
    piniReadULong( hDSModule, "RXColor", DEF_RXCOLOR );
  stGrParam.pParamVal[1].clrGraph =
    piniReadULong( hDSModule, "TXColor", DEF_TXCOLOR );

  stProperties.ulSort = piniReadULong( hDSModule, "Sort", FLD_INDEX );
  stProperties.fSortDesc = piniReadULong( hDSModule,
                                                     "SortDesc", FALSE );
  stProperties.ulUpdateInterval = piniReadULong( hDSModule,
                                    "UpdateInterval", DEF_INTERVAL );
  if ( stProperties.ulUpdateInterval < 500 ||
       stProperties.ulUpdateInterval > 2000 ||
       stProperties.ulUpdateInterval % 100 != 0 )
    stProperties.ulUpdateInterval = DEF_INTERVAL;
  stProperties.ulTimeWindow = piniReadULong( hDSModule,
                                    "TimeWindow", DEF_TIMEWIN );

  return TRUE;
}

DSEXPORT VOID APIENTRY dsDone()
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cInterfaces; ulIdx++ )
    _destroyIterface( apInterfaces[ulIdx] );

  pgrDone( &stGraph );
  soclose( iSock );
  debugFree( pstStatAT );

  if ( stGrParam.pszOrdinateCaption != NULL )
    debugFree( stGrParam.pszOrdinateCaption );
}

DSEXPORT ULONG APIENTRY dsGetUpdateDelay()
{
  return stProperties.ulUpdateInterval;
}

DSEXPORT ULONG APIENTRY dsGetCount()
{
  return cInterfaces;
}

DSEXPORT VOID APIENTRY dsSetWnd(ULONG ulIndex, HWND hwnd, HPS hps)
{
}

DSEXPORT ULONG APIENTRY dsUpdate(ULONG ulTime)
{
  struct ifmib		stIfMIB = { 0 };
  struct iftable	*pIFT;
  PINTERFACE		pInterface;
  ULONG			ulStatIdx;
  LONG			lIFIdx;
  ULONG			ulDelta;
  BOOL			fListChanged = FALSE;
  BOOL			fFound;
  BOOL			fInterval;
  ULONG			ulInterval;
//  PINTERFACE		apInterfacesOld[MAX_INTERFACES];

  if ( os2_ioctl( iSock, SIOSTATIF42, (caddr_t)&stIfMIB,
                  sizeof(struct ifmib) ) == -1 )
  {
    debug( "os2_ioctl(,SIOSTATIF42,,) fail, errno = %d", sock_errno() );
    return DS_UPD_NONE;
  }

//  memcpy( &apInterfacesOld, &apInterfaces, cInterfaces * sizeof(PINTERFACE) );
  fInterval = pgrGetTimestamp( &stGraph, &ulInterval );
  pgrNewTimestamp( &stGraph, ulTime );
  if ( fInterval )
  {
    ulInterval = ulTime - ulInterval;
    if ( ulInterval < 100 )
      // This can happen when user switches between enable/disable states
      // of data source. Avoid division by zero and inaccurate result.
      return DS_UPD_NONE;
  }

  for( ulStatIdx = 0; ulStatIdx < stIfMIB.ifNumber; ulStatIdx++ )
  {
    pIFT = &stIfMIB.iftable[ulStatIdx];

    // Skeep loopback interface
    if ( pIFT->iftDescr[0] == '\0' && pIFT->iftSpeed == 0 )
      continue;

    // Search interface's record in the list.
    fFound = FALSE;
    for( lIFIdx = 0; lIFIdx < cInterfaces; lIFIdx++ )
    {
      pInterface = apInterfaces[lIFIdx];

      if ( pIFT->iftIndex == pInterface->stIFT.iftIndex &&
           pIFT->iftType == pInterface->stIFT.iftType
           /* && need compare iftPhysAddr ? */ )
      {
        fFound = TRUE;
        break;
      }
    }

    if ( !fFound )
    {
      if ( cInterfaces == MAX_INTERFACES )
      {
        debug( "WTF? Too many interfaces listed." );
        continue;
      }

      // There is no interface's record in list - create new.
      pInterface = debugMAlloc( sizeof(INTERFACE) );
      if ( pInterface == NULL )
      {
        debug( "Warning! Not enough memory." );
        continue;
      }
      // Initialize values storages of graphs.
      pgrInitVal( &stGraph, &pInterface->stGVSent );
      pgrInitVal( &stGraph, &pInterface->stGVRecv );
      // Set flag "selected" for first interface in list.
      if ( cInterfaces == 0 )
        pInterface->fSelected = TRUE;
      // Add new interface to the list.
      apInterfaces[cInterfaces++] = pInterface;

      fListChanged = TRUE;
    }
    else if ( fInterval )
    {
      ulDelta = pIFT->iftOutOctets - pInterface->stIFT.iftOutOctets;
      // Correct delta value with realy updates time interval.
      // Or: delta bytes to speed.
      ulDelta = (ULONG)( ( (ULLONG)ulDelta * 1000 ) / ulInterval );
      pgrSetValue( &stGraph, &pInterface->stGVSent, ulDelta );

      ulDelta = pIFT->iftInOctets - pInterface->stIFT.iftInOctets;
      ulDelta = (ULONG)( ( (ULLONG)ulDelta * 1000 ) / ulInterval );
      pgrSetValue( &stGraph, &pInterface->stGVRecv, ulDelta );
    }

    // Copy data from record iftable to the interface's record.
    memcpy( &pInterface->stIFT, pIFT, sizeof(struct iftable) );
    // Sign interface's record "updated".
    pInterface->ulUpdate = cUpdate;
  }

  // Remove disappeared (not updated) interfaces from the list.
  if ( cInterfaces > 0 )
  for( lIFIdx = cInterfaces - 1; lIFIdx >= 0; lIFIdx-- )
  {
    pInterface = apInterfaces[lIFIdx];

    if ( pInterface->ulUpdate != cUpdate )
    {
      // Move "selected" mark to other interface.
      if ( pInterface->fSelected )
      {
        if ( lIFIdx < (cInterfaces - 1) )
          apInterfaces[lIFIdx + 1]->fSelected = TRUE;
        else if ( lIFIdx > 0 )
          apInterfaces[lIFIdx - 1]->fSelected = TRUE;
      }
      _destroyIterface( pInterface );

      // Cut pointer to destroyed interface's record from the list.
      cInterfaces--;
      if ( lIFIdx != cInterfaces )
        memcpy( &apInterfaces[lIFIdx], &apInterfaces[lIFIdx + 1],
                ( cInterfaces - lIFIdx ) * sizeof(PINTERFACE) );

      fListChanged = TRUE;
    }
  }  

  // Next update-stamp.
  cUpdate++;

  // Sort list of interfaces.
  qsort( apInterfaces, cInterfaces, sizeof(PINTERFACE), _sortComp );

  // Check - does order of records changed?
//  fListChanged = memcmp( &apInterfacesOld, &apInterfaces,
//                         cInterfaces * sizeof(PINTERFACE) ) != 0;

  // Gets all interface addresses.
  if ( os2_ioctl( iSock, SIOSTATAT, (caddr_t)pstStatAT,
                  sizeof(IOSTATAT) ) == -1 )
  {
    debug( "os2_ioctl(,SIOSTATAT,,) fail, errno = %d", sock_errno() );
  }

  return fListChanged ? DS_UPD_LIST : DS_UPD_DATA;
}

DSEXPORT BOOL APIENTRY dsSetSel(ULONG ulIndex)
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cInterfaces; ulIdx++ )
    apInterfaces[ulIdx]->fSelected = ( ulIdx == ulIndex );

  return TRUE;
}

DSEXPORT ULONG APIENTRY dsGetSel()
{
  ULONG		ulIdx;

  for( ulIdx = 0; ulIdx < cInterfaces; ulIdx++ )
    if ( apInterfaces[ulIdx]->fSelected )
      return ulIdx;

  return DS_SEL_NONE;
}

DSEXPORT BOOL APIENTRY dsIsSelected(ULONG ulIndex)
{
  return ulIndex < cInterfaces ? apInterfaces[ulIndex]->fSelected : FALSE;
}

DSEXPORT ULONG APIENTRY dsSortBy(ULONG ulNewSort)
{
  ULONG		ulResult;

  if ( ulNewSort != DSSORT_QUERY )
  {
    stProperties.ulSort = ulNewSort & DSSORT_VALUE_MASK;
    stProperties.fSortDesc = (ulNewSort & DSSORT_FL_DESCENDING) != 0;
    // Sort interfaces
    qsort( apInterfaces, cInterfaces, sizeof(PINTERFACE), _sortComp );
    // Store new sort type to the ini-file
    piniWriteULong( hDSModule, "Sort", stProperties.ulSort );
    piniWriteULong( hDSModule, "SortDesc", stProperties.fSortDesc );
  }

  ulResult = stProperties.ulSort;
  if ( stProperties.fSortDesc )
    ulResult |= DSSORT_FL_DESCENDING;

  return ulResult;
}

DSEXPORT VOID APIENTRY dsGetHintText(ULONG ulIndex, PCHAR pcBuf, ULONG cbBuf)
{
  ULONG		ulIdx;
  CHAR		acBuf[16];
  LONG		cbLine;
  ULONG		ulMask;

  // List all IP addresses for given interface.
  for( ulIdx = 0; ulIdx < pstStatAT->cAddr; ulIdx++ )
  {
    if ( pstStatAT->aAddr[ulIdx].usIFIdx !=
         apInterfaces[ulIndex]->stIFT.iftIndex )
      continue;

    ulMask = ntohl( pstStatAT->aAddr[ulIdx].ulMask );
    strlcpy( &acBuf, inet_ntoa( *((struct in_addr *)&ulMask) ), sizeof(acBuf) );
    cbLine = _snprintf( pcBuf, cbBuf, "%s/%s\n",
               inet_ntoa( *((struct in_addr *)&pstStatAT->aAddr[ulIdx].ulIP) ),
               &acBuf );
    if ( cbLine <= 0 )
      break;
    pcBuf += cbLine;
    cbBuf -= cbLine;
  }
}

DSEXPORT VOID APIENTRY dsSetWndStart(HPS hps, PSIZEL pSize)
{
  FONTMETRICS		stFontMetrics;
  SIZEL			sizeText;

  // Calculate window elements's position/sizes with current font.

  if ( GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &stFontMetrics ) )
  {
    ulCharSpace = stFontMetrics.lAveCharWidth;
    ulLPad = stFontMetrics.lAveCharWidth;
    ulRPad = 2 * stFontMetrics.lAveCharWidth;
    ulLineHeight = stFontMetrics.lMaxBaselineExt;
    ulLedWidth = ulCharSpace * 1.5;
    ulLedHeight = ulCharHeight / 2;
  }

  putilGetTextSize( hps, 47, "Lower layer down, Ethernet-Csmacd 100 Mbit/sec.",
                    &sizeText );
  ulInfWidth = sizeText.cx;
  putilGetTextSize( hps, 4, "9999", &sizeText );
  ulIdxWidth = sizeText.cx;
  putilGetTextSize( hps, 4, "RX: ", &sizeText );
  ulXLabWidth = sizeText.cx;
  ulCharHeight = sizeText.cy;
  ulTPad = ulCharHeight - ( ulLineHeight - ulCharHeight );
  ulBPad = ulCharHeight;
  ulSpdLOffs = ulInfWidth >> 1;

  // Set size for windows of items.
  pSize->cx = ulIdxWidth + ulLPad + ulInfWidth + ulRPad;
  pSize->cy = ulTPad + ( ulLineHeight * 4 ) + ulBPad;
}

DSEXPORT VOID APIENTRY dsPaintItem(ULONG ulIndex, HPS hps, PSIZEL psizeWnd)
{
  PINTERFACE	pIF = apInterfaces[ulIndex];
  CHAR		szBuf[64];
  SIZEL		size;
  POINTL	pt;
  ULONG		cbBuf;
  ULONG		ulSpeed;
  SIZEL		sizeText;
  BOOL		fSpeed;

  // Index
  cbBuf = sprintf( &szBuf, "%u", pIF->stIFT.iftIndex );
  putilGetTextSize( hps, cbBuf, &szBuf, &size );
  pt.x = ( ulIdxWidth - size.cx ) / 2;
  pt.y = ( psizeWnd->cy - size.cy ) / 2;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );

  pt.x = ulIdxWidth + ulLPad;
  pt.y = ulBPad + ulLineHeight * 3;

  // OperStatus
  if ( pIF->stIFT.iftOperStatus > 0 && pIF->stIFT.iftOperStatus <= 7 )
     cbBuf = sprintf( &szBuf, "%s, ",
                      apszOperStatus[pIF->stIFT.iftOperStatus - 1] );
  else
    cbBuf = 0;

  // Description
  cbBuf += _snprintf( &szBuf[cbBuf], sizeof(szBuf) - cbBuf,
                      &pIF->stIFT.iftDescr );
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );

  // Interface speed
  szBuf[0] = ' ';
  cbBuf = 1 + pstrFromBits( pIF->stIFT.iftSpeed, sizeof(szBuf) - 1,
                                       &szBuf[1], TRUE );
  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );
  pt.x = ulIdxWidth + ulLPad + ulInfWidth - sizeText.cx;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );

  // MAC address
  cbBuf = sprintf( &szBuf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", 
                           pIF->stIFT.iftPhysAddr[0],
                           pIF->stIFT.iftPhysAddr[1],
                           pIF->stIFT.iftPhysAddr[2],
                           pIF->stIFT.iftPhysAddr[3],
                           pIF->stIFT.iftPhysAddr[4],
                           pIF->stIFT.iftPhysAddr[5] );
  pt.x = ulIdxWidth + ulLPad;
  pt.y = ulBPad + ulLineHeight * 2;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );

  // MTU
  cbBuf = sprintf( &szBuf, "MTU: %u", pIF->stIFT.iftMtu );
  putilGetTextSize( hps, cbBuf, &szBuf, &sizeText );
  pt.x = ulIdxWidth + ulLPad + ulInfWidth - sizeText.cx;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );

  // RX

  fSpeed = pgrGetValue( &stGraph, &pIF->stGVRecv, &ulSpeed );
  // Led
  pt.x = ulIdxWidth + ulLPad;
  pt.y = ulBPad + ulLineHeight;
  _drawLed( hps, pt, aGrValParam[0].clrGraph, ulSpeed > 0 );
  // Space
  GpiQueryCurrentPosition( hps, &pt );
  pt.x += 2 * ulCharSpace;
  // Counter
  GpiCharStringAt( hps, &pt, 3, "RX:" );
  cbBuf = pstrFromBytes( pIF->stIFT.iftInOctets, sizeof(szBuf),
                                    &szBuf, FALSE );
  pt.x += ulXLabWidth;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );
  // Speed
  if ( fSpeed )
  {
    pt.x = ulIdxWidth + ulLPad + ulSpdLOffs;
    cbBuf = pstrFromBytes( ulSpeed, sizeof(szBuf), &szBuf, TRUE );
    GpiCharStringAt( hps, &pt, cbBuf, &szBuf );
  }

  // TX

  pgrGetValue( &stGraph, &pIF->stGVSent, &ulSpeed );
  // Led
  pt.x = ulIdxWidth + ulLPad;
  pt.y = ulBPad;
  _drawLed( hps, pt, aGrValParam[1].clrGraph, ulSpeed > 0 );
  // Space
  GpiQueryCurrentPosition( hps, &pt );
  pt.x += 2 * ulCharSpace;
  // Counter
  GpiCharStringAt( hps, &pt, 3, "TX:" );
  cbBuf = pstrFromBytes( pIF->stIFT.iftOutOctets, sizeof(szBuf),
                                    &szBuf, FALSE );
  pt.x += ulXLabWidth;
  GpiCharStringAt( hps, &pt, cbBuf, &szBuf );
  // Speed
  if ( fSpeed )
  {
    pt.x = ulIdxWidth + ulLPad + ulSpdLOffs;
    cbBuf = pstrFromBytes( ulSpeed, sizeof(szBuf), &szBuf, TRUE );
    GpiCharStringAt( hps, &pt, cbBuf, &szBuf );
  }
}

DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  PINTERFACE		pIF = _getSelInterface();
  CHAR			szAbscissaBottomCaption[64];
  PGRVAL		apGrVal[2];
  RECTL			rclGraph;
  SIZEL			sizeText;

  if ( pIF != NULL )
  {
    stGrParam.pszAbscissaTopCaption = &pIF->stIFT.iftDescr;

    if ( pstrFromSec( stProperties.ulTimeWindow,
                      sizeof(szAbscissaBottomCaption),
                      &szAbscissaBottomCaption ) > 0 )
      stGrParam.pszAbscissaBottomCaption = &szAbscissaBottomCaption;
    else
      stGrParam.pszAbscissaBottomCaption = NULL;

    putilGetTextSize( hps, 15, "9999 bytes/sec.", &sizeText );
    rclGraph.xLeft = sizeText.cx + GRAPG_ORDINATE_LEFT_PAD;
    rclGraph.xRight = psizeWnd->cx - 30;
    rclGraph.yTop = psizeWnd->cy - sizeText.cy - GRAPG_ABSCISSA_TOP_PAD - 5;
    rclGraph.yBottom = sizeText.cy + GRAPG_ABSCISSA_BOTTOM_PAD + 5;
    apGrVal[0] = &pIF->stGVRecv;
    apGrVal[1] = &pIF->stGVSent;
    pgrDraw( &stGraph, hps, &rclGraph, 2, &apGrVal, &stGrParam );
  }
}

