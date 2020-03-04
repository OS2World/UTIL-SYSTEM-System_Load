#define INCL_DOSPROFILE
#define INCL_DOSERRORS
#define INCL_DOSMODULEMGR
#define INCL_DOSFILEMGR
#include <os2.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <ctype.h> 

#define INCR_BUF_SIZE 		(1024 * 10)

typedef QSGREC		*PQSGREC;
typedef QSPREC		*PQSPREC;
typedef QSTREC		*PQSTREC;
typedef QSPTRREC	*PQSPTRREC;
typedef QSFREC		*PPQSFREC;
typedef QSFREC		*PQSFREC;
typedef QSSFT		*PQSSFT;
typedef QSLREC		*PQSLREC;
typedef QSEXLREC	*PQSEXLREC;
typedef QSLOBJREC	*PQSLOBJREC;
typedef QSS32REC	*PQSS32REC;
typedef QSEXLREC	*PQSEXLREC;

static PQSPTRREC		pPtrRec = NULL;
static ULONG			cbPtrRec = 0;
static PSZ			pszQueryMask = NULL;
static PID			pidQuery = (PID)(-1);


static BOOL _isMatch(PSZ pszFile, PSZ pszMask)
{
  CHAR		szBuf[CCHMAXPATH];
  PCHAR		pcFileSlash;
  PCHAR		pcFilePath = pszFile;
  ULONG		cbFilePath = 0;
  PCHAR		pcMaskSlash = strrchr( pszMask, '\\' );
  PCHAR		pcMaskPath = pszMask;
  ULONG		cbMaskPath = 0;
  ULONG		ulRC;

  if ( pszFile == NULL )
    return ( pszMask == NULL ) || ( *pszMask == '*' ) || ( *pszMask == '\0' );

  pcFileSlash = strrchr( pszFile, '\\' );
  if ( pcFileSlash != NULL )
  {
    pszFile = pcFileSlash + 1;
    cbFilePath = pcFileSlash - pcFilePath;
  }

  if ( pcMaskSlash != NULL )
  {
    pszMask = pcMaskSlash + 1;
    cbMaskPath = pcMaskSlash - pcMaskPath;
  }

  if ( cbMaskPath != 0 )
  {
    if ( ( cbFilePath != cbMaskPath ) ||
         ( memicmp( pcFilePath, pcMaskPath, cbMaskPath ) != 0 ) )
      return FALSE;
  }

  if ( ( strchr( pszMask, '*' ) == NULL ) && ( strchr( pszMask, '?' ) == NULL ) )
    return ( stricmp( pszMask, pszFile ) == 0 );

  ulRC = DosEditName( 1, pszFile, pszMask, &szBuf, CCHMAXPATH );

  return ( ulRC == NO_ERROR ) && ( stricmp( &szBuf, pszFile ) == 0 );
}

static BOOL _querySysState(PULONG pcbPtrRec, PQSPTRREC *ppPtrRec,
                           ULONG ulFlags)
{
  ULONG		ulRC;

  if ( ( *ppPtrRec == NULL ) || ( *pcbPtrRec == 0 ) )
  {
    *pcbPtrRec = 0;
    goto l00;
  }

  memset( *ppPtrRec, 0, *pcbPtrRec );
  do
  {
    ulRC = DosQuerySysState( ulFlags, 0, 0, 0,  *ppPtrRec, *pcbPtrRec );
    if ( ulRC == NO_ERROR )
      break;

    if ( *ppPtrRec != NULL )
      free( *ppPtrRec );
    *ppPtrRec = NULL;

    if ( ulRC != ERROR_BUFFER_OVERFLOW )
    {
      printf( "DosQuerySysState(), rc = %u", ulRC );
      return FALSE;
    }

l00:
    *pcbPtrRec += INCR_BUF_SIZE;
    *ppPtrRec = calloc( 1, *pcbPtrRec );
    if ( *ppPtrRec == NULL )
    {
      puts( "Not enough memory" );
      return FALSE;
    }
//    printf( "Buffer size expanded to %u bytes\n", *pcbPtrRec );
  }
  while( TRUE );

  return TRUE;
}

static PQSLREC _findModule(HMODULE hModule)
{
  PQSLREC	pLRec = pPtrRec->pLibRec;

  for( pLRec = pPtrRec->pLibRec; pLRec != NULL; pLRec = pLRec->pNextRec )
    if ( pLRec->hmte == hModule )
      break;

  return pLRec;
}

