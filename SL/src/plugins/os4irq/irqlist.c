#define INCL_DOSFILEMGR
#define INCL_DOSDEVIOCTL
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#include <os2.h>
#include <rmcalls.h>
#include <stdlib.h>
#include <string.h>
#include "irqlist.h"
#include <debug.h>     // Must be the last.


typedef struct _IRQLIST {
  ULONG      ulSize;
  ULONG      ulAllocated;
  PIRQDESCR  pData;
} IRQLIST, *PIRQLIST;


/* ulRTCIRQLevel - RTC adapter (timer) IRQ level. */
ULONG        ulRTCIRQLevel = ~0;


static PRM_ENUMNODES_DATA _RMNewEnumNodes(HFILE hfResMgr)
{
  ULONG                ulRC;
  RM_ENUMNODES_PARM    stEnumParam = { RM_COMMAND_PHYS };
  RM_ENUMNODES_DATA    stEnumData;
  PRM_ENUMNODES_DATA   pEnumData = NULL;
  ULONG                cbEnumData;

  // Query list length
  ulRC = DosDevIOCtl( hfResMgr, 0x80, 2, /* 2 - FUNC_RM_ENUM_NODE */
                      &stEnumParam, sizeof(stEnumParam), 0,
                      (PULONG)&stEnumData, sizeof(stEnumData), 0 );
  if ( ulRC != 0xFF0C )
  {
    debug( "#1 EnumNode, rc: 0x%X\n", ulRC );
    return NULL;
  }

  cbEnumData = sizeof(RM_ENUMNODES_DATA) - sizeof(NODEENTRY) +
               ( stEnumData.NumEntries * sizeof(NODEENTRY) );
  pEnumData = malloc( cbEnumData );
  if ( pEnumData == NULL )
  {
    debugCP( "Not enough memory" );
    return NULL;
  }

  ulRC = DosDevIOCtl( hfResMgr, 0x80, 2, /* 2 - FUNC_RM_ENUM_NODE */
                      &stEnumParam, sizeof(stEnumParam), 0, (PULONG)pEnumData,
                      cbEnumData, 0 );
  if ( ulRC != 0 )
  {
    debug( "#2 EnumNode, rc: 0x%X\n", ulRC );
    free( pEnumData  );
    return NULL;
  }

  if ( stEnumData.NumEntries != pEnumData->NumEntries )
  {
    debug( "EnumNode, RM reported that %u records would be returned, "
           "but returned %u", stEnumData.NumEntries, pEnumData->NumEntries );
    if ( pEnumData->NumEntries > stEnumData.NumEntries )
      pEnumData->NumEntries = stEnumData.NumEntries;
  }

  return pEnumData;
}

static BOOL _RMGetNodeInfo(HFILE hfResMgr, ULONG ulRMHandle,
                           ULONG cbBuf, PVOID pBuf)
{
  ULONG                ulRC;
  RM_GETNODE_PARM      stNodeParam;

  if ( ( pBuf == NULL ) || ( cbBuf <= sizeof(PRM_GETNODE_DATA) ) )
    return FALSE;

  stNodeParam.RMHandle = ulRMHandle;
  stNodeParam.Linaddr = (ULONG)pBuf; /* RM with a native (not OS/4) kernel
                                        requires this. :-/  */

  ulRC = DosDevIOCtl( hfResMgr, 0x80, 1, &stNodeParam, sizeof(stNodeParam), 0,
                      (PULONG)pBuf, cbBuf - 1, 0 );
  if ( ulRC != 0 )
  {
    debug( "GetNodeInfo, rc: 0x%X\n", ulRC );
    return FALSE;
  }

  if ( ((PRM_GETNODE_DATA)pBuf)->RMNodeSize >= cbBuf )
  {
    debug( "GetNodeInfo data block too big: %lu bytes, requested %lu bytes",
           ((PRM_GETNODE_DATA)pBuf)->RMNodeSize, cbBuf - 1 );
    return FALSE;
  }

  // Just paranoia. To be sure that no string will be read abroad data.
  ((PBYTE)pBuf)[ ((PRM_GETNODE_DATA)pBuf)->RMNodeSize ] = 0;

  return TRUE;
}


