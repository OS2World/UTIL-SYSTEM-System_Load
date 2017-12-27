#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_DOSERRORS    /* DOS error values */
#define INCL_DOSMISC
#include "utils.h"
#include "sl.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "debug.h"

extern HAB			hab;	// sl.c

BOOL utilGetTextSize(HPS hps, ULONG cbText, PSZ pszText, PSIZEL pSize)
{
  FONTMETRICS		stFontMetrics;
  RECTL			rctlText;
  LONG			lRC;
  GRADIENTL		gradlAngle = { 0, 0 };
  POINTL		aptText[TXTBOX_COUNT];

  if ( GpiQueryCharAngle( hps, &gradlAngle ) && ( gradlAngle.y != 0 ) &&
       GpiQueryTextBox( hps, cbText, pszText, TXTBOX_COUNT, &aptText ) )
  {
    LONG     lMaxX = aptText[0].x, lMinX = aptText[0].x;
    LONG     lMaxY = aptText[0].y, lMinY = aptText[0].y;
    ULONG    ulIdx;

    for( ulIdx = 1; ulIdx < TXTBOX_COUNT - 1; ulIdx++ )
    {
      if ( aptText[ulIdx].x > lMaxX )
        lMaxX = aptText[ulIdx].x;
      if ( aptText[ulIdx].x < lMinX )
        lMinX = aptText[ulIdx].x;

      if ( aptText[ulIdx].y > lMaxY )
        lMaxY = aptText[ulIdx].y;

      if ( aptText[ulIdx].y < lMinY )
        lMinY = aptText[ulIdx].y;
    }

    pSize->cx = lMaxX - lMinX;
    pSize->cy = lMaxY - lMinY;
    return TRUE;
  }

  if ( !GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &stFontMetrics ) )
    return FALSE;

// Not work for TTF?
//  pSize->cy = stFontMetrics.lMaxBaselineExt + stFontMetrics.lExternalLeading;

  pSize->cy = stFontMetrics.lEmHeight // - stFontMetrics.lExternalLeading
              - stFontMetrics.lInternalLeading;

  rctlText.yTop = pSize->cy + 1;
  rctlText.yBottom = 0;
  rctlText.xLeft = 0;
  rctlText.xRight = 16384;
  lRC = WinDrawText( hps, cbText, pszText, &rctlText, 0, 0,
                     DT_LEFT | DT_TOP | DT_QUERYEXTENT );
  pSize->cx = rctlText.xRight - rctlText.xLeft;

  return lRC != 0;
}

#if 0
BOOL utilGetTextSize(HPS hps, ULONG cbText, PSZ pszText, PSIZEL pSize)
{
  POINTL		aptText[TXTBOX_COUNT];
  LONG			lMinX, lMaxX, lMinY, lMaxY;
  ULONG			ulIdx;
  FONTMETRICS		stFontMetrics;
  GRADIENTL		gradlAngle = { 0, 0 };
  POINTL		ptAngle = { 0, 0 };
  LONG			lDir;
  
  if ( !GpiQueryTextBox( hps, cbText, pszText, TXTBOX_COUNT, &aptText ) )
    return FALSE;

  GpiQueryCharAngle( hps, &gradlAngle );
  GpiSetCharShear( hps, &ptAngle );
  lDir = GpiQueryCharDirection( hps );
  if ( gradlAngle.y == 0 && gradlAngle.x >= 0 &&
       ( lDir == CHDIRN_DEFAULT || lDir == CHDIRN_LEFTRIGHT ) &&
       ptAngle.x == 0 && ptAngle.y >= 0 )
  {
    GpiQueryFontMetrics( hps, sizeof(FONTMETRICS), &stFontMetrics );
    pSize->cx = aptText[TXTBOX_BOTTOMRIGHT].x - aptText[TXTBOX_BOTTOMLEFT].x;
    pSize->cy = stFontMetrics.lEmHeight // - stFontMetrics.lExternalLeading
                - stFontMetrics.lInternalLeading;
    return TRUE;
  }

  lMaxX = lMinX = aptText[0].x;
  lMaxY = lMinY = aptText[0].y;
  for( ulIdx = 1; ulIdx < TXTBOX_COUNT - 1; ulIdx++ )
  {
    if ( aptText[ulIdx].x > lMaxX )
      lMaxX = aptText[ulIdx].x;
    if ( aptText[ulIdx].x < lMinX )
      lMinX = aptText[ulIdx].x;

    if ( aptText[ulIdx].y > lMaxY )
      lMaxY = aptText[ulIdx].y;

    if ( aptText[ulIdx].y < lMinY )
      lMinY = aptText[ulIdx].y;
  }

  pSize->cx = lMaxX - lMinX;
  pSize->cy = lMaxY - lMinY;
  return TRUE;
}
#endif

