#define INCL_ERRORS
#define INCL_RXMACRO              /* include macrospace info         */
#define INCL_RXFUNC               /* include external function  info */
#define INCL_REXXSAA
#include <os2.h>
#include <rexxsaa.h>
#include <string.h>
#include <bldlevel.h>	// ..\getver\bldlevel.h
#include <stdlib.h>	// ultoa()
#include <stdio.h>	// _snprintf()

#define INVALID_ROUTINE		40           /* Raise Rexx error           */
#define VALID_ROUTINE		0            /* Successful completion      */

RexxFunctionHandler gvLoadFuncs;
RexxFunctionHandler gvDropFuncs;
RexxFunctionHandler gvParse;
RexxFunctionHandler gvGetFromFile;

static PSZ RxFncTable[] =
{
  "gvLoadFuncs",
  "gvDropFuncs",
  "gvParse",
  "gvGetFromFile"
};

#define BUILDRXSTRING(t, s) { \
  strcpy((t)->strptr,(s));\
  (t)->strlength = strlen((s)); \
}


static VOID _setRxValue(PSZ pszName, ULONG cbValue, PCHAR pcValue)
{
  SHVBLOCK	sBlock;

  sBlock.shvnext = NULL;
  MAKERXSTRING( sBlock.shvname, pszName, strlen( pszName ) );
  MAKERXSTRING( sBlock.shvvalue, pcValue, cbValue );
  sBlock.shvnamelen = sBlock.shvname.strlength;
  sBlock.shvvaluelen = sBlock.shvvalue.strlength;
  sBlock.shvcode = RXSHV_SET;
  sBlock.shvret = 0;

  RexxVariablePool( &sBlock );
}

static VOID _infoToStem(PBLDLEVELINFO pInfo, RXSTRING rxStem)
{
  CHAR			acName[256];
  ULONG			ulStemLen;

  ulStemLen = RXSTRLEN( rxStem );

  strcpy( &acName, RXSTRPTR( rxStem ) );
  strupr( &acName );
  if ( acName[ulStemLen - 1] != '.' )
    acName[ulStemLen++] = '.';

  strcpy( &acName[ulStemLen], "_BL_VENDOR" );
  _setRxValue( &acName, pInfo->cbVendor, pInfo->acVendor );
  strcpy( &acName[ulStemLen], "_BL_REVISION" );
  _setRxValue( &acName, pInfo->cbRevision, pInfo->acRevision );
  strcpy( &acName[ulStemLen], "_BL_DATETIME" );
  _setRxValue( &acName, pInfo->cbDateTime, pInfo->acDateTime );
  strcpy( &acName[ulStemLen], "_BL_BUILDMACHINE" );
  _setRxValue( &acName, pInfo->cbBuildMachine, pInfo->acBuildMachine );
  strcpy( &acName[ulStemLen], "_BL_ASDFEATUREID" );
  _setRxValue( &acName, pInfo->cbASDFeatureID, pInfo->acASDFeatureID );
  strcpy( &acName[ulStemLen], "_BL_LANGUAGECODE" );
  _setRxValue( &acName, pInfo->cbLanguageCode, pInfo->acLanguageCode );
  strcpy( &acName[ulStemLen], "_BL_COUNTRYCODE" );
  _setRxValue( &acName, pInfo->cbCountryCode, pInfo->acCountryCode );
  strcpy( &acName[ulStemLen], "_BL_BUILD" );
  _setRxValue( &acName, pInfo->cbBuild, pInfo->acBuild );
  strcpy( &acName[ulStemLen], "_BL_UNKNOWN" );
  _setRxValue( &acName, pInfo->cbUnknown, pInfo->acUnknown );
  strcpy( &acName[ulStemLen], "_BL_FIXPACKVER" );
  _setRxValue( &acName, pInfo->cbFixPackVer, pInfo->acFixPackVer );
  strcpy( &acName[ulStemLen], "_BL_DESCRIPTION" );
  _setRxValue( &acName, pInfo->cbDescription, pInfo->acDescription );
}


