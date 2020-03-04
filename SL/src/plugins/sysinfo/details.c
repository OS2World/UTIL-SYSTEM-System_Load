#define INCL_DOSMISC
#define INCL_WIN
#include <os2.h>
#include <ds.h>
#include <sl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sysinfo.h"
#include <debug.h>     // Must be the last.

#define TITLE                    "Available memory"
#define STR_PRIVATE              "Private:"
#define STR_SHARED               "Shared:"
#define LGEND_LOW                "Low (512 Mb)"
#define COL_LOW                  0x00A0A050
#define COL_LOW_AVAIL            0x00D0E000
//#define COL_LOW_AVAIL_LIGHT      putilMixRGB( COL_LOW_AVAIL, 0x00FFFFFF, 200 )
#define COL_LOW_AVAIL_LIGHT      0x00F4F8C8
//#define COL_LOW_AVAIL_DARK       putilMixRGB( COL_LOW_AVAIL, 0x00000000, 170 )
#define COL_LOW_AVAIL_DARK       0x00464B00
#define COL_HIGH                 0x007570A0
#define COL_HIGH_AVAIL           0x00A0A0FF
//#define COL_HIGH_AVAIL_LIGHT     putilMixRGB( 0x00A0A0FF, 0x00FFFFFF, 120 )
#define COL_HIGH_AVAIL_LIGHT     0x00CCCCFF
//#define COL_HIGH_AVAIL_DARK      putilMixRGB( 0x00A0A0FF, 0x00000000, 100 )
#define COL_HIGH_AVAIL_DARK      0x0062629C
#define COL_SYSTEM               0x007080F0
#define COL_LIMIT_LIGHT          0x00FFC0C0
#define COL_LIMIT_DARK           0x00FF0000
#define TXT_VERT_SPACE           6
#define TOTAL_MEM                (4 * 1024 * 1024) // [Kb]
#define LOW_MEM                  (512 * 1024) // [Kb]
                
// Low private: 1 0000h - 1BFF 0000h
#define LIMIT_LOW_PR ( (0x1BFF0000 /* max */ - 0x10000 /* start */) / 1024 )
// Low shared: 400 0000h - 1FFF 0000h
#define LIMIT_LOW_SH ( (0x1FFF0000 - 0x4000000) / 1024 )

extern ULONG                     aulData[QSV_MAX_OS4];
extern PHutilGetTextSize         putilGetTextSize;
extern PHutil3DFrame             putil3DFrame;
extern PHutilMixRGB              putilMixRGB;
extern PHstrFromBytes            pstrFromBytes;


