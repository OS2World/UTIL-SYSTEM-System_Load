#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include "graph.h"
#include "utils.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern HAB		hab;	// sl.c

BOOL grInit(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow,
            ULONG ulMin, ULONG ulMax)
{
  if ( ulValWindow <= 3 )
    return FALSE;

  pGraph->lLastIndex = -1;
  pGraph->cTimestamps = 0;

  pGraph->ulTimeWindow = ulTimeWindow;
  if ( ulMin < ulMax )
  {
    pGraph->ulInitMin = ulMin;
    pGraph->ulInitMax = ulMax;
  }
  else
  {
    // The maximum is not above the minimum - the scale of the ordinate will
    // be determined automatically in a function of grDraw().
    pGraph->ulInitMin = 0xFFFFFFFF;
    pGraph->ulInitMax = 0;
  }
  pGraph->ulValWindow = 0;
  pGraph->pulTimestamps = NULL;

  return grSetTimeScale( pGraph, ulValWindow, ulTimeWindow );
}

VOID grDone(PGRAPH pGraph)
{
  if ( pGraph->pulTimestamps != NULL )
    utilMemFree( pGraph->pulTimestamps );
}

BOOL grSetTimeScale(PGRAPH pGraph, ULONG ulValWindow, ULONG ulTimeWindow)
{
  LONG		lIdx;
  PULONG	pulTimestamps;
  ULONG		ulSrcIdx;
  ULONG		ulDstIdx;
  ULONG		cTimestamps;

  if ( ulValWindow == 0 || ulTimeWindow == 0 )
    return FALSE;

  // We need additional 3 points to full fill the time window on graph.
  ulValWindow += 3;

  pGraph->ulTimeWindow = ulTimeWindow;

  // Round ulValWindow up to the next 1024-boundary for a more efficient use
  // of memory allocated by utilMemAlloc() below (Because utilMemAlloc() calls
  // DosAllocMem() which round size up to the next page-size boundary).
  if ( (ulValWindow & 0x03FFL) != 0 )
    ulValWindow = ( ulValWindow & ~0x03FFL ) + 0x0400L;

  if ( pGraph->ulValWindow == ulValWindow )
    return TRUE;

  pulTimestamps = utilMemAlloc( ulValWindow * sizeof(ULONG) );
  if ( pulTimestamps == NULL )
  {
    debug( "Not enough memory" );
    return FALSE;
  }

  ulSrcIdx = pGraph->lLastIndex;
  ulDstIdx = ulValWindow - 1;
  cTimestamps = min( pGraph->cTimestamps, ulValWindow );

  for( lIdx = 0; lIdx < cTimestamps; lIdx++ )
  {
    pulTimestamps[ulDstIdx--] = pGraph->pulTimestamps[ulSrcIdx];

    if ( ulSrcIdx == 0 )
      ulSrcIdx = pGraph->ulValWindow - 1;
    else
      ulSrcIdx--;
  }

  if ( pGraph->pulTimestamps != NULL )
    utilMemFree( pGraph->pulTimestamps );
  pGraph->pulTimestamps = pulTimestamps;
  pGraph->cTimestamps = cTimestamps;
  pGraph->ulValWindow = ulValWindow;
  pGraph->lLastIndex = ulValWindow - 1;

  return TRUE;
}

VOID grNewTimestamp(PGRAPH pGraph, ULONG ulTimestamp)
{
  pGraph->lLastIndex++;

  if ( pGraph->lLastIndex == pGraph->ulValWindow )
    pGraph->lLastIndex = 0;

  pGraph->pulTimestamps[ pGraph->lLastIndex ] = ulTimestamp;

  if ( pGraph->cTimestamps < pGraph->ulValWindow )
    pGraph->cTimestamps++;

  pGraph->ulMin = pGraph->ulInitMin;
  pGraph->ulMax = pGraph->ulInitMax;
  pGraph->ulMinAmp = (ULONG)(-1);
  pGraph->ulMaxAmp = 0;

  DosGetDateTime( &pGraph->stDateTime );
}

