#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSFILEMGR   /* File manager values */
#define INCL_DOSERRORS    /* DOS error values    */
#define INCL_DOSMISC
#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <ds.h>		// type ULLONG
#include "diskutil.h"
#include <debug.h>


#pragma pack(1)

typedef struct _BPB {
    USHORT      usBytesPerSector;       // Number of bytes per sector
    BYTE        bSectorsPerCluster;     // Number of sectors per cluster
    USHORT      usReservedSectors;      // Number of reserved sectors
    BYTE        cFATs;                  // Number of FATs
    USHORT      cRootEntries;           // Number of root directory entries
    USHORT      cSectors;               // Number of sectors
    BYTE        bMedia;                 // Media descriptor
    USHORT      usSectorsPerFAT;        // Number of secctors per FAT
    USHORT      usSectorsPerTrack;      // Number of sectors per track
    USHORT      cHeads;                 // Number of heads
    ULONG       cHiddenSectors;         // Number of hidden sectors
    ULONG       cLargeSectors;          // Number of large sectors
    BYTE        abReserved[6];          // Reserved
} BPB;

typedef struct _IOCTLPAR {
    UCHAR       command;
    USHORT      unit;
} IOCTLPAR;

typedef struct _IOCTLDATA {
    BPB         bpb;
    USHORT      cylinders;
    UCHAR       devtype;
    USHORT      devattr;
} IOCTLDATA;

#pragma pack()