VOID util3DFrame(HPS hps, PRECTL pRect, LONG lLTColor, LONG lRBColor)
{
  POINTL	pt;
  // Store current color to lColor,
  LONG		lSaveColor = GpiQueryColor( hps );

  GpiSetColor( hps, lLTColor );
  GpiMove( hps, (PPOINTL)pRect );
  pt.x = pRect->xLeft;
  pt.y = pRect->yTop - 1;
  GpiLine( hps, &pt );
  pt.x = pRect->xRight - 1;
  pt.y = pRect->yTop - 1;
  GpiLine( hps, &pt );
  GpiSetColor( hps, lRBColor );
  pt.x = pRect->xRight - 1;
  pt.y = pRect->yBottom;
  GpiLine( hps, &pt );
  GpiLine( hps, (PPOINTL)pRect );

  // Restore color.
  GpiSetColor( hps, lSaveColor );
}

VOID utilBox(HPS hps, PRECTL pRect, LONG lColor)
{
  LONG		lSaveColor = GpiQueryColor( hps );
  POINTL	pt;

  GpiSetColor( hps, lColor );
  GpiMove( hps, (PPOINTL)pRect );
  pt.x = pRect->xRight - 1;
  pt.y = pRect->yTop - 1;
  GpiBox( hps, DRO_FILL, &pt, 0, 0 );
  GpiSetColor( hps, lSaveColor );
}

BOOL utilWriteResStr(HPS hps, HMODULE hMod, ULONG ulId,
                     ULONG cVal, PSZ *ppszVal)
{
  CHAR		szTemp[255];
  CHAR		szBuf[255];
  ULONG		ulLen;
  ULONG		ulRC;
  PSZ		pszBuf;

  ulLen = WinLoadString( hab, hMod, ulId, sizeof(szTemp), &szTemp );
  if ( ulLen == 0 )
    return FALSE;

  if ( cVal != 0 )
  {
    ulRC = DosInsertMessage( ppszVal, cVal, szTemp, ulLen, &szBuf,
                             sizeof(szBuf), &ulLen );
    if ( ulRC != NO_ERROR )
    {
      debug( "DosInsertMessage(), rc = %u", ulRC );
      return FALSE;
    }
    pszBuf = &szBuf;
  }
  else
    pszBuf = &szTemp;

  return GpiCharString( hps, ulLen, pszBuf ) != GPI_ERROR;
}

BOOL utilWriteResStrAt(HPS hps, PPOINTL pptPos, HMODULE hMod, ULONG ulId,
                       ULONG cVal, PSZ *ppszVal)
{
  GpiMove( hps, pptPos );

  return utilWriteResStr( hps, hMod, ulId, cVal, ppszVal );
}


// VOID utilCharStringRect(HPS hps, PRECTL pRect, ULONG cbBuf, PCHAR pcBuf,
//                         ULONG ulFlags)
// 
// Draws a character string starting at pRect.xLeft, pRect.yBottom or
// pRect.xLeft and vertical center if flag SL_CSR_VCENTER specified.
// Displays as many characters as will fit on width of rectangle pRect.
// Not the full string will end with three dots if flag SL_CSR_CUTDOTS set or
// gradual shading if flag SL_CSR_CUTDOTS not specified.