static PQSPREC _findProcess(PID pid)
{
  PQSPREC	pPRec = pPtrRec->pProcRec;

  while( pPRec->RecType == QS_PROCESS )
  {
    if ( pPRec->pid == pid )
      return pPRec;

    pPRec = (PQSPREC)( (PBYTE)(pPRec->pThrdRec) +
                       ( pPRec->cTCB * sizeof(QSTREC) ) );
  }

  return NULL;
}

static VOID _catBuf(PSZ pszBuf, PSZ pszStr)
{
  ULONG		ulLen = strlen( pszBuf );

  if ( ulLen != 0 )
  {
    *((PUSHORT)&pszBuf[ulLen]) = ' ,';
    ulLen += 2;
  }

  strcpy( &pszBuf[ulLen], pszStr );
}

static VOID _printSft(PQSSFT pSft)
{
  ULONG		ulMode = *((PULONG)&pSft->mode);
  CHAR		szBuf[320];

  szBuf[0] = (pSft->attr & 0x20) == 0 ? '-' : 'A';
  szBuf[1] = (pSft->attr & 0x10) == 0 ? '-' : 'D';
  szBuf[2] = (pSft->attr & 0x08) == 0 ? '-' : 'L';
  szBuf[3] = (pSft->attr & 0x04) == 0 ? '-' : 'S';
  szBuf[4] = (pSft->attr & 0x02) == 0 ? '-' : 'H';
  szBuf[5] = (pSft->attr & 0x01) == 0 ? '-' : 'R';
  szBuf[6] = '\0';

  printf( "    Flags: 0x%X, Attributes (0x%X): %s\n",
          *((PULONG)&pSft->flags), pSft->attr, &szBuf );
  printf( "    Mode (0x%X): ", pSft->mode );
  szBuf[0] = '\0';
  if ( (ulMode & OPEN_FLAGS_DASD) != 0 )
    _catBuf( &szBuf, "DASD" );
  if ( (ulMode & OPEN_FLAGS_WRITE_THROUGH) != 0 )
    _catBuf( &szBuf, "Write through cache" );
  if ( (ulMode & OPEN_FLAGS_FAIL_ON_ERROR) != 0 )
    _catBuf( &szBuf, "Fail on error" );
  if ( (ulMode & OPEN_FLAGS_NO_CACHE) != 0 )
    _catBuf( &szBuf, "No-Cache" );
  if ( (ulMode & OPEN_FLAGS_SEQUENTIAL) != 0 )
    _catBuf( &szBuf, "Sequential access" );
  if ( (ulMode & OPEN_FLAGS_RANDOM) != 0 )
    _catBuf( &szBuf, "Random access" );
  if ( (ulMode & OPEN_FLAGS_RANDOMSEQUENTIAL) != 0 )
    _catBuf( &szBuf, "Random/locality access" );
  if ( (ulMode & OPEN_FLAGS_NOINHERIT) != 0 )
    _catBuf( &szBuf, "Private handle" );
  if ( (ulMode & OPEN_SHARE_DENYREAD) == OPEN_SHARE_DENYREAD )
    _catBuf( &szBuf, "Share: deny read" );
  else
  {
    if ( (ulMode & OPEN_SHARE_DENYWRITE ) != 0 )
      _catBuf( &szBuf, "Share: deny write" );
    if ( (ulMode & OPEN_SHARE_DENYREADWRITE ) != 0 )
      _catBuf( &szBuf, "Share: deny read/write" );
  }
  if ( (ulMode & OPEN_SHARE_DENYNONE ) != 0 )
    _catBuf( &szBuf, "Share: deny none" );
  if ( (ulMode & (OPEN_ACCESS_WRITEONLY | OPEN_ACCESS_READWRITE) ) == 0 )
    _catBuf( &szBuf, "Read only" );
  else
  {
    if ( (ulMode & OPEN_ACCESS_WRITEONLY ) != 0 )
      _catBuf( &szBuf, "Write only" );
    if ( (ulMode & OPEN_ACCESS_READWRITE ) != 0 )
      _catBuf( &szBuf, "Read/write" );
  }
  puts( &szBuf );
}