ULONG gvLoadFuncs(PUCHAR puchName, ULONG cArgs, RXSTRING aArgs[],
                  PSZ pszQueue, RXSTRING *prxstrRet)
{
  ULONG			ulIdx;
 
  prxstrRet->strlength = 0;
  if ( cArgs > 0 )
    return INVALID_ROUTINE;
 
  for( ulIdx = 0; ulIdx < sizeof(RxFncTable)/sizeof(PSZ); ulIdx++ )
    RexxRegisterFunctionDll( RxFncTable[ulIdx], "RXGETVER", RxFncTable[ulIdx] );

  return VALID_ROUTINE;
}

ULONG gvDropFuncs(PUCHAR puchName, ULONG cArgs, RXSTRING aArgs[],
                  PSZ pszQueue, RXSTRING *prxstrRet)
{
  ULONG			ulIdx;
 
  prxstrRet->strlength = 0;
  if ( cArgs > 0 )
    return INVALID_ROUTINE;
 
  for( ulIdx = 0; ulIdx < sizeof(RxFncTable)/sizeof(PSZ); ulIdx++ )
    RexxDeregisterFunction( RxFncTable[ulIdx] );

  return VALID_ROUTINE;
}

// gvParse( Signature, stem )
// Returns "OK" - successfully or "ERROR: Text." - error.

ULONG gvParse(PUCHAR puchName, ULONG cArgs, RXSTRING aArgs[],
              PSZ pszQueue, RXSTRING *prxstrRet)
{
  BLDLEVELINFO		stInfo;

  if ( cArgs != 2 || !RXVALIDSTRING( aArgs[0] ) || !RXVALIDSTRING( aArgs[1] ) )
    return INVALID_ROUTINE;

  if ( !blParse( RXSTRLEN( aArgs[0] ), RXSTRPTR( aArgs[0] ), &stInfo ) )
  {
    BUILDRXSTRING( prxstrRet, "ERROR: Invalid signature." );
    return VALID_ROUTINE;
  }

  _infoToStem( &stInfo, aArgs[1] );

  BUILDRXSTRING( prxstrRet, "OK" );
  return VALID_ROUTINE;
}

// gvGetFromFile( File, stem )
// Returns "OK" - successfully or "ERROR: Text." - error.

ULONG gvGetFromFile(PUCHAR puchName, ULONG cArgs, RXSTRING aArgs[],
                    PSZ pszQueue, RXSTRING *prxstrRet)
{
  BLDLEVELINFO		stInfo;
  ULONG			ulRC;

  if ( cArgs != 2 || !RXVALIDSTRING( aArgs[0] ) || !RXVALIDSTRING( aArgs[1] ) )
    return INVALID_ROUTINE;

  ulRC = blGetFromFile( RXSTRPTR( aArgs[0] ), &stInfo );
  if ( ulRC != NO_ERROR )
  {
    CHAR	acBuf[128];
    CHAR	acCode[16];
    PSZ		pszError;

    switch( ulRC )
    {
      case ERROR_OPEN_FAILED:
        pszError = "File open failed";
        break;

      case ERROR_ACCESS_DENIED:
        pszError = "Access denied";
        break;

      case ERROR_INVALID_DRIVE:
      case ERROR_FILENAME_EXCED_RANGE:
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
        pszError = "File not found";
        break;

      case ERROR_BAD_FORMAT:
        pszError = "Signature not found";
        break;

      default:
        ultoa( ulRC, &acCode, 10 );
        pszError = &acCode;
    }

    _snprintf( &acBuf, sizeof(acBuf), "ERROR: %s.", pszError );
    BUILDRXSTRING( prxstrRet, &acBuf );
    return VALID_ROUTINE;
  }

  _infoToStem( &stInfo, aArgs[1] );

  BUILDRXSTRING( prxstrRet, "OK" );
  return VALID_ROUTINE;
}