VOID utilCharStringRect(HPS hps,		// Presentation space handle.
                        PRECTL pRect,		// Rrectangle for drawing.
                        ULONG cbBuf,		// Length of string.
                        PCHAR pcBuf,		// Pointer to the string.
                        ULONG ulFlags)		// Flags SL_CSR_*.
{
  SIZEL		sizeText;
  SIZEL		sizeDots;
  POINTL	pt;

  utilGetTextSize( hps, cbBuf, pcBuf, &sizeText );

  pt.x = pRect->xLeft;
  if ( (ulFlags & SL_CSR_VCENTER) != 0 )
    pt.y = pRect->yBottom +
           ( (pRect->yTop - pRect->yBottom) - sizeText.cy ) / 2;
  else
    pt.y = pRect->yBottom;

  if ( ( pRect->xLeft + sizeText.cx ) < pRect->xRight )
  {
    GpiCharStringAt( hps, &pt, cbBuf, pcBuf );
    return;
  }

  if ( (ulFlags & SL_CSR_CUTDOTS) != 0 )
  {
    utilGetTextSize( hps, 3, "...", &sizeDots );

    if ( (pt.x + sizeDots.cx) < pRect->xRight )
    {
      PCHAR		pCh = pcBuf;

      GpiMove( hps, &pt );
      while( (cbBuf--) > 0 )
      {
        GpiQueryCurrentPosition( hps, &pt );
        utilGetTextSize( hps, 1, pCh, &sizeText );
        if ( (pt.x + sizeText.cx + sizeDots.cx) >= pRect->xRight )
        {
          GpiCharString( hps, 3, "..." );
          break;
        }
        GpiCharString( hps, 1, pCh );
        pCh++;
      }
    }
  }
  else	// SL_CSR_CUTFADE
  {
    PCHAR		pCh = pcBuf;
    BOOL		fFade = FALSE;
    LONG		lColor = GpiQueryColor( hps );
    LONG		lBgColor = GpiQueryBackColor( hps );
    ULONG		ulMix = 73;

    GpiMove( hps, &pt );
    while( cbBuf > 0 )
    {
      GpiQueryCurrentPosition( hps, &pt );
      if ( !fFade )
      {
        utilGetTextSize( hps, min( cbBuf, 4 ), pCh, &sizeText );
        fFade = (pt.x + sizeText.cx) >= pRect->xRight;
      }

      if ( fFade )
      {
        utilGetTextSize( hps, 1, pCh, &sizeText );

        if ( (pt.x + sizeText.cx) >= pRect->xRight )
          break;
        
        GpiSetColor( hps, utilMixRGB( lColor, lBgColor, ulMix ) );
        ulMix += 45;
      }
        
      GpiCharString( hps, 1, pCh );
      pCh++;
      cbBuf--;
    }

    GpiSetColor( hps, lColor );
  }
}


// BOOL utilSetFontFromPS(HPS hpsDst, HPS hpsSrc, LONG llcid)
//
// Set font for hpsDst same as font of hpsSrc.
// llcid  - Local identifier. Refer to GPI Guide and Reference:
//          GpiCreateLogFont() ( or simply use 1 :) ).

BOOL utilSetFontFromPS(HPS hpsDst, HPS hpsSrc, LONG llcid)
{
  FATTRS		stFAttrs;
  SIZEF			sizeCharBox; 
  FONTMETRICS		stFontMetrics;
  BOOL			fError;

  if ( llcid <= 0 ||
       !GpiQueryFontMetrics( hpsSrc, sizeof(FONTMETRICS), &stFontMetrics ) )
    return FALSE;

  stFAttrs.usRecordLength = sizeof(FATTRS);
  stFAttrs.fsSelection = stFontMetrics.fsSelection;
  stFAttrs.lMatch = stFontMetrics.lMatch;
  strcpy( &stFAttrs.szFacename, &stFontMetrics.szFacename );
  stFAttrs.idRegistry = 0;//stFontMetrics.idRegistry;
  stFAttrs.usCodePage = 0;//stFontMetrics.usCodePage;
  stFAttrs.lMaxBaselineExt = 0;//stFontMetrics.lMaxBaselineExt;
  stFAttrs.lAveCharWidth = 0;//stFontMetrics.lAveCharWidth;
  stFAttrs.fsType = 0;//FATTR_TYPE_KERNING;// | FATTR_TYPE_ANTIALIASED;//stFontMetrics.fsType;
  //->   stFAttrs.fsFontUse = FATTR_FONTUSE_OUTLINE;
  //stFAttrs.fsType = stFontMetrics.fsType;
  stFAttrs.fsFontUse = 0;//FATTR_FONTUSE_TRANSFORMABLE | FATTR_FONTUSE_NOMIX;
// FATTR_FONTUSE_OUTLINE FATTR_FONTUSE_TRANSFORMABLE

  GpiSetCharSet( hpsDst, LCID_DEFAULT );
  fError = GpiCreateLogFont( hpsDst, NULL, llcid, &stFAttrs ) == GPI_ERROR;
  if ( fError )
    debug( "GpiCreateLogFont() fail" );
  GpiSetCharSet( hpsDst, llcid );

  GpiQueryCharBox( hpsSrc, &sizeCharBox );
  fError = !GpiSetCharBox( hpsDst, &sizeCharBox );
  if ( fError )
    debug( "GpiSetCharBox() fail" );

  return fError;
}

