// Utility to display the CPU temperature.
// It uses API provided by the library CPUTEMP.DLL.

#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>	// strtoul()
#include <signal.h>
#include <unistd.h> 
#include <string.h>	// strrchr()
#include "cputemp.h"

static ULONG		ulDelay = 1;

void stopSignal(int signo)
{
  ulDelay = 0;
}

int main(int argc, char *argv[])
{
  int		iCh;
  BOOL		fShowUsage = FALSE;
  ULONG		ulDelay = 1;
  ULONG		ulIdx;
  CPUTEMP	aCPUInfo[64];
  ULONG		cCPU;
  ULONG		ulRC;
  BOOL		fHaveData;

  puts( "System Load - Processor temperature monitor.\n" );

  // Parse command line switches.

  do
  {
    iCh = getopt( argc, argv, ":t:" );
    if ( iCh == -1 )
      break;

    switch( iCh )
    {
      case 't':
        {
          PCHAR		pEnd;

          ulDelay = strtoul( optarg, &pEnd, 10 );
          fShowUsage = pEnd == optarg;
        }
        break;

      case '?':
      case ':':
        fShowUsage = TRUE;
        break;
    }
  }
  while( !fShowUsage );

  if ( fShowUsage )
  {
    printf( "Syntax:\n  %s [-t seconds]\nwhere:\n",
            strrchr( argv[0], '\\' ) + 1 );
    puts( "  -t seconds  Specify the sensors querying interval.\n"
          "              Default is 1 second. If 0 seconds are specified the\n"
          "              program will write the current values and exit.\n" );
    return 1;
  }

  ulDelay *= 1000; // sec -> msec

  // Set signals to interrupt the work.
  signal( SIGINT, stopSignal ); 
  signal( SIGBREAK, stopSignal ); 

  do
  {
    ulRC = cputempQuery( sizeof(aCPUInfo) / sizeof(CPUTEMP),
                         &aCPUInfo, &cCPU );
    if ( ulRC != NO_ERROR )
    {
      switch( ulRC )
      {
        case CPUTEMP_NO_DRIVER:
          puts( "Cannot open driver RDMSR$" );
          break;

        case CPUTEMP_INIT_FAILED:
          puts( "Module initialization failed." );
          break;

        default:
          printf( "Error code: %u\n", ulRC );
      }

      return 2;
    }

    // Print prefix "Core 0/1/../N: "

    printf( "Core" );
    for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
      printf( "%c%u", ulIdx == 0 ? ' ' : '/', ulIdx );

    printf( ": " );

    // Print temperatures "NN/NN/../NN\n" or errors.

    fHaveData = FALSE;
    for( ulIdx = 0; ulIdx < cCPU; ulIdx++ )
    {
      if ( ulIdx != 0 )
        printf( "/" );

      if ( aCPUInfo[ulIdx].ulCode == CPU_OK )
      {
        printf( "%u", aCPUInfo[ulIdx].ulCurTemp );
        fHaveData = TRUE;
      }
      else
      {
        PSZ	pszStatus;

        switch( aCPUInfo[ulIdx].ulCode )
        {
          case CPU_UNSUPPORTED:
            pszStatus = "Unsupported CPU";
            break;

          case CPU_IOCTL_ERROR:
            pszStatus = "IOCTRL error";
            break;

          case CPU_INVALID_VALUE:
            pszStatus = "Invalud value received";
            break;

          case CPU_OFFLINE:
            pszStatus = "Offline";
            break;

          default:
            printf( "Error %u", aCPUInfo[ulIdx].ulCode );
            pszStatus = NULL;
        }

        if ( pszStatus != NULL )
          printf( pszStatus );
      }
    }
    puts( "" );

  }
  while ( fHaveData && ulDelay != 0 && DosSleep( ulDelay ) == NO_ERROR );
        // Pause (or exit with command line switch -t0).

  // Successful exit.
  return 0;
}