BOOL grGetTimestamp(PGRAPH pGraph, PULONG pulTimestamp)
{
  if ( pGraph->cTimestamps == 0 || pGraph->lLastIndex == -1 )
   return FALSE;

  *pulTimestamp = pGraph->pulTimestamps[ pGraph->lLastIndex ];
  return TRUE;
}

VOID grInitVal(PGRAPH pGraph, PGRVAL pGrVal)
{
  // Memory block for values will be allocated in grSetValue()
  pGrVal->pulValues = NULL;
  // Value window size will be set in grSetValue()
  pGrVal->ulValWindow = 0;
  pGrVal->lLastIndex = -1;

  grResetVal( pGraph, pGrVal );
}

VOID grDoneVal(PGRVAL pGrVal)
{
  if ( pGrVal->pulValues != NULL )
    utilMemFree( pGrVal->pulValues );
}

VOID grResetVal(PGRAPH pGraph, PGRVAL pGrVal)
{
  pGrVal->ulMin = pGraph->ulInitMin;
  pGrVal->ulMax = pGraph->ulInitMax;
  pGrVal->cValues = 0;
}

VOID grSetValue(PGRAPH pGraph, PGRVAL pGrVal, ULONG ulValue)
{
  BOOL		fMinAway = FALSE;
  BOOL		fMaxAway = FALSE;
  ULONG		ulIdx;

  if ( pGrVal->ulValWindow != pGraph->ulValWindow )
  {
    // Value window size changed - allocate new size memory block and copy
    // (part) data from old.
    PULONG	pulValues = utilMemAlloc( pGraph->ulValWindow * sizeof(ULONG) );

    if ( pulValues != NULL )
    {
      ULONG	ulSrcIdx = pGrVal->lLastIndex;
      ULONG	ulDstIdx = pGraph->ulValWindow - 1;
      ULONG	cValues = min( pGrVal->cValues, pGraph->cTimestamps );
      ULONG	ulSrcValue;

      pGrVal->ulMin = pGraph->ulInitMin;
      pGrVal->ulMax = pGraph->ulInitMax;
      for( ulIdx = 0; ulIdx < cValues; ulIdx++ )
      {
        ulSrcValue = pGrVal->pulValues[ulSrcIdx];
        pulValues[ulDstIdx--] = ulSrcValue;

        if ( ulSrcValue > pGrVal->ulMax )
          pGrVal->ulMax = ulSrcValue;
        else if ( ulSrcValue < pGrVal->ulMin )
          pGrVal->ulMin = ulSrcValue;

        if ( ulSrcIdx == 0 )
          ulSrcIdx = pGrVal->ulValWindow - 1;
        else
          ulSrcIdx--;
      }

      if ( pGrVal->pulValues != NULL )
        utilMemFree( pGrVal->pulValues );
      pGrVal->pulValues = pulValues;
      pGrVal->cValues = cValues;
      pGrVal->ulValWindow = pGraph->ulValWindow;
      pGrVal->lLastIndex = 0;
    }
    else if ( pGrVal->ulValWindow == 0 )
      return;
  }
  else
  {
    // New index for value
    pGrVal->lLastIndex++;
    if ( pGrVal->lLastIndex == pGrVal->ulValWindow )
      pGrVal->lLastIndex = 0;

    // We overwrite the old value - set flags for searching maximum/minimum
    // valuess if overwrites values is extremum.
    if ( pGrVal->cValues == pGraph->ulValWindow )
    {
      ULONG	ulValueAway = pGrVal->pulValues[pGrVal->lLastIndex];

      fMinAway = ulValueAway == pGrVal->ulMin;
      fMaxAway = ulValueAway == pGrVal->ulMax;
    }
  }

  // New value is the extremes - no need to seek extremes in the data.
  if ( ulValue <= pGrVal->ulMin )
  {
    fMinAway = FALSE;
    pGrVal->ulMin = ulValue;
  }
  if ( ulValue >= pGrVal->ulMax )
  {
    fMaxAway = FALSE;
    pGrVal->ulMax = ulValue;
  }

  // Store the new value.
  pGrVal->pulValues[pGrVal->lLastIndex] = ulValue;
  // Increment stored values counter.
  if ( pGrVal->cValues < pGraph->ulValWindow )
    pGrVal->cValues++;

  // Search extremes in the stored values if appropriate flags set.

  if ( fMinAway )
  {
    pGrVal->ulMin = pGraph->ulInitMin;
    for( ulIdx = 0; ulIdx < pGrVal->ulValWindow - 1; ulIdx++ )
    {
      ulValue = pGrVal->pulValues[ulIdx];
      if ( ulValue < pGrVal->ulMin )
        pGrVal->ulMin = ulValue;
    }
  }

  if ( fMaxAway )
  {
    pGrVal->ulMax = pGraph->ulInitMax;
    for( ulIdx = 0; ulIdx < pGrVal->ulValWindow - 1; ulIdx++ )
    {
      ulValue = pGrVal->pulValues[ulIdx];
      if ( ulValue > pGrVal->ulMax )
        pGrVal->ulMax = ulValue;
    }
  }

  // Set the global value of minimum.
  if ( pGrVal->ulMin < pGraph->ulMin )
    pGraph->ulMin = pGrVal->ulMin;
  // Set the global value of maximum.
  if ( pGrVal->ulMax > pGraph->ulMax )
    pGraph->ulMax = pGrVal->ulMax;
  // Set global minimum amplitude
  {
    ULONG		ulAmp = pGrVal->ulMax - pGrVal->ulMin;

    if ( ulAmp != 0 )
    {
      if ( pGraph->ulMinAmp > ulAmp )
        pGraph->ulMinAmp = ulAmp;
      if ( pGraph->ulMaxAmp < ulAmp )
        pGraph->ulMaxAmp = ulAmp;
    }
  }
}