BOOL utilWriteProfileLong(HINI hIni, PSZ pszApp, PSZ pszKey, LONG lData)
{
  CHAR			szData[12];

  return hIni == NULLHANDLE ? FALSE : 
         PrfWriteProfileString( hIni, pszApp, pszKey,
                                ltoa( lData,  &szData, 10 ) );
}

BOOL utilWriteProfileULong(HINI hIni, PSZ pszApp, PSZ pszKey, ULONG ulData)
{
  CHAR			szData[12];

  return hIni == NULLHANDLE ? FALSE : 
         PrfWriteProfileString( hIni, pszApp, pszKey,
                                ultoa( ulData,  &szData, 10 ) );
}

BOOL utilWriteProfileStr(HINI hIni, PSZ pszApp, PSZ pszKey, PSZ pszData)
{
  return hIni == NULLHANDLE ? FALSE : 
         PrfWriteProfileString( hIni, pszApp, pszKey, pszData );
}

BOOL utilWriteProfileData(HINI hIni, PSZ pszApp, PSZ pszKey, PVOID pBuf,
                          ULONG cbBuf)
{
  return hIni == NULLHANDLE ? FALSE : 
         PrfWriteProfileData( hIni, pszApp, pszKey, pBuf, cbBuf );
}

LONG utilQueryProfileLong(HINI hIni, PSZ pszApp, PSZ pszKey, LONG lDefault)
{
  CHAR			szData[12];
  LONG			lData;
  PCHAR			pCh;

  if ( hIni == NULLHANDLE ||
       PrfQueryProfileString( hIni, pszApp, pszKey, NULL,
                              &szData, sizeof(szData) ) == 0 )
    return lDefault;

  lData = strtol( &szData, &pCh, 10 ); 
  return pCh == &szData ? lDefault : lData;
}

ULONG utilQueryProfileULong(HINI hIni, PSZ pszApp, PSZ pszKey, ULONG ulDefault)
{
  CHAR			szData[12];
  ULONG			ulData;
  PCHAR			pCh;

  if ( hIni == NULLHANDLE ||
       PrfQueryProfileString( hIni, pszApp, pszKey, NULL,
                              &szData, sizeof(szData) ) == 0 )
    return ulDefault;

  ulData = strtoul( &szData, &pCh, 10 ); 
  return pCh == &szData ? ulDefault : ulData;
}

ULONG utilQueryProfileStr(HINI hIni, PSZ pszApp, PSZ pszKey, PSZ pszDefault,
                          PCHAR pcBuf, ULONG cbBuf)
{
  ULONG		cbResult = 0;

  if ( hIni != NULLHANDLE )
    cbResult = PrfQueryProfileString( hIni, pszApp, pszKey, pszDefault,
                                      pcBuf, cbBuf );

  if ( cbResult == 0 )
  {
    if ( pszDefault == NULL )
      pcBuf[0] = '\0';
    else
      cbResult = strlcpy( pcBuf, pszDefault, cbBuf );
  }

  return cbResult;
}

