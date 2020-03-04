#define INCL_DOSERRORS    /* DOS error values */
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bldlevel.h>
#include <debug.h>

int main(int argc, char *argv[])
{
  BLDLEVELINFO	stInfo;
  ULONG		ulRC;

  puts( "System Load - GETVER utility.\n" );

  if ( argc != 2 )
  {
    printf( "Syntax:\n  %s <file>\n", strrchr( argv[0], '\\' ) + 1 );
    return 2;
  }

  debugInit();
  ulRC = blGetFromFile( argv[1], &stInfo );
  debugDone();

  if ( ulRC != NO_ERROR )
  {
    PSZ		pszError;
    CHAR	acCode[16];

    switch( ulRC )
    {
      case ERROR_OPEN_FAILED:
        pszError = "File open failed.";
        break;

      case ERROR_ACCESS_DENIED:
        pszError = "Access denied.";
        break;

      case ERROR_INVALID_DRIVE:
      case ERROR_FILENAME_EXCED_RANGE:
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
        pszError = "File not found.";
        break;

      case ERROR_BAD_FORMAT:
        pszError = "Signature not found.";
        break;

      default:
        *((PULONG)&acCode) = (ULONG)' :CR';
        ultoa( ulRC, &acCode[4], 10 );
        pszError = &acCode;
    }

    puts( pszError );
    return 1;
  }

  printf( "Signature type: %u\n", stInfo.ulType );

  if ( stInfo.cbVendor != 0 )
    printf( "Vendor: %s\n", &stInfo.acVendor );

  if ( stInfo.cbRevision != 0 )
    printf( "Revision: %s\n", &stInfo.acRevision );

  if ( stInfo.cbDateTime != 0 )
    printf( "Date/Time: %s\n", &stInfo.acDateTime );

  if ( stInfo.cbBuildMachine != 0 )
    printf( "Build Machine: %s\n", &stInfo.acBuildMachine );

  if ( stInfo.cbASDFeatureID != 0 )
    printf( "ASD Feature ID: %s\n", &stInfo.acASDFeatureID );

  if ( stInfo.cbLanguageCode != 0 )
    printf( "Language Code: %s\n", &stInfo.acLanguageCode );

  if ( stInfo.cbCountryCode != 0 )
    printf( "Country Code: %s\n", &stInfo.acCountryCode );

  if ( stInfo.cbBuild != 0 )
    printf( "Build: %s\n", &stInfo.acBuild );

  if ( stInfo.cbUnknown != 0 )
    printf( "Unknown: %s\n", &stInfo.acUnknown );

  if ( stInfo.cbFixPackVer != 0 )
    printf( "FixPack Ver: %s\n", &stInfo.acFixPackVer );

  if ( stInfo.cbDescription != 0 )
    printf( "Description: %s\n", &stInfo.acDescription );

  return 0;
}