static BOOL _ensureSpace(PIRQLIST pList, ULONG ulSpace)
{
  ULONG      ulNewSize;
  PIRQDESCR  pNew;

  if ( ( pList->ulSize + ulSpace ) < pList->ulAllocated )
    return TRUE;

  // Rounding up to 256 bytes
  ulNewSize = ( (pList->ulSize + ulSpace) + 0xFELU ) & ~0xFFLU;

  pNew = realloc( pList->pData, ulNewSize );
  if ( pNew == NULL )
  {
    debugCP( "Not enough memory" );
    return FALSE;
  }

  pList->ulAllocated = ulNewSize;
  pList->pData = pNew;
  return TRUE;
}

static BOOL _addIRQItem(PIRQLIST pList, USHORT usIRQLevel, PSZ pszTitle)
{
  PIRQDESCR  pIRQDescr;
  ULONG      cbTitle = strlen( pszTitle );
  PCHAR      pcDescr;
  BOOL       fNotListed;

  if ( !_ensureSpace( pList, sizeof(IRQDESCR) + cbTitle + 2 ) )
    return FALSE;

  if ( pList->ulSize == 0 )
    fNotListed = TRUE;
  else
  {
    pIRQDescr = pList->pData;
    fNotListed = FALSE;

    while( pIRQDescr->usIRQLevel != usIRQLevel )
    {
      if ( pIRQDescr->usNextOffs == 0 )
      {
        fNotListed = TRUE;
        pIRQDescr->usNextOffs = &((PBYTE)pList->pData)[pList->ulSize] -
                                (PBYTE)pIRQDescr;
        break;
      }
      pIRQDescr = (PIRQDESCR)( &((PBYTE)pIRQDescr)[pIRQDescr->usNextOffs] );
    }
  }

  if ( fNotListed )
  {
    // Create a new entry at the end of list
    pIRQDescr = (PIRQDESCR)( &((PBYTE)pList->pData)[pList->ulSize] );

    pIRQDescr->usIRQLevel = usIRQLevel;
    pIRQDescr->usNextOffs = 0;
    pIRQDescr->acDescr[0] = '\0';
    pList->ulSize += sizeof(IRQDESCR);
    /*debug( "IRQ %u: Created a new entry, size = %lu",
           usIRQLevel, pList->ulSize );*/
  }

  if ( pIRQDescr->acDescr[0] != '\0' )
    // The entry already has text, we need two more bytes (comma and space).
    cbTitle += 2;

  if ( pIRQDescr->usNextOffs != 0 )
  {
    /* To add text to an existing entry, we need to shift the next piece of
       data by the length of the text with a comma and a space.  */

    /*debug( "IRQ %u: Move %d bytes of forward data. "
           "Total size: %lu, entry old size: %lu",
           usIRQLevel,

           &((PBYTE)pList->pData)[pList->ulSize] -
           &((PBYTE)pIRQDescr)[pIRQDescr->usNextOffs],

           pList->ulSize, pIRQDescr->usNextOffs );*/

    memmove( &((PBYTE)pIRQDescr)[pIRQDescr->usNextOffs + cbTitle],

             &((PBYTE)pIRQDescr)[pIRQDescr->usNextOffs],

             &((PBYTE)pList->pData)[pList->ulSize] -
             &((PBYTE)pIRQDescr)[pIRQDescr->usNextOffs] );

    pIRQDescr->usNextOffs += cbTitle;
  }

  if ( pIRQDescr->acDescr[0] != '\0' )
  {
    // There is already text in the entry, add the prefix: ",".

    /*debug( "IRQ %u: Write next string (len: %lu): \", %s\"",
           usIRQLevel, cbTitle, pszTitle );*/
    pcDescr = strchr( pIRQDescr->acDescr, '\0' );
    *(pcDescr++) = 0x0D;
    *(pcDescr++) = 0x0A;
/*    *(pcDescr++) = ',';
    *(pcDescr++) = ' ';  */
  }
  else
  {
    /*debug( "IRQ %u: Write string (len: %lu): \"%s\"",
           usIRQLevel, cbTitle, pszTitle );*/
    pcDescr = pIRQDescr->acDescr;
  }

  strcpy( pcDescr, pszTitle );
  pList->ulSize += cbTitle;
  /*debug( "IRQ %u: New size: %lu", usIRQLevel, pList->ulSize );*/

  return TRUE;
}