static ULONG _showProcesses()
{
  static PSZ	apszTypes[] = { "fullscreen", "DOS", "VIO", "PM", "detached" };
  PQSPREC	pPRec = pPtrRec->pProcRec;
  PQSPREC	pPRecNext, pPRecParent;
  PQSLREC	pLRec;
  PQSTREC	pTRec;
  PQSFREC	pFRec;
  PQSSFT	pSft;
  ULONG		cFH;
  PSZ		pszMsg;
  ULONG		ulIdx, ulIdxFile;
  PUSHORT	pusFN;
  ULONG		ulFN;
  BOOL		fFound;
  CHAR		szBuf[32];
  ULONG		ulCount = 0;

  while( pPRec->RecType == QS_PROCESS )
  {
    pPRecNext = (PQSPREC)( (PBYTE)(pPRec->pThrdRec) +
                           ( pPRec->cTCB * sizeof(QSTREC) ) );

    // Name of file.

    pLRec = _findModule( pPRec->hMte );

    if ( ( ( pszQueryMask != NULL ) &&
           ( ( pLRec == NULL ) || !_isMatch( pLRec->pName, pszQueryMask ) ) )
         ||
         ( ( pidQuery != (PID)(-1) ) && ( pidQuery != pPRec->pid ) ) )
    {
      pPRec = pPRecNext;
      continue;
    }

    printf( "Process (PID 0x%X): ", pPRec->pid );
    if ( pLRec != NULL )
      printf( "%s\n", pLRec->pName );
    else
      puts( "Module not found!" );

    // Parent process.
        
    pPRecParent = _findProcess( pPRec->ppid );
    if ( pPRecParent == NULL )
      pszMsg = "Process not found!";
    else
    {
      PQSLREC	pLRecParent = _findModule( pPRecParent->hMte );

      if ( pLRecParent != NULL )
        pszMsg = pLRecParent->pName;
      else
        pszMsg = "Module not found!";
    }

    printf( "  Parent (PID 0x%X): %s\n", pPRec->ppid, pszMsg );

    // Information fields.

    // Kernel bug: QSPREC.cFH is not valid. We calculate this value.
    cFH = ( (PBYTE)pPRec->pThrdRec - (PBYTE)pPRec->pFSRec ) >> 1;

    if ( pPRec->type < sizeof(apszTypes) / sizeof(PSZ) )
      pszMsg = apszTypes[ pPRec->type ];
    else
    {
      sprintf( &szBuf, "UNKNOWN (%u)", pPRec->type );
      pszMsg = &szBuf;
    }

    printf( "  Type: %s, Status: 0x%X, "
            "Screen group: %u,\n  Module handle: %u, "
            "Max. files: %u\n\n",
            pszMsg, pPRec->stat, pPRec->sgid, pPRec->hMte, cFH );

    // Threads.

    printf( "  Threads (%u):\n", pPRec->cTCB );
    pTRec = pPRec->pThrdRec;
    for( ulIdx = 0; ulIdx < pPRec->cTCB; ulIdx++, pTRec++ )
    {
      switch( pTRec->state )
      {
        case 1:
          pszMsg = "ready";
          break;

        case 2:
          pszMsg = "blocked";
          break;

        case 5:
          pszMsg = "running";
          break;

        case 9:
          pszMsg = "loaded";
          break;

        default:
          sprintf( &szBuf, "UNKNOWN (%u)", pTRec->state );
          pszMsg = &szBuf;
      }

      printf( "    TID: 0x%X, Priority: 0x%X, State: %s\n",
              pTRec->tid, pTRec->priority, pszMsg );
    }

    // Files.

    puts( "\n  Files:" );
    pusFN = pPRec->pFSRec;
    for( ulIdx = 0; ulIdx < cFH; ulIdx++, pusFN++ )
    {
      ulFN = *pusFN;                     // File number in table of files.
      if ( ulFN == 0xFFFF || ulFN == 0 ) // 0xFFFF - handle closed, 0 - skip rec.
        continue;

      // Search file in table by number.
      pFRec = pPtrRec->pFSRec;
      fFound = FALSE;
      while( ( pFRec != NULL ) && ( pFRec->RecType == QS_FILESYS ) )
      {
        pSft = pFRec->pSft;

        for( ulIdxFile = 0; ulIdxFile < pFRec->ctSft; ulIdxFile++, pSft++ )
        {
          if ( pSft->sfn == ulFN )
          {
            fFound = TRUE;
            break;
          }
        }

        if ( fFound )
          break;
        pFRec = pFRec->pNextRec;
      }

      if ( !fFound )
        continue;

      printf( "    Handle: %u, Name: %s\n",
              ulIdx, &((PCHAR)(pFRec))[sizeof(QSFREC)] );
      _printSft( pSft );
    }

    // Modules.

    if ( pLRec->ctImpMod != 0 )
    {
      printf( "\n  Modules (%u):\n", pLRec->ctImpMod );
      pusFN = (PUSHORT)( (PBYTE)&pLRec->pName + 4 );
      cFH = pLRec->ctImpMod;
      for( ulIdx = 0; ulIdx < cFH; ulIdx++, pusFN++ )
      {
        printf( "    Handle: %4.u, Name: ", *pusFN );

        pLRec = _findModule( *pusFN );
        puts( pLRec != NULL ? pLRec->pName : "Module %u not found!" );
      }
    }

    // Next record.

    puts( "" );
    pPRec = pPRecNext;
    ulCount++;
  }

  return ulCount;
}