BOOL utilQueryProfileData(HINI hIni, PSZ pszApp, PSZ pszKey, PVOID pBuf,
                          PULONG pcbBuf)
{
  return hIni == NULLHANDLE ? FALSE :
         PrfQueryProfileData( hIni, pszApp, pszKey, pBuf, pcbBuf );
}

BOOL utilQueryProfileSize(HINI hIni, PSZ pszApp, PSZ pszKey, PULONG pcbData)
{
  return hIni == NULLHANDLE ? FALSE :
         PrfQueryProfileSize( hIni, pszApp, pszKey, pcbData );
}


// LONG utilMixRGB(LONG lColor1, LONG lColor2, ULONG ulMix)
// Mix RGB lColor1 and RGB lColor1 for a given coefficient ulMix (0..255).
// Returns result RGB color.

LONG utilMixRGB(LONG lColor1, LONG lColor2, ULONG ulMix)
{
  RGB2		rgb1 = *(PRGB2)&lColor1;
  RGB2		rgb2 = *(PRGB2)&lColor2;

  if ( ulMix > 255 )
    ulMix = 255;

  rgb1.bBlue += ( ( (LONG)rgb2.bBlue - (LONG)rgb1.bBlue ) * ulMix / 255L );
  rgb1.bGreen += ( ( (LONG)rgb2.bGreen - (LONG)rgb1.bGreen ) * ulMix / 255L );
  rgb1.bRed += ( ( (LONG)rgb2.bRed - (LONG)rgb1.bRed ) * ulMix / 255L );

  return *(PLONG)&rgb1;
}


// ULONG utilLoadInsertStr(HMODULE hMod, BOOL fStrMsg, ULONG ulId,
//                         ULONG cVal, PSZ *ppszVal, ULONG cbBuf, PCHAR pcBuf)
//
// Loads string ulId from resource ( string table or message table ) and
// inserts variable text-string information from the array ppszVal.
// pcBuf - The address where function returns the requested string.
// cbBuf - The length, in bytes, of the buffer.
// Returns actual length, in bytes, of the string in pcBuf or 0 on error.

ULONG utilLoadInsertStr(HMODULE hMod,		// module handle
                        BOOL fStrMsg, 		// string (1) / message (0)
                        ULONG ulId,		// string/message id
                        ULONG cVal, PSZ *ppszVal,// count and pointers to values
                        ULONG cbBuf, PCHAR pcBuf)// result buffer
{
  ULONG		ulLen;
  PCHAR		pcTemp = alloca( cbBuf );
  BOOL		fHeapUsed;
  ULONG		ulRC;

  if ( pcTemp == NULL )
  {
    pcTemp = debugMAlloc( cbBuf );
    if ( pcTemp == NULL )
    {
      debug( "Not enough memory" );
      return 0;
    }
    fHeapUsed = TRUE;
  }
  else
    fHeapUsed = FALSE;

  if ( fStrMsg )
    ulLen = WinLoadString( hab, hMod, ulId, cbBuf, pcTemp );
  else
    ulLen = WinLoadMessage( hab, hMod, ulId, cbBuf, pcTemp );

  ulRC = DosInsertMessage( ppszVal, cVal, pcTemp, ulLen, pcBuf, cbBuf - 1, &ulLen );
  if ( ulRC != NO_ERROR )
    debug( "DosInsertMessage(), rc = %u", ulRC );
  else
    pcBuf[ulLen] = '\0';

  if ( fHeapUsed )
    debugFree( pcTemp );

  return ulRC == NO_ERROR ? ulLen : 0;
}


// BOOL utilTestFont(PSZ pszFont)
// Returns TRUE if given font pszFont allowed.

BOOL utilTestFont(PSZ pszFont)
{
  BOOL		fPresent;
  LONG		lCnt = 0;
  HPS		hps = WinGetPS( HWND_DESKTOP );

  fPresent = GpiQueryFonts( hps, QF_PUBLIC, pszFont, &lCnt, 0, NULL ) > 0;
  WinReleasePS( hps );

  return fPresent;
}