BOOL duDiskInfo(ULONG ulDrv, PDISKINFO pInfo)
{
  ULONG			ulRC;
  ULONG			cbData;
  #pragma pack(1)
  struct {
    UCHAR	ucCmd;
    UCHAR	ucDrv;
  }			stParms;
  #pragma pack()
  ULONG			cbParms;
  BYTE			bFixed;
  BIOSPARAMETERBLOCK	stBPB;
  CHAR			szDrv[] = "C:";
  BYTE			abBuf[sizeof(FSQBUFFER2) + (3 * CCHMAXPATH)] = { 0 };
  PFSQBUFFER2		pBuf = (PFSQBUFFER2)&abBuf;
  FSINFO		stFSInfo;
  FSALLOCATE		stFSAllocate;
  static ULONG		ulBootDrv = ((ULONG)(-1));


  szDrv[0] = 'A' + ulDrv;
  memset( pInfo, 0, sizeof(DISKINFO) );

  // Check boot drive.

  if ( ulBootDrv == ((ULONG)(-1)) )
  {
    // Query boot drive only first time.
    ulRC = DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                            &ulBootDrv, sizeof(ULONG) );
    if ( ulRC != NO_ERROR )
      debug( "DosQuerySysInfo() fail" );
    else
      ulBootDrv--; // DosQuerySysInfo() returns 1 for A: - decrement result.
  }
  pInfo->fBoot = ulDrv == ulBootDrv;
  pInfo->fRemovable = FALSE;

  // Query drive type and check if media present.

  stParms.ucDrv = ulDrv;

  // Is fixed disk (bFixed) ?
  ulRC = DosDevIOCtl( (HFILE)(-1), IOCTL_DISK, DSK_BLOCKREMOVABLE,
                      &stParms, sizeof(stParms), &cbParms,
                      &bFixed, sizeof(BYTE), &cbData );

  if ( ulRC == NO_ERROR )
  {
    if ( bFixed == 0 )
    {
      // Check if media present.
      HFILE	hFile = NULLHANDLE;

      pInfo->fPresent = DosOpen( &szDrv, &hFile, &cbData, 0, FILE_NORMAL,
                                 OPEN_ACTION_OPEN_IF_EXISTS,
                                 OPEN_FLAGS_DASD | OPEN_FLAGS_FAIL_ON_ERROR |
                                 OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYNONE,
                                 NULL ) == NO_ERROR;
      if ( hFile != NULLHANDLE )
        DosClose( hFile );

      pInfo->ulType = DI_TYPE_UNKNOWN;
      pInfo->fRemovable = TRUE;
    }
    else
    {
      pInfo->fPresent = TRUE;
      pInfo->ulType = DI_TYPE_HDD;
    }

    stParms.ucCmd = 0; 
    ulRC = DosDevIOCtl( (HFILE)(-1), IOCTL_DISK, DSK_GETDEVICEPARAMS,
                        &stParms, sizeof(stParms), &cbParms,
                        &stBPB, sizeof(stBPB), &cbData );
    if ( ulRC == NO_ERROR )
    {
      if ( (stBPB.fsDeviceAttr & 0x08) != 0 ) // Removable flag (flash drive?)
      {
        pInfo->fRemovable = TRUE;
        pInfo->ulType = DI_TYPE_HDD;
      }
      else if ( ( stBPB.bDeviceType == 7 ) &&
                ( stBPB.usBytesPerSector == 2048 ) &&
                ( stBPB.usSectorsPerTrack == (USHORT)(-1) ) )
        pInfo->ulType = DI_TYPE_CDDVD;
      else
      {
        switch ( stBPB.bDeviceType )
        {
          case DEVTYPE_TAPE:      // 6
            pInfo->ulType = DI_TYPE_TAPE;
            break;

          case DEVTYPE_UNKNOWN:
            if ( ( stBPB.usBytesPerSector == 2048 ) &&
                 ( stBPB.usSectorsPerTrack == (USHORT)(-1) ) )
            {
              pInfo->ulType = DI_TYPE_CDDVD;
              break;
            }
            // 7, 1.44/1.84M  3.5" floppy ?
          case DEVTYPE_48TPI:     // 0, 360k  5.25" floppy
          case DEVTYPE_96TPI:     // 1, 1.2M  5.25" floppy
          case DEVTYPE_35:        // 2, 720k  3.5" floppy
          case 9:                 // 9, 288MB 3.5" floppy?
            pInfo->ulType = ulDrv <= 1 ? DI_TYPE_FLOPPY : DI_TYPE_VDISK;
            break;

          case 8:                 // CD/DVD?
            if ( ( stBPB.usBytesPerSector == 2048 ) &&
                 ( stBPB.usSectorsPerTrack == (USHORT)(-1) ) )
              pInfo->ulType = DI_TYPE_CDDVD;
            break;
        }
      }
    }
  }
  else if ( ulRC == ERROR_NOT_SUPPORTED )
  {
    pInfo->ulType = DI_TYPE_REMOTE;
    pInfo->fPresent = TRUE;
  }
  else if ( ulRC == ERROR_INVALID_DRIVE )
  {
    pInfo->ulType = DI_TYPE_UNKNOWN;
    pInfo->fPresent = FALSE;
  }
  else
    return FALSE; // no drive

  // Query file system.

  cbData = sizeof(abBuf);
  ulRC = DosQueryFSAttach( &szDrv, 0, FSAIL_QUERYNAME, pBuf, &cbData );

  if ( ulRC != NO_ERROR )
  {
    debug("DosQueryFSAttach error: return code = %u\n", ulRC );
  }
  else
  {
    pInfo->cbFSDName = pBuf->cbFSDName;
    memcpy( &pInfo->szFSDName, pBuf->szName+pBuf->cbName+1, pBuf->cbFSDName+1 );
  }

  // Query label.

  // for DosQueryFSInfo() - A: is 1, B: is 2... 
  ulDrv++;

  ulRC = DosQueryFSInfo( ulDrv, FSIL_VOLSER, &stFSInfo, sizeof(FSINFO) );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosQueryFSInfo(%u,,,), rc = %u\n", ulDrv, ulRC );
  }
  else
  {
    memcpy( &pInfo->szLabel, &stFSInfo.vol.szVolLabel, stFSInfo.vol.cch + 1 );

    // fat32 always gives me length of the label (stFSInfo.vol.cch) equal 11.
    // So we will use strlen().
//    pInfo->cbLabel = stFSInfo.vol.cch;
    pInfo->cbLabel = strlen( &stFSInfo.vol.szVolLabel );
  }

  // Query size.

  ulRC = DosQueryFSInfo( ulDrv, FSIL_ALLOC, &stFSAllocate, sizeof(FSALLOCATE) );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosQueryFSInfo(), rc = %u\n", ulRC );
  }
  else
  {
    ULONG	ullUnitBytes = stFSAllocate.cbSector * stFSAllocate.cSectorUnit;

    pInfo->ullTotal = ullUnitBytes * (ULLONG)stFSAllocate.cUnit;
    pInfo->ullFree = ullUnitBytes * (ULLONG)stFSAllocate.cUnitAvail;
  }

  return TRUE;
}

BOOL duEject(ULONG ulDrv, BOOL fUnlock)
{
  IOCTLPAR	stParam;
  IOCTLDATA	stData;
  ULONG		ulRC;
  ULONG		cbParam = sizeof(stParam);
  ULONG		cbData = sizeof(stData);

  stParam.unit = ulDrv;

  if ( fUnlock )
  {
    // Unlock media

    stParam.command = 0;
    ulRC = DosDevIOCtl( -1, IOCTL_DISK, DSK_UNLOCKEJECTMEDIA, // DSK_UNLOCKDRIVE?
                        &stParam, sizeof(stParam), &cbParam,
                        &stData, sizeof(stData), &cbData );

    stParam.unit = ulDrv;
    cbParam = sizeof(stParam);
    cbData = sizeof(stData);
  }

  // Eject media

  stParam.command = 2;
  ulRC = DosDevIOCtl( -1, IOCTL_DISK, DSK_UNLOCKEJECTMEDIA,
                      &stParam, sizeof(stParam), &cbParam,
                      &stData, sizeof(stData), &cbData );

  return ulRC == NO_ERROR;
} 
