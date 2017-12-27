#define DI_TYPE_UNKNOWN		0
#define DI_TYPE_HDD		1
#define DI_TYPE_CDDVD		2
#define DI_TYPE_TAPE		3
#define DI_TYPE_FLOPPY		4
#define DI_TYPE_VDISK		5
#define DI_TYPE_REMOTE		6

#define DiskInfoSize(pdi) (sizeof(DISKINFO) - CCHMAXPATH + (pdi)->cbFSDName + 1)

typedef struct _DISKINFO {
  ULONG		ulType;			// DI_TYPE_*
  BOOL		fBoot;			// Boot drive flag.
  BOOL		fPresent;
  BOOL		fRemovable;
  ULONG		cbLabel;		// Length of szLabel (without '\0').
  CHAR		szLabel[12];		// Volume label.
  ULLONG	ullTotal;		// Total space [bytes].
  ULLONG	ullFree;		// Free space [bytes].
  ULONG		cbFSDName;		// Length of szFSDName (without '\0').
  CHAR		szFSDName[CCHMAXPATH];	// Name of the file-system driver.
} DISKINFO, *PDISKINFO;

BOOL duDiskInfo(ULONG ulDrv, PDISKINFO pInfo);
BOOL duEject(ULONG ulDrv, BOOL fUnlock);