static ULONG _showFiles()
{
  PQSFREC	pFRec = pPtrRec->pFSRec;
  PQSSFT	pSft;
  PQSPREC	pPRec;
  PQSLREC	pLRec;
  PSZ		pszName;
  ULONG		ulIdx, ulIdxFN;
  ULONG		cFH;
  PUSHORT	pusFN;
  BOOL		fFound;
  ULONG		ulCount = 0;

  // Scan all file records.
  while( ( pFRec != NULL ) && ( pFRec->RecType == QS_FILESYS ) )
  {
    pszName = &((PCHAR)(pFRec))[sizeof(QSFREC)];

    if ( ( pszQueryMask != NULL ) &&
         ( ( pLRec == NULL ) || !_isMatch( pszName, pszQueryMask ) ) )
    {
      pFRec = pFRec->pNextRec;
      continue;
    }

    printf( "Name: %s\n", pszName );
    pSft = pFRec->pSft;

    // Scan SFT-records for file.
    for( ulIdx = 0; ulIdx < pFRec->ctSft; ulIdx++, pSft++ )
    {
      // Looking for for the process that opened file.

      pPRec = pPtrRec->pProcRec;
      fFound = FALSE;
      while( pPRec->RecType == QS_PROCESS )
      {
        // Kernel bug: QSPREC.cFH is not valid. We calculate this value.
        cFH = ( (PBYTE)pPRec->pThrdRec - (PBYTE)pPRec->pFSRec ) >> 1;
        pusFN = pPRec->pFSRec;

        // Scan handles table of process.
        for( ulIdxFN = 0; ulIdxFN < cFH; ulIdxFN++, pusFN++ )
        {
          if ( *pusFN != pSft->sfn )
            continue;

          // Found!
          printf( "  Process (PID 0x%X): ", pPRec->pid );

          pLRec = _findModule( pPRec->hMte );
          if ( pLRec != NULL )
            printf( "%s\n", pLRec->pName );
          else
            puts( "Module not found!" );

          fFound = TRUE;
          break;
        }

        // One (device?) handle (fn) may be used by multiple processes. Is why
        // continues to scan even if process for this handle is found.
        pPRec = (PQSPREC)( (PBYTE)(pPRec->pThrdRec) +
                           ( pPRec->cTCB * sizeof(QSTREC) ) );
      }
   
      if ( !fFound )
        printf( "  Process for handle (sfn) %u not found.\n", pSft->sfn );

      _printSft( pSft );
    }

    puts( "" );
    pFRec = pFRec->pNextRec;
    ulCount++;
  }

  return ulCount;
}

static ULONG _showModules()
{
  PQSLREC	pLRec;
  PQSLREC	pLRecImp;
  ULONG		ulIdx;
  HMODULE	hModule;
  ULONG		ulCount = 0;

  for( pLRec = pPtrRec->pLibRec; pLRec != NULL; pLRec = pLRec->pNextRec )
  {
    if ( ( pszQueryMask != NULL ) && !_isMatch( pLRec->pName, pszQueryMask ) )
      continue;

    printf( "Module (hdl %u, %sbit): %s\n",
            pLRec->hmte, pLRec->fFlat == 0 ? "16" : "32", pLRec->pName );

    if ( pLRec->ctImpMod )
    {
      printf( "  Imported modules (%u):\n", pLRec->ctImpMod );
      for( ulIdx = 0; ulIdx < pLRec->ctImpMod; ulIdx++ )
      {
        hModule = ((PUSHORT)( (PBYTE)&pLRec->pName + 4 ))[ulIdx];
        printf( "    Handle: %u, ",  hModule );

        pLRecImp = _findModule( hModule );
        puts( pLRecImp == NULL ? "Module not found." : pLRecImp->pName );
      }
    }

    puts( "" );
    ulCount++;
  }

  return ulCount;
}

