#include <filever.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

ULONG FileVerGetVersionStringFromFile2(PSZ pszFile, PCHAR pcVersionString,
                                     ULONG ulReserved, PULONG pcbVersionString)
{
  CHAR		acBuf[4096];
  ULONG		ulRC;
  HFILE		hFile;
  ULONG		ulAction;
  ULONG		cbBuf;

  ulRC = DosOpen( pszFile, &hFile, &ulAction, 0, 0, 
                  OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE |
                  OPEN_ACCESS_READONLY, NULL );

  if ( ulRC != NO_ERROR )
    return ulRC;

  DosSetFilePtr( hFile, -(sizeof(acBuf) - 1), FILE_END, &cbBuf );

  ulRC = DosRead( hFile, &acBuf, sizeof(acBuf) - 1, &cbBuf );
  if ( ulRC == NO_ERROR )
  {
    ulRC = 3;
    if ( cbBuf > 10 )
    {
      PCHAR	pcScan = &acBuf[cbBuf - 6];

      acBuf[cbBuf] = '\0';
      while( pcScan > &acBuf )
      {
        if ( ( *pcScan == '@' ) && ( pcScan[1] == '#' ) )
        {
          if ( strstr( &pcScan[2], "#@" ) != NULL )
          {
            ULONG		ulLen = strlen( pcScan );

/*            while( ( ulLen > 0 ) && isspace( pcScan[ulLen-1] ) )
              ulLen--;
            pcScan[ulLen] = '\0';*/

            if ( strcspn( pcScan, "\r\n" ) == ulLen )
            {
              if ( *pcbVersionString <= ulLen )
              {
                *pcbVersionString = ulLen + 1;
                ulRC = ERROR_BUFFER_OVERFLOW;
              }
              else
              {
                strcpy( pcVersionString, pcScan );
                ulRC = NO_ERROR;
              }
              break;
            }
          }
        }

        pcScan--;
      }
    }
  }

  DosClose( hFile );
  return ulRC;
}

int main(int argc, char *argv[])
{
  ULONG		ulRC;
  CHAR		acVersionString[510] = { 0 };
  ULONG		cbVersionString = sizeof(acVersionString);
  FILEVERINFO	stFileVer;

  if ( argc != 2 )
  {
    puts( "Usage: GETVER.EXE <file>" );
    return 1;
  }

  ulRC = FileVerGetVersionStringFromFile2(
           argv[1],          // An ASCIIZ string that contains the file name.
           &acVersionString, // Buffer where the version string will be stored.
           0,                // Unknown.
           &cbVersionString  // Pointer to the length of the buffer.
         );

  if ( ulRC != NO_ERROR )
  {
    switch( ulRC )
    {
      case 3:
        printf( "File %s has no version string.\n", argv[1] );
        break;

      case ERROR_BAD_PATHNAME:
        printf( "Incorrect path/filename %s\n", argv[1] );
        break;

      default:
        printf( "Unknown return code %u from FileVerGetVersionStringFromFile\n",
                ulRC );
    }
    return 1;
  }

  printf( "Signature: %s\n\n", &acVersionString );

  ulRC = FileVerParseVersionString(
          &acVersionString, // Version string.
          1,                // Level of file information required(?) Must be 1.
          0,                // Unknown.
          &stFileVer        // A pointer to the FILEVERINFO.
         );

  if ( ulRC != NO_ERROR )
  {
    if ( ulRC == 4 )
      puts( "Version string cannot be parsed." );
    else
      printf( "FileVerParseVersionString(), rc = %u\n", ulRC );
    return 1;
  }

  printf( "Revision: %u.%u.%u\n",
          stFileVer.ulMajor, stFileVer.ulMinor, stFileVer.ulRevision );
  printf( "Vendor: %s\n", &stFileVer.acVendor );
  printf( "Date/Time: %s\n", &stFileVer.acDateTime );
  printf( "Build Machine: %s\n", &stFileVer.acBuildMachine );
  printf( "ASD Feature ID: %s\n", &stFileVer.acASDFeatureID );
  printf( "Language Code: %s\n", &stFileVer.acLanguageCode );
  printf( "Country Code: %s\n", &stFileVer.acCountryCode );
  printf( "File Version: %s\n", &stFileVer.acFileVersion );
  printf( "Description: %s\n", &stFileVer.acDescription );

  return 0;
}