ULONG utilQueryProgPath(ULONG cbBuf, PCHAR pcBuf)
{
  PTIB		pTIB;
  PPIB		pPIB;
  PCHAR		pCh;
  ULONG		ulRC;
  ULONG		cbPath;

  ulRC = DosGetInfoBlocks( &pTIB, &pPIB );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosGetInfoBlocks(), rc = %u", ulRC );
    return 0;
  }

  pCh = strrchr( pPIB->pib_pchcmd, '\\' );
  if ( pCh == NULL || pCh == pPIB->pib_pchcmd )
  {
    debug( "Warning! Cannot get path from %s", pPIB->pib_pchcmd );
    if ( cbBuf < 3 )
      return 0;
    strcpy( pcBuf, ".\\" );
    return 2;
  }

  cbPath = pCh - pPIB->pib_pchcmd + 1;
  if ( cbBuf <= cbPath )
    return 0;

  memcpy( pcBuf, pPIB->pib_pchcmd, cbPath );
  pcBuf[cbPath] = '\0';
  return cbPath;
}


// Strings

// LONG strFromSec(ULONG ulVal, ULONG cbBuf, PCHAR pcBuf)
//
// Builds string like "NN h. NN min. NN sec." from value ulVal in seconds.
// Returns actual lenght of string.

LONG strFromSec(ULONG ulVal, ULONG cbBuf, PCHAR pcBuf)
{
  LONG		lLen;
  LONG		cbSave = cbBuf;

  if ( ulVal >= 60 * 60 )
  {
    lLen = _snprintf( pcBuf, cbBuf, "%u ", ulVal / (60 * 60) );
    if ( lLen < 0 )
      return -1;
    pcBuf += lLen;
    cbBuf -= lLen;
    strLoad2( 0, IDS_HOUR_SHORT, &cbBuf, &pcBuf );
    ulVal = ulVal % (60 * 60);

    if ( ulVal != 0 )
    {
      if ( lLen < 2 )
        return -1;
      *(pcBuf++) = ' ';
      cbBuf--;
    }
  }

  if ( ulVal >= 60 )
  {
    lLen = _snprintf( pcBuf, cbBuf, "%u ", ulVal / 60 );
    if ( lLen < 0 )
      return -1;
    pcBuf += lLen;
    cbBuf -= lLen;
    strLoad2( 0, IDS_MIN_SHORT, &cbBuf, &pcBuf );
    ulVal = ulVal % 60;

    if ( ulVal != 0 )
    {
      if ( lLen < 2 )
        return -1;
      *(pcBuf++) = ' ';
      cbBuf--;
    }
  }

  if ( ulVal > 0 )
  {
    lLen = _snprintf( pcBuf, cbBuf, "%u ", ulVal );
    if ( lLen < 0 )
      return -1;
    pcBuf += lLen;
    cbBuf -= lLen;
    if ( !strLoad2( 0, IDS_SEC_SHORT, &cbBuf, &pcBuf ) )
      return -1;
  }

  return cbSave - cbBuf;
}