static BOOL _checkRMNode(PIRQLIST pList, PRM_GETNODE_DATA pRMDevNode)
{
  ULONG                ulIdx;
  PRESOURCELIST        pResources = pRMDevNode->RMNode.pResourceList;
  PRESOURCESTRUCT      pResourceStruct;
  PSZ                  pszTitle;
  BOOL                 fRTC;               // Adapter is RTC (Timer)

  if ( ( pResources == NULL ) ||
       ( (pRMDevNode->RMNode.NodeType != RMTYPE_ADAPTER) &&
         (pRMDevNode->RMNode.NodeType != RMTYPE_DEVICE) ) )
    return TRUE;

  if ( pRMDevNode->RMNode.NodeType == RMTYPE_ADAPTER )
  {
    pszTitle = pRMDevNode->RMNode.pAdapterNode->AdaptDescriptName;

    fRTC = ( pRMDevNode->RMNode.pAdapterNode->BaseType == AS_BASE_PERIPH ) &&
           ( pRMDevNode->RMNode.pAdapterNode->SubType == AS_SUB_RTC );
  }
  else
  {
    pszTitle = pRMDevNode->RMNode.pDeviceNode->DevDescriptName;
    fRTC = FALSE;
  }

  for( ulIdx = 0; ulIdx < pResources->Count; ulIdx++ )
  {
    pResourceStruct = &pResources->Resource[ulIdx];
    if ( pResourceStruct->ResourceType != RS_TYPE_IRQ )
      continue;

    if ( fRTC )
      // Store timer IRQ level.
      ulRTCIRQLevel = pResourceStruct->IRQResource.IRQLevel;

    if ( !_addIRQItem( pList, pResourceStruct->IRQResource.IRQLevel,
                       pszTitle ) )
      return FALSE;
  }

  return TRUE;
}



PIRQDESCR irqGetList()
{
  HFILE                hfResMgr;
  ULONG                ulRC, ulAction, ulIdx;
  PRM_ENUMNODES_DATA   pEnumData;
  BYTE                 abRMDevNode[1024];
  PRM_GETNODE_DATA     pRMDevNode = (PRM_GETNODE_DATA)abRMDevNode;
  IRQLIST              stIRQList = { 0 };
  PIRQDESCR            pIRQDescr;

  ulRC = DosOpen( "RESMGR$", &hfResMgr, &ulAction,
                  0, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_ACCESS_READWRITE | OPEN_FLAGS_NOINHERIT |
                  OPEN_SHARE_DENYNONE | OPEN_FLAGS_FAIL_ON_ERROR, 0 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosOpen(\"%s\",,,,,,,), rc=%lu", "RESMGR$", ulRC );
    return NULL;
  }

  pEnumData = _RMNewEnumNodes( hfResMgr );
  if ( pEnumData != NULL )
  {
    for( ulIdx = 0; ulIdx < pEnumData->NumEntries; ulIdx++ )
    {
      if ( _RMGetNodeInfo( hfResMgr, pEnumData->NodeEntry[ulIdx].RMHandle,
                           sizeof(abRMDevNode), pRMDevNode ) &&
           !_checkRMNode( &stIRQList, pRMDevNode ) )
        break;
    }

    free( pEnumData );
  }

  DosClose( hfResMgr );

  if ( stIRQList.ulSize == 0 )
  {
    if ( stIRQList.pData != NULL )
      free( stIRQList.pData );
    pIRQDescr = NULL;
  }
  else
  {
    pIRQDescr = realloc( stIRQList.pData, stIRQList.ulSize );
    if ( pIRQDescr == NULL )
      pIRQDescr = stIRQList.pData;
  }

  return pIRQDescr;
}

VOID irqFreeList(PIRQDESCR pIRQDescr)
{
  if ( pIRQDescr != NULL )
    free( pIRQDescr );
}

PSZ irqGetDescription(PIRQDESCR pIRQDescr, USHORT usIRQLevel)
{
  PSZ        pszDescription = NULL;

  if ( pIRQDescr == NULL )
    return NULL;

  while( TRUE )
  {
    if ( pIRQDescr->usIRQLevel == usIRQLevel )
    {
      pszDescription = pIRQDescr->acDescr;
      break;
    }

    if ( pIRQDescr->usNextOffs == 0 )
      break;
    pIRQDescr = (PIRQDESCR)( &((PBYTE)pIRQDescr)[pIRQDescr->usNextOffs] );
  }

  return pszDescription;
}