static VOID _vertGradBox(HPS hps, PRECTL pRect, LONG lColor1, LONG lColor2,
                         LONG lColor3)
{
  LONG       lColor = GpiQueryColor( hps );
  POINTL     pt1, pt2;
  ULONG      ulLine;
  LONG       lYMid = ( pRect->yTop - pRect->yBottom ) / 2;
  LONG       lLastLine;

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

static VOID _limitLine(HPS hps, LONG lX, LONG lY1, LONG lY2)
{
  LONG       lColor = GpiQueryColor( hps );
  POINTL     pt;

  pt.x = lX;
  pt.y = lY1 + 1;
  GpiSetColor( hps, COL_LIMIT_DARK );
  GpiMove( hps, &pt );
  pt.y = lY2 - 2;
  GpiLine( hps, &pt );

  pt.x--;
  pt.y = lY1 + 1;
  GpiSetColor( hps, COL_LIMIT_LIGHT );
  GpiMove( hps, &pt );
  pt.y = lY2 - 2;
  GpiLine( hps, &pt );
  GpiSetColor( hps, lColor );
}

static VOID _centeredText(HPS hps, LONG lX, LONG lY, ULONG cbText, PSZ pszText,
                          BOOL fTop, LONG lMinWidth)
{
  SIZEL      size;
  POINTL     pt;

  putilGetTextSize( hps, cbText, pszText, &size );
  if ( ( lMinWidth >= 0 ) && ( size.cx > lMinWidth ) )
    return;

  pt.x = lX - ( size.cx / 2 );
  pt.y = fTop ? lY : lY - size.cy;
  GpiCharStringAt( hps, &pt, cbText, pszText );
}

static VOID _centeredTextBytes(HPS hps, LONG lX, LONG lY, ULLONG ullVal,
                               BOOL fTop, ULONG ulMinWidth)
{
  CHAR       acBuf[16];
  LONG       cbBuf = pstrFromBytes( ullVal, sizeof(acBuf), &acBuf, FALSE );

  _centeredText( hps, lX, lY, cbBuf, &acBuf, fTop, ulMinWidth );
}

static VOID _labelTextBytes(HPS hps, LONG lX, LONG lY,
                            ULONG cbText, PSZ pszText, ULONG ulSpace)
{
  POINTL     pt;

  pt.x = lX;
  pt.y = lY;
  GpiMove( hps, &pt );
  pt.y -= ulSpace;
  GpiLine( hps, &pt );
  pt.y -= TXT_VERT_SPACE;
  _centeredText( hps, pt.x, pt.y, cbText, pszText, FALSE, -1 );
}

static VOID _legendColor(HPS hps, PPOINTL pPos, LONG lColor, PSZ pszText,
                         ULONG ulLeft, ULONG ulRight)
{
  ULONG    cbText = strlen( pszText );
  SIZEL    sizeText;
  ULONG    ulBoxSize;
  RECTL    rect;
  LONG     lXRight;

  putilGetTextSize( hps, cbText, pszText, &sizeText );
  sizeText.cy += sizeText.cy / 2;
  ulBoxSize = sizeText.cy - sizeText.cy / 4;
  lXRight = pPos->x + sizeText.cx + sizeText.cy;

  if ( ( pPos->x > ulLeft ) && ( lXRight > ulRight ) )
  {
    pPos->x = ulLeft;
    pPos->y -= sizeText.cy;
    lXRight = ulLeft + sizeText.cx + sizeText.cy;
  }

  ulBoxSize = sizeText.cy - sizeText.cy / 4;
  rect.xLeft = pPos->x;
  rect.yBottom = pPos->y + ( sizeText.cy - ulBoxSize ) / 2;
  rect.xRight = rect.xLeft + ulBoxSize;
  rect.yTop = rect.yBottom + ulBoxSize;
  WinDrawBorder( hps, &rect, 1, 1, 0x00000000, lColor, DB_INTERIOR );
  rect.xLeft = pPos->x + sizeText.cy;
  rect.yBottom = pPos->y;
  rect.xRight = lXRight;
  rect.yTop = rect.yBottom + sizeText.cy;
  WinDrawText( hps, cbText, pszText, &rect, 0, 0, DT_VCENTER );

  pPos->x = lXRight + sizeText.cy;
}


DSEXPORT VOID APIENTRY dsPaintDetails(HPS hps, PSIZEL psizeWnd)
{
  RECTL      rectMap, rect;
  ULONG      ulKbVAL = aulData[QSV_VIRTUALADDRESSLIMIT - 1] * 1024;
  ULONG      ulBytesVAL = ulKbVAL * 1024;
  ULONG      ulXRightLow;
  ULONG      ulXRightHi, ulKbHigh = ulKbVAL - LOW_MEM;
  ULONG      ulLoPr, ulBytesLoPr = aulData[QSV_MAXPRMEM - 1];
  ULONG      ulLoPrLimit, ulLoShLimit;
  ULONG      ulLoSh, ulBytesLoSh = aulData[QSV_MAXSHMEM - 1];
  ULONG      ulHiPr, ulBytesHiPr = aulData[QSV_MAXHPRMEM - 1];
  ULONG      ulHiSh, ulBytesHiSh = aulData[QSV_MAXHSHMEM - 1];
  ULONG      ulTemp = (ulBytesVAL - 0x20000000) >> 3;
  ULONG      ulBytesHiPrLimit = (ulBytesVAL - ulTemp) - 0x20000000;
  ULONG      ulBytesHiShLimit = ulBytesVAL - (0x20000000 + ulTemp);
  ULONG      ulHiPrLimit, ulHiShLimit;
  ULONG      ulWidth, ulYMid;
  SIZEL      size1, size2;
  ULONG      cbVAL, cbPrivate, cbShared;
  ULONG      ulHalfWL1, ulHalfWL2, ulHalfWL3, ulHalfWL4;
  CHAR       acVAL[16];
  POINTL     pt;

  // Get half width of each label.
  putilGetTextSize( hps, 1, "0", &size1 );
  ulHalfWL1 = size1.cx / 2;
  putilGetTextSize( hps, 6, "512 Mb", &size1 );
  ulHalfWL2 = size1.cx / 2;
  cbVAL = pstrFromBytes( (ULLONG)ulKbVAL * 1024, sizeof(acVAL), &acVAL, FALSE );
  strcpy( &acVAL[cbVAL], " (VAL)" );
  cbVAL += 6;
  putilGetTextSize( hps, cbVAL, &acVAL, &size1 );
  ulHalfWL3 = size1.cx / 2;
  putilGetTextSize( hps, 4, "4 Gb", &size1 );
  ulHalfWL4 = size1.cx / 2;
  // Get sizes of labels "Private:" and "Shared:".
  cbPrivate = strlen( STR_PRIVATE );
  putilGetTextSize( hps, cbPrivate, STR_PRIVATE, &size1 );
  cbShared = strlen( STR_SHARED );
  putilGetTextSize( hps, cbShared, STR_SHARED, &size2 );

  // Title.

  _centeredText( hps, psizeWnd->cx / 2,
                 psizeWnd->cy - TXT_VERT_SPACE, strlen( TITLE ), TITLE,
                 FALSE, psizeWnd->cx );

  // Memory map rectangle.
  rectMap.xLeft = 20 + max( size1.cx, size2.cx );
  rectMap.xRight = psizeWnd->cx - ulHalfWL4 - 10;
  if ( rectMap.xLeft >= rectMap.xRight )
    return;
  rectMap.yTop = psizeWnd->cy - size1.cy * 5;
  rectMap.yBottom = rectMap.yTop - 35;

  // Draw labels "Private:" and "Shared:".
  pt.x = rectMap.xLeft - size1.cx - 10;
  pt.y = rectMap.yTop + TXT_VERT_SPACE;
  GpiCharStringAt( hps, &pt, cbPrivate, STR_PRIVATE );
  pt.x = rectMap.xLeft - size2.cx - 10;
  pt.y = rectMap.yBottom - size2.cy - TXT_VERT_SPACE;
  GpiCharStringAt( hps, &pt, cbShared, STR_SHARED );

  ulWidth = rectMap.xRight - rectMap.xLeft;
  ulYMid = ( rectMap.yBottom + rectMap.yTop ) / 2;

  // Convert values from Kb to window coordinates.
  ulXRightLow = rectMap.xLeft + ( LOW_MEM * ulWidth / TOTAL_MEM );
  ulXRightHi = ulXRightLow + ( ulKbHigh * ulWidth / TOTAL_MEM );
  ulLoPr = (ulBytesLoPr / 1024) * ulWidth / TOTAL_MEM;
  ulLoSh = (ulBytesLoSh / 1024) * ulWidth / TOTAL_MEM;
  ulLoPrLimit = LIMIT_LOW_PR * ulWidth / TOTAL_MEM;
  ulLoShLimit = LIMIT_LOW_SH * ulWidth / TOTAL_MEM;
  ulHiPr = (ulBytesHiPr / 1024) * ulWidth / TOTAL_MEM;
  ulHiSh = (ulBytesHiSh / 1024) * ulWidth / TOTAL_MEM;
  ulHiPrLimit = (ulBytesHiPrLimit / 1024) * ulWidth / TOTAL_MEM;
  ulHiShLimit = (ulBytesHiShLimit / 1024) * ulWidth / TOTAL_MEM;

  putil3DFrame( hps, &rectMap, 0x00FFFFFF, 0x00505050 );
  WinInflateRect( NULLHANDLE, &rectMap, -1, -1 );
  putil3DFrame( hps, &rectMap, 0x00505050, 0x00FFFFFF );
  WinInflateRect( NULLHANDLE, &rectMap, -1, -1 );

  // Low  memory.

  rect.xLeft = rectMap.xLeft;
  rect.yBottom = rectMap.yBottom;
  rect.xRight = ulXRightLow;
  rect.yTop = rectMap.yTop;
  WinFillRect( hps, &rect, COL_LOW );
  if ( rect.xLeft < rect.xRight )
  {
    _centeredTextBytes( hps, ( rect.xLeft + rect.xRight ) / 2,
                        rect.yTop + 2 + TXT_VERT_SPACE, ulBytesLoPr, TRUE,
                        rect.xRight - rect.xLeft );
    _centeredTextBytes( hps, ( rect.xLeft + rect.xRight ) / 2,
                        rect.yBottom - 2 - TXT_VERT_SPACE, ulBytesLoSh, FALSE,
                        rect.xRight - rect.xLeft );

    // Low private memory.
    rect.xRight = ulXRightLow;
    rect.xLeft = rect.xRight - ulLoPr;
    rect.yBottom = ulYMid;
    rect.yTop = rectMap.yTop;
    _vertGradBox( hps, &rect,
                  putilMixRGB( COL_LOW_AVAIL, 0x00FFFFFF, 200 ),
                  COL_LOW_AVAIL, putilMixRGB( COL_LOW_AVAIL, 0x00000000, 170 ) );
    _limitLine( hps, ulXRightLow - ulLoPrLimit, ulYMid, rectMap.yTop );

    // Low shared memory.
    rect.xLeft = rectMap.xLeft;
    rect.xRight = rect.xLeft + ulLoSh;
    rect.yBottom = rectMap.yBottom;
    rect.yTop = ulYMid;
    _vertGradBox( hps, &rect,
                  COL_LOW_AVAIL_LIGHT, COL_LOW_AVAIL, COL_LOW_AVAIL_DARK );
    _limitLine( hps, rectMap.xLeft + ulLoShLimit, rectMap.yBottom, ulYMid );
  }

  // High memory.

  rect.xLeft = ulXRightLow;
  rect.yBottom = rectMap.yBottom;
  rect.xRight = ulXRightHi;
  rect.yTop = rectMap.yTop;
  WinFillRect( hps, &rect, COL_HIGH );
  if ( rect.xLeft < rect.xRight )
  {
    _centeredTextBytes( hps, ( rect.xLeft + rect.xRight ) / 2,
                        rect.yTop + 2 + TXT_VERT_SPACE, ulBytesHiPr, TRUE,
                        rect.xRight - rect.xLeft );
    _centeredTextBytes( hps, ( rect.xLeft + rect.xRight ) / 2,
                        rect.yBottom - 2 - TXT_VERT_SPACE, ulBytesHiSh, FALSE,
                        rect.xRight - rect.xLeft );

    // High private memory.
    rect.xRight = ulXRightHi;
    rect.xLeft = rect.xRight - ulHiPr;
    rect.yBottom = ulYMid;
    rect.yTop = rectMap.yTop;
    _vertGradBox( hps, &rect,
                  COL_HIGH_AVAIL_LIGHT, COL_HIGH_AVAIL, COL_HIGH_AVAIL_DARK );
    _limitLine( hps, ulXRightHi - ulHiPrLimit, ulYMid, rectMap.yTop );

    // High shared memory.
    rect.xLeft = ulXRightLow;
    rect.xRight = rect.xLeft + ulHiSh;
    rect.yBottom = rectMap.yBottom;
    rect.yTop = ulYMid;
    _vertGradBox( hps, &rect,
                  COL_HIGH_AVAIL_LIGHT, COL_HIGH_AVAIL, COL_HIGH_AVAIL_DARK );
    _limitLine( hps, ulXRightLow + ulHiShLimit, rectMap.yBottom, ulYMid );
  }

  // System memory.
  rect.xLeft = ulXRightHi;
  rect.yBottom = rectMap.yBottom;
  rect.xRight = rectMap.xRight;
  rect.yTop = rectMap.yTop;
  WinFillRect( hps, &rect, COL_SYSTEM );

  // Labels under map.
  {
    ULONG      ulSpace = size1.cy + size1.cy / 2;
    LONG       lY = rectMap.yBottom - 4;
    LONG       lX = ulXRightHi - ulHalfWL3;

    _labelTextBytes( hps, ulXRightHi, lY, cbVAL, &acVAL, ulSpace );

    if ( (ulXRightHi + ulHalfWL3) < (rectMap.xRight - ulHalfWL4) )
      _labelTextBytes( hps, rectMap.xRight, lY, 4, "4 Gb", ulSpace );

    if ( ( ulKbVAL != LOW_MEM ) &&
         ( (ulXRightLow + ulHalfWL2) < lX ) )
    {
      _labelTextBytes( hps, ulXRightLow, lY, 6, "512 Mb", ulSpace );
      lX = ulXRightLow - ulHalfWL2;
    }

    if ( (rectMap.xLeft + ulHalfWL1) < lX )
      _labelTextBytes( hps, rectMap.xLeft, lY, 1, "0", ulSpace );
  }

  // Map legend.
  {
    POINTL   pt;
    CHAR     acBuf[128];
    CHAR     acSize[16];

    pt.x = rectMap.xLeft;
    pt.y = rectMap.yBottom - size1.cy * 7;

    _legendColor( hps, &pt, COL_LOW, LGEND_LOW,
                  rectMap.xLeft, rectMap.xRight );

    if ( ulKbVAL != LOW_MEM )
    {
      pstrFromBytes( (ULLONG)ulKbHigh * 1024, sizeof(acSize), &acSize, FALSE );
      sprintf( &acBuf, "High (%s)", &acSize );
      _legendColor( hps, &pt, COL_HIGH, &acBuf, rectMap.xLeft, rectMap.xRight );
    }

    pstrFromBytes( (ULLONG)(TOTAL_MEM - ulKbVAL) * 1024, sizeof(acSize),
                   &acSize, FALSE );
    sprintf( &acBuf, "System (%s)", &acSize );
    _legendColor( hps, &pt, COL_SYSTEM, &acBuf, rectMap.xLeft, rectMap.xRight );
  }
}