LONG strFromBits(ULONG ulVal, ULONG cbBuf, PCHAR pcBuf, BOOL fSlashSec)
{
  ULONG		ulEuId;
  CHAR		acBuf[16] = { 0 };
  LONG		cbVal;
  LONG		cbRes = 0;

  if ( cbBuf < 6 )
    return -1;

  if ( ulVal >= 1000000000 )
  {
    ulVal /= 1000000;
    ulEuId = IDS_GBIT;
  }
  else if ( ulVal >= 1000000 )
  {
    ulVal /= 1000;
    ulEuId = IDS_MBIT;
  }
  else if ( ulVal >= 1000 )
    ulEuId = IDS_KBIT;
  else
  {
    cbVal = _snprintf( pcBuf, cbBuf, "%u ", ulVal );
    if ( cbVal <= 0 )
      return -1;
    pcBuf += cbVal;
    cbBuf -= cbVal;
    cbRes += cbVal;
    ulEuId = IDS_BITS;
    goto l01;
  }

  cbVal = sprintf( &acBuf, "%u", ulVal );

  if ( cbVal == 6 )		// NNN
  {
    *((PULONG)pcBuf) = *((PULONG)&acBuf);
    cbVal = 3;
  }
  else if ( cbVal == 5 )	// NN.N
  {
    *((PUSHORT)pcBuf) = *((PUSHORT)&acBuf);
    pcBuf[2] = '\.';
    pcBuf[3] = acBuf[2];
    cbVal = pcBuf[3] == '0' ? 2 : 4;
  }
  else // cbVal == 4		N.NN
  {
    pcBuf[0] = acBuf[0];
    pcBuf[1] = '\.';
    *((PUSHORT)&pcBuf[2]) = *((PUSHORT)&acBuf[1]);
    cbVal = pcBuf[3] == '0' ? 
            ( pcBuf[2] == '0' ? 1 : 3 ) : 4;
  }

  pcBuf += cbVal;
  cbBuf -= cbVal;
  cbRes += cbVal;

  if ( cbBuf <= 1 )
    return -1;
  *(pcBuf++) = ' ';
  cbBuf--;
  cbRes++;

l01:
  cbVal = strLoad2( 0, ulEuId, &cbBuf, &pcBuf );
  if ( cbVal == 0 )
    return -1;
  cbRes += cbVal;

  if ( fSlashSec )
  {
    if ( cbBuf <= 1 )
      return -1;
    *(pcBuf++) = '/';
    cbBuf--;
    cbRes++;
    cbVal = strLoad( 0, IDS_SEC_SHORT, cbBuf, pcBuf );
    if ( cbVal == 0 )
      return -1;
    cbRes += cbVal;
  }

  return cbRes;
}

LONG strFromBytes(ULLONG ullVal, ULONG cbBuf, PCHAR pcBuf, BOOL fSlashSec)
{
  ULONG		ulVal = ullVal;
  ldiv_t	stDiv;
  ULONG		ulEuId;
  LONG		cbVal;
  LONG		cbRes;

  if ( ulVal < 1024 )
  {
    cbVal = _snprintf( pcBuf, cbBuf, "%u ", (ULONG)ullVal );
    ulEuId = IDS_BYTES;
  }
  else
  {
    if ( ullVal >= (1024ULL * 1024ULL * 1024ULL * 1024ULL) )
    {
      ulVal = ullVal / (1024 * 1024 * 1024);
      ulEuId = IDS_TB;
    }
    else if ( ullVal >= (1024 * 1024 * 1024) )
    {
      ulVal = ullVal / (1024 * 1024);
      ulEuId = IDS_GB;
    }
    else if ( ullVal >= (1024 * 1024) )
    {
      ulVal = ullVal / 1024;
      ulEuId = IDS_MB;
    }
    else
      ulEuId = IDS_KB;

    stDiv = ldiv( ulVal, 1024 ); 

    if ( ulVal > 102400 || stDiv.rem == 0 )
      // NNN / NN / N
      cbVal = _snprintf( pcBuf, cbBuf, "%u ", stDiv.quot );
    else
      // N.NN / NN.N
      cbVal = _snprintf( pcBuf, cbBuf,
                         ulVal < 10240 ? "%.2f " : "%.1f ",
                         (float)ulVal / 1024 );
  }

  if ( cbVal <= 0 )
    return -1;

  pcBuf += cbVal;
  cbBuf -= cbVal;
  cbRes = cbVal + strLoad2( 0, ulEuId, &cbBuf, &pcBuf );

  if ( fSlashSec )
  {
    if ( cbBuf <= 1 )
      return -1;
    *(pcBuf++) = '/';
    cbBuf--;
    cbRes++;
    cbVal = strLoad( 0, IDS_SEC_SHORT, cbBuf, pcBuf );
    if ( cbVal == 0 )
      return -1;
    cbRes += cbVal;
  }

  return cbRes;
}

ULONG strLoad(HMODULE hModule, ULONG ulStrId, ULONG cbBuf, PCHAR pcBuf)
{
  LONG		lLen;

  if ( cbBuf == 0 )
    return 0;

  lLen = WinLoadString( hab, hModule, ulStrId, cbBuf, pcBuf );
  if ( lLen == 0 )
    _bprintf( pcBuf, cbBuf, "<string #%u not loaded>", ulStrId );

  return lLen;
}