BOOL grGetValue(PGRAPH pGraph, PGRVAL pGrVal, PULONG pulValue)
{
  if ( pGrVal->lLastIndex < 0 )
  {
    *pulValue = 0;
    return FALSE;
  }

  if ( pulValue != NULL )
    *pulValue = pGrVal->pulValues[pGrVal->lLastIndex];

  return TRUE;
}

VOID grDraw(PGRAPH pGraph, HPS hps, PRECTL prclGraph,
            ULONG cGrVal, PGRVAL *ppGrVal, PGRPARAM pParam)
{
  POINTL		pt;
  ULONG			ulMin = pGraph->ulMin;
  ULONG			ulMax = pGraph->ulMax;
  LONG			lIdx;
  PGRVAL		pGrVal;
  ULONG			ulLineWidth = 0;
  RECTL			rclGraph;	// Rectangle to draw lines.
  HRGN			hrgn;		// Region to draw (and clipping) lines.

  // Definition of the rectangle in which the lines of graphs will be drawn.

  for( lIdx = 0; lIdx < cGrVal; lIdx++ )
    if ( ulLineWidth < pParam->pParamVal[lIdx].ulLineWidth )
      ulLineWidth = pParam->pParamVal[lIdx].ulLineWidth;

  rclGraph = *prclGraph;
  WinInflateRect( hab, &rclGraph, -pParam->ulBorderCX, -pParam->ulBorderCY );
  hrgn = GpiCreateRegion( hps, 1, &rclGraph );
  WinInflateRect( hab, &rclGraph, -ulLineWidth, -ulLineWidth );
  if ( WinIsRectEmpty( hab, &rclGraph ) )	// Have space to draw?
  {
    GpiDestroyRegion( hps, hrgn );
    return;
  }


  // Calculation ordinate minimum and maximum.
  // -----------------------------------------

  // When maximum is not above the minimum - user is allowed to determine the
  // scale of ordinates automatically.
  if ( ( pGraph->ulInitMax <= pGraph->ulInitMin ) &&
       ( pGraph->ulMax > pGraph->ulMin ) )
  do
  {
    // The minimum and maximum values are chosen so that the graph with the
    // smallest amplitude took 25% of the height of the rectangle.

    ULONG	ulGrValMin = ppGrVal[0]->ulMin;
    ULONG	ulGrValMax = ppGrVal[0]->ulMax;
    ULONG	ulGrAmp, ulValWin;
    ULONG	ulGrScrAmp;
    ULONG	ulGlobMinAmp = pGraph->ulMinAmp;
    ULONG	ulGlobMaxAmp = pGraph->ulMaxAmp;
    ULONG	ulScrMaxAmp = rclGraph.yTop - rclGraph.yBottom;
    ULONG	ulScrMinAmp = ulScrMaxAmp / 4;

    if ( ulGlobMinAmp >= ulGlobMaxAmp )
      break;

    for( lIdx = 1; lIdx < cGrVal; lIdx++ )
    {
      pGrVal = ppGrVal[lIdx];

      if ( pGrVal->ulMax > ulGrValMax )
        ulGrValMax = pGrVal->ulMax;
      if ( pGrVal->ulMin < ulGrValMin )
        ulGrValMin = pGrVal->ulMin;
    }
    ulGrAmp = ulGrValMax - ulGrValMin;

    // Maximal amplitude - use the global values of minimum and maximum.
    if ( ulGrAmp == ulGlobMaxAmp )
    {
      ulMin = ulGrValMin;
      ulMax = ulGrValMax;
      break;
    }

    // Zero amplitude - try set line at center and min./max. multiple of
    // 10/100/1000.
    if ( ulGrAmp == 0 )
    {
      if ( ulGrValMin == 0 )
      {
        ulMax = 10;
        ulMin = 0;
      }
      else if ( ulGrValMin < (((~0UL)/2)-1000) )
      {
        ULONG	ulMul;

        if ( ulGrValMin > 1000 )
          ulMul = 1000;
        else if ( ulGrValMin > 100 )
          ulMul = 100;
        else
          ulMul = 10;

        ulMax = (ulGrValMin * 2 / ulMul) * ulMul;
        ulMin = 0;
      }
      break;
    }

    // ulGlobMinAmp - Global minimum amplitude.
    // ulGlobMaxAmp - Global maximum amplitude.
    // ulGrAmp - This graph amplitude (maximum of all graphs that will be drawn
    //           now).
    // ulScrMinAmp - Minimum amplitude on destination device.
    // ulScrMaxAmp - Maximum amplitude on destination device.

    // The calculation of the target display amplitude: ulGrScrAmp.
    //
    //            screen Y
    // ulScrMaxAmp _|......../
    //              |       /.
    //              |      / .
    // ulGrScrAmp? _|...../. .
    // ulScrMinAmp _|..../ . .
    //              |    . . .
    //              -----+-+-+------------> graph values
    //        ulGlobMinAmp | ulGlobMaxAmp
    //                  ulGrAmp

    // ulGlobAmpD = ulGlobMaxAmp - ulGlobMinAmp;
    // ulScrAmpD = ulScrMaxAmp - ulScrMinAmp;
    //
    // ulGlobAmpD   (ulGrAmp - ulGlobMinAmp)
    // ---------- = ------------------------
    // ulScrAmpD    (ulGrScrAmp - ulScrMinAmp)
    //
    // ulGrScrAmp = (ulScrAmpD * (ulGrAmp - ulGlobMinAmp) / ulGlobAmpD) + ulScrMinAmp;

    ulGrScrAmp = ( (ulScrMaxAmp - ulScrMinAmp) * (ulGrAmp - ulGlobMinAmp) /
                   (ulGlobMaxAmp - ulGlobMinAmp) ) + ulScrMinAmp;

    if ( ulGrScrAmp == 0 ) // Is target rectangle's height close to zero?
      break;

    // The calculation of the graph values window to fit in target display
    // height: ulValWin.
    //
    // ulValWin       ulGrAmp
    // ------------ = --------
    // ulScrMaxAmp    ulGrScrAmp

    ulValWin = (ulScrMaxAmp * ulGrAmp) / ulGrScrAmp;

    /* Set minimum and maximum values.
    //
    // I. Values window near zero   II. Top of values window is maximum
    //    (minimum = 0).                value.
    //                |                                 |        
    //         ulMax -|........    ulMax=pGrVal->ulMax -|........       
    // pGrVal->ulMax _|                  pGrVal->ulMin _| /\/\/\        
    // pGrVal->ulMin _| /\/\/\                          |  
    //                |                          ulMin -|........
    //          ulMin-0--------                         0-------- 
    */

    if ( ulGrValMax <= ulValWin )
    {
      ulMax = ulValWin;
      ulMin = 0;
    }
    else
    {
      ulMax = ( ( ulGrValMax / 10 ) + 1 ) * 10;
      if ( (ulMax - ulValWin) > ulGrValMin )
        ulMax = ulGrValMax;
      ulMin = ulMax - ulValWin;
    }
  }
  while( FALSE );


  // Captions of graph
  // -----------------

  {
    GRADIENTL		gradlAngle;
    ULONG		ulOrdAreaBottom, ulOrdAreaTop, ulAbsAreaRight;
    CHAR		szBuf[128];
    SIZEL		sizeText;
    LONG		lLen;
    LONG		lMinLeft = prclGraph->xLeft;

    // Maximum value near left top corner

    if ( ( pParam->ulFlags & GRPF_MAX_LABEL ) != 0 )
    {
      if ( pParam->fnValToStr != NULL )
        lLen = pParam->fnValToStr( ulMax, &szBuf, sizeof(szBuf) );
      else
        lLen = _snprintf( &szBuf, sizeof(szBuf), "%u", ulMax );

      if ( lLen > 0 )
      {
        utilGetTextSize( hps, lLen, &szBuf, &sizeText );
        pt.y = prclGraph->yTop - sizeText.cy;
        if ( pt.y < prclGraph->yBottom )
          ulOrdAreaTop = prclGraph->yTop;
        else
        {
          pt.x = prclGraph->xLeft - GRAPG_ORDINATE_LEFT_PAD - sizeText.cx;
          GpiCharStringAt( hps, &pt, lLen, &szBuf );
          ulOrdAreaTop = pt.y;
          lMinLeft = pt.x;
        }
      }
    }
    else
      ulOrdAreaTop = prclGraph->yTop;

    // Minimum value near left bottom corner

    if ( ( pParam->ulFlags & GRPF_MIN_LABEL ) != 0 )
    {
      if ( pParam->fnValToStr != NULL )
        lLen = pParam->fnValToStr( ulMin, &szBuf, sizeof(szBuf) );
      else
        lLen = sprintf( &szBuf, "%u", ulMin );

      if ( lLen > 0 )
      {
        utilGetTextSize( hps, lLen, &szBuf, &sizeText );
        ulOrdAreaBottom = prclGraph->yBottom + sizeText.cy;
        if ( ulOrdAreaBottom > ulOrdAreaTop )
          ulOrdAreaBottom = prclGraph->yBottom;
        else
        {
          pt.x = prclGraph->xLeft - GRAPG_ORDINATE_LEFT_PAD - sizeText.cx;
          pt.y = prclGraph->yBottom;
          GpiCharStringAt( hps, &pt, lLen, &szBuf );
          ulOrdAreaBottom = pt.y + sizeText.cy;
          if ( pt.x < lMinLeft )
            lMinLeft = pt.x;
        }
      }
    }
    else
      ulOrdAreaBottom = prclGraph->yBottom;

    // Ordinate caption

    if ( pParam->pszOrdinateCaption != NULL )
    {
      GpiSetCharMode( hps, CM_MODE3 );
      gradlAngle.x = 0;
      gradlAngle.y = 100;
      GpiSetCharAngle( hps, &gradlAngle );

      lLen = strlen( pParam->pszOrdinateCaption );
      utilGetTextSize( hps, lLen, pParam->pszOrdinateCaption, &sizeText );
      pt.y = ( (ulOrdAreaBottom + ulOrdAreaTop) / 2 ) - ( sizeText.cy / 2 );
      if ( (LONG)(pt.y - ulOrdAreaBottom) > GRAPG_ORDINATE_CAPTION_VPAD )
      {
        pt.x = prclGraph->xLeft - GRAPG_ORDINATE_LEFT_PAD;
        GpiCharStringAt( hps, &pt, lLen, pParam->pszOrdinateCaption );
        pt.x -= sizeText.cx;
        if ( pt.x < lMinLeft )
          lMinLeft = pt.x;
      }

      gradlAngle.y = 0;
      GpiSetCharAngle( hps, &gradlAngle );
      GpiSetCharMode( hps, CM_MODE1 );
    }

    // Abscissa top caption

    if ( pParam->pszAbscissaTopCaption != NULL )
    {
      RECTL		rect;

      lLen = strlen( pParam->pszAbscissaTopCaption );
      utilGetTextSize( hps, lLen, pParam->pszAbscissaTopCaption, &sizeText );

      if ( ( pParam->ulFlags & GRPF_LEFT_TOP_CAPTION ) != 0 )
        rect.xLeft = prclGraph->xLeft;
      else
        rect.xLeft = ( (prclGraph->xLeft + prclGraph->xRight) >> 1 ) -
                     ( sizeText.cx >> 1 );

      rect.yBottom = prclGraph->yTop + GRAPG_ABSCISSA_TOP_PAD;
      rect.xRight = prclGraph->xRight;
      rect.yTop = rect.yBottom + sizeText.cy;

      utilCharStringRect( hps, &rect, lLen, pParam->pszAbscissaTopCaption,
                          SL_CSR_CUTFADE );
    }

    // Time of last timestamp near right bottom corner

    if ( ( pParam->ulFlags & GRPF_TIME_LABEL ) != 0 )
    {
      lLen = sprintf( &szBuf, "%u:%.2u:00.00", pGraph->stDateTime.hours,
                       pGraph->stDateTime.minutes );
      utilGetTextSize( hps, lLen, &szBuf, &sizeText );
      pt.x = prclGraph->xRight - sizeText.cx;
      if ( pt.x < lMinLeft )
        ulAbsAreaRight = prclGraph->xRight;
      else
      {
        pt.y = prclGraph->yBottom - GRAPG_ABSCISSA_BOTTOM_PAD - sizeText.cy;

        lLen = sprintf( &szBuf, "%u:%.2u:%.2u.%u", pGraph->stDateTime.hours,
                         pGraph->stDateTime.minutes, pGraph->stDateTime.seconds,
                         pGraph->stDateTime.hundredths );
        GpiCharStringAt( hps, &pt, lLen, &szBuf );
        ulAbsAreaRight = pt.x - GRAPG_ABSCISSA_BOTTOM_CAPTION_RPAD;
      }
    }
    else
      ulAbsAreaRight = prclGraph->xRight;

    // Abscissa bottom caption

    if ( pParam->pszAbscissaBottomCaption != NULL )
    {
      lLen = strlen( pParam->pszAbscissaBottomCaption );
      utilGetTextSize( hps, lLen, pParam->pszAbscissaBottomCaption, &sizeText );

      pt.x = (prclGraph->xLeft + ulAbsAreaRight) / 2 - sizeText.cx / 2;

      if ( pt.x >= prclGraph->xLeft )
      {
        pt.y = prclGraph->yBottom - GRAPG_ABSCISSA_BOTTOM_PAD - sizeText.cy;
        GpiCharStringAt( hps, &pt, lLen,
                         pParam->pszAbscissaBottomCaption );
      }
    }
  }

  
  // Graph
  // -----

  // Graph background and border
  WinDrawBorder( hps, prclGraph, pParam->ulBorderCX, pParam->ulBorderCY,
                 pParam->clrBorder, pParam->clrBackground, DB_INTERIOR );

  if ( ( pGraph->ulMax > pGraph->ulMin ) && ( pGraph->cTimestamps > 1 ) )
  {
    HRGN		hrgnOld;
    ULONG		ulGrValIdx;
    ULONG		ulTSIdx, ulValIdx;
    ULONG		ulLastTS;
    ULONG		ulGraphHeight;
    ULONG		ulGraphWidth;
    ULONG		ulParamValIdx = 0;
    LONG		lSaveColor, lSaveBackColor;

    // Region already initialized - set clipping with it.
    GpiSetClipRegion( hps, hrgn, &hrgnOld );

    ulGraphHeight = rclGraph.yTop - rclGraph.yBottom;
    ulGraphWidth = rclGraph.xRight - rclGraph.xLeft;

    // Save PS colors
    lSaveColor = GpiQueryColor( hps );
    lSaveBackColor = GpiQueryBackColor( hps );

    // For all given data storages...
    for( ulGrValIdx = 0; ulGrValIdx < cGrVal; ulGrValIdx++ )
    {
      pGrVal = ppGrVal[ulGrValIdx];

      // Set display parameters for data storage
      GpiSetColor( hps, pParam->pParamVal[ulParamValIdx].clrGraph );
      ulLineWidth = pParam->pParamVal[ulParamValIdx].ulLineWidth;
      if ( ulLineWidth > 0 )
        GpiSetLineWidth( hps, ulLineWidth * LINEWIDTH_NORMAL );
      else
      {
        GpiBeginPath( hps, 1 );
        pt.x = rclGraph.xRight;
        pt.y = rclGraph.yBottom;
        GpiMove( hps, &pt );
      }

      // Next index in the array of graph parameters
      ulParamValIdx++;
      ulParamValIdx %= pParam->cParamVal;

      // Last timestamp index
      ulTSIdx = pGraph->lLastIndex;
      // Last timestamp
      ulLastTS = pGraph->pulTimestamps[ulTSIdx];
      // Last value index in data storage
      ulValIdx = pGrVal->lLastIndex;

      // Set first (rightmost) position of graph in PS
      pt.x = rclGraph.xRight;
      pt.y = rclGraph.yBottom +
             ( ((pGrVal->pulValues[ulValIdx] - ulMin) * ulGraphHeight) / (ulMax - ulMin) );
      if ( ulLineWidth > 0 )
        GpiMove( hps, &pt );
      else
        GpiLine( hps, &pt );

      // Draw graph for data storage

      for( lIdx = 0; lIdx < min( pGraph->cTimestamps, pGrVal->cValues ) - 1;
           lIdx++ )
      {
        // Previous timestamp index
        if ( ulTSIdx == 0 )
          ulTSIdx = pGraph->ulValWindow - 1;
        else
          ulTSIdx--;

        // Previous value index
        if ( ulValIdx == 0 )
          ulValIdx = pGrVal->ulValWindow - 1;
        else
          ulValIdx--;

        // Draw section
        pt.x = rclGraph.xRight -
               ( (ulLastTS - pGraph->pulTimestamps[ulTSIdx]) * ulGraphWidth / pGraph->ulTimeWindow );
        pt.y = rclGraph.yBottom +
               ( ( (pGrVal->pulValues[ulValIdx] - ulMin) * ulGraphHeight ) / (ulMax - ulMin) );
        GpiLine( hps, &pt );

        if ( pt.x <= rclGraph.xLeft )
          break;
      }

      if ( ulLineWidth == 0 )
      {
        pt.y = rclGraph.yBottom;
        GpiLine( hps, &pt );
        GpiEndPath( hps );
        GpiFillPath( hps, 1, FPATH_INCL );
      }
    }

    // Restore PS colors.
    GpiSetColor( hps, lSaveColor );
    GpiSetBackColor( hps, lSaveBackColor );
    // Restore clipping.
    GpiSetClipRegion( hps, NULLHANDLE, &hrgnOld );
  }

  GpiDestroyRegion( hps, hrgn );
}