static ULONG _showExModules()
{
  PQSEXLREC	pExLRec;
  PQSEXLREC	pExLRecImp;
  ULONG		ulIdx;
  PUSHORT	phModule;
  ULONG		ulCount = 0;

  for( pExLRec = (PQSEXLREC)pPtrRec; pExLRec != NULL; pExLRec = pExLRec->next )
  {
    if ( ( ( pszQueryMask != NULL ) && !_isMatch( pExLRec->name, pszQueryMask ) )
         || ( ( pidQuery != (PID)(-1) ) && ( pidQuery != pExLRec->pid ) ) )
      continue;

    printf( "Module (hdl %u, %sbit, PID 0x%X): %s\n",
            pExLRec->hndmod, pExLRec->type == 0 ? "16" : "32",
            pExLRec->pid, pExLRec->name );

    if ( pExLRec->refcnt != 0 )
    {
      printf( "  Imported modules (%u):\n", pExLRec->refcnt );
      phModule = (PUSHORT)( (PBYTE)pExLRec + sizeof(QSEXLREC) );
      for( ulIdx = 0; ulIdx < pExLRec->refcnt; ulIdx++ )
      {
        for( pExLRecImp = (PQSEXLREC)pPtrRec; pExLRecImp != NULL;
             pExLRecImp = pExLRecImp->next )
        {
          if ( pExLRecImp->hndmod == *phModule )
            break;
        }

        printf( "    Handle: %u, %s\n", *phModule,
                pExLRecImp == NULL ? "Module not found." :
                                     pExLRecImp->name );
        phModule++;
      }
    }

    puts( "" );
    ulCount++;
  }

  return ulCount;
}

/*
Снова ядерная бага: pPtrRec->p32SemRec всегда NULL

static VOID _showSem32()
{
  PQSS32REC	pS32Rec = pPtrRec->p32SemRec;

  while( pS32Rec != NULL && pS32Rec->flags == QS_SEMAPHORE )
  {
    printf( "Name: %s\n", pS32Rec->pName );
    printf( "SHUN record v.: %u, blockid: %u, index: %u, OpenCt: %u\n",
            pS32Rec->qsh,
            pS32Rec->blockid,
            pS32Rec->index,
            pS32Rec->OpenCt );

    pS32Rec = (PQSS32REC)pS32Rec->pNextRec;
  }
}
*/

int main(int argc, char *argv[])
{
  int		iCh;
  BOOL		fError = FALSE;
  BOOL		fProcess = FALSE;
  BOOL		fFiles = FALSE;
  BOOL		fModules = FALSE;
  BOOL		fExModules = FALSE;

  puts( "System Load - System Status utility.\n" );

  while( TRUE )
  {
    iCh = getopt( argc, argv, ":pfmei:w:" );
    if ( iCh == -1 )
      break;

    switch( iCh )
    {
      case 'p':
        fProcess = TRUE;
        break;

      case 'f':
        fFiles = TRUE;
        break;

      case 'm':
        fModules = TRUE;
        break;

      case 'e':
        fExModules = TRUE;
        break;

      case 'i':
        {
          PCHAR		pEnd;

          pidQuery = strtoul( optarg, &pEnd,
                              memicmp( optarg, "0x", 2 ) == 0 ? 16 : 10 );
          fError = pEnd == optarg;
        }
        break;

      case 'w':
        {
          pszQueryMask = optarg;
          while( isspace( *pszQueryMask ) )
            pszQueryMask++;
          fError = *pszQueryMask == '\0';
        }
        break;

      case '?':
      case ':':
        fError = TRUE;
        break;
    }

    if ( fError )
      break;
  }

  if ( fError || ( !fProcess && !fFiles && !fModules && !fExModules ) )
  {
    printf( "Syntax:\n  %s <-pfme> [-i pid] [-w mask]\nwhere:\n",
            strrchr( argv[0], '\\' ) + 1 );
    puts( "  -p        Processes.\n"
          "  -f        Files.\n"
          "  -m        Modules.\n"
          "  -e        Modules (extended system query).\n"
          "  -i NNN    Specifies the process ID (for -p and -e).\n"
          "  -w mask   Specifies files to list.\n" );
    return 2;
  }

  if ( ( fProcess || fFiles || fModules ) &&
       !_querySysState( &cbPtrRec, &pPtrRec, QS_PROCESS | QS_FILESYS | QS_MTE ) )
    return 1;

  if ( fProcess )
  {
    puts( "  [ Processes ]\n" );
    printf( "Processes listed: %u\n\n", _showProcesses() );
  }

  if ( fFiles )
  {
    puts( "  [ Files ]\n" );
    printf( "Files listed: %u\n\n", _showFiles() );
  }

  if ( fModules )
  {
    puts( "  [ Modules ]\n" );
    printf( "Modules listed: %u\n\n", _showModules() );
  }

  if ( fExModules )
  {
    puts( "  [ Modules ]\n" );
    if ( !_querySysState( &cbPtrRec, &pPtrRec, QS_MODVER ) )
      return 1;
    printf( "Modules listed: %u\n\n", _showExModules() );
  }

puts( "Done." );
  return 0;
}