ULONG strLoad2(HMODULE hModule, ULONG ulStrId, PULONG pcbBuf, PCHAR *ppcBuf)
{
  LONG		lLen = strLoad( hModule, ulStrId, *pcbBuf, *ppcBuf );

  *pcbBuf -= lLen;
  *ppcBuf += lLen;
  return lLen;
}

ULONG strFromULL(PCHAR pcBuf, ULONG cbBuf, ULLONG ullVal)
{
  PCHAR		pCh;
  ULONG		ulLen = 1;
  LONG		cbStr = _snprintf( pcBuf, cbBuf, "%llu", ullVal );

  if ( cbStr <= 0 )
    return 0;
  pCh = &pcBuf[cbStr];

  cbStr += ( cbStr - 1 ) / 3;
  if ( (cbStr + 1) >= cbBuf )
    return 0;

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

  return cbStr;
}

// VOID strRemoveMnemonic(ULONG cbBuf, PCHAR pcBuf, PSZ pszText)
//
// Copies to buffer pcBuf up to cbBuf-1 characters from pszText and ZERO
// Character '~' will be removed.

VOID strRemoveMnemonic(ULONG cbBuf, PCHAR pcBuf, PSZ pszText)
{
  PCHAR		pCh;

  if ( pszText == NULL )
  {
    if ( cbBuf != 0 )
      pcBuf[0] = '\0';
    return;
  }

  strlcpy( pcBuf, pszText, cbBuf );
  // Remove tilde character
  pCh = strchr( pcBuf, '~' );
  if ( pCh != NULL )
    strcpy( pCh, &pCh[1] );
}


/*
// Memory

// PVOID utilMemAlloc(ULONG ulSize)
//
// Tries to allocate object in the high memory. Object will be allocated in
// the first 512Mb of virtual-address space if high memory is not available.

PVOID utilMemAlloc(ULONG ulSize)
{
  PVOID		pMem;
  ULONG		ulRC;
  static ULONG	ulFlags = PAG_READ | PAG_WRITE | PAG_COMMIT | OBJ_ANY;

#ifdef DEBUG_FILE
  static BOOL	fHiMemDbgMessageWrited = FALSE;
  // Use first four bytes to store size of object.
  ulSize += sizeof(ULONG);
#endif

  do
  {
    // First time: trying to place at high memory.
    ulRC = DosAllocMem( &pMem, ulSize, ulFlags );
    if ( ulRC == NO_ERROR )
    {
      // Success.
#ifdef DEBUG_FILE
      if ( !fHiMemDbgMessageWrited )
      {
        debug( "High memory used.", UTIL_IS_HIGH_PTR(pMem) );
        fHiMemDbgMessageWrited = TRUE;
      }
#endif
      break;
    }

    if ( ulRC == ERROR_INVALID_PARAMETER )
    {
      debug( "High memory cannot be used." );

      ulRC = DosAllocMem( &pMem, ulSize, PAG_READ | PAG_WRITE | PAG_COMMIT );
      if ( ulRC == NO_ERROR )
      {
        // Object successfuly allocated in the first 512Mb of virtual-address
        // space - we will use this memory for future requests.
        ulFlags &= ~OBJ_ANY;
        break;
      }
    }

    debug( "DosAllocMem(), rc = %u", ulRC );
    return NULL;
  }
  while( FALSE );

#ifdef DEBUG_FILE
  ulSize -= sizeof(ULONG);
  debug_counter( "utilMem", ulSize );
  *(PULONG)pMem = ulSize;
  pMem = (PCHAR)pMem + sizeof(ULONG);
#endif

  return pMem;
}

// VOID utilMemFree(PVOID pMem)
//
// Frees a private or shared memory object allocated by utilMemAlloc().

VOID utilMemFree(PVOID pMem)
{
  ULONG		ulRC;

#ifdef DEBUG_FILE
  pMem = (PCHAR)pMem - sizeof(ULONG);
  debug_counter( "utilMem", -( *(PULONG)pMem ) );
#endif

  ulRC = DosFreeMem( pMem );
  if ( ulRC != NO_ERROR )
    debug( "DosFreeMem(), rc = %u", ulRC );
}
*/
