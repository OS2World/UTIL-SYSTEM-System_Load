#define INCL_DOSERRORS    /* DOS error values */
#define INCL_DOSMODULEMGR
#define INCL_DOSDEVICES   /* Device values */
#define INCL_DOSPROCESS
#include <acpi.h>
#include <accommon.h>
#include <acpiapi.h>
#include <stdio.h>
#include <string.h>
#include <os2.h>
#include <ds.h>		// ULLONG
#include "termzone.h"
#include <debug.h>


ACPI_STATUS APIENTRY (*pAcpiTkValidateVersion)(UINT32 TestMajor, UINT32 TestMinor);
ACPI_STATUS APIENTRY (*pAcpiTkEvaluateObject)(ACPI_HANDLE Object, ACPI_STRING Pathname,
    ACPI_OBJECT_LIST *ParameterObjects, ACPI_BUFFER *ReturnObjectBuffer);
const char * APIENTRY (*pAcpiTkStatusToStr)(ACPI_STATUS Status);

static HMODULE		hAcpi32 = NULLHANDLE;	// acpi32.dll module handle.


BOOL tzInit()
{
  CHAR		szErr[256];
  ULONG		ulRC;

  ulRC = DosLoadModule( &szErr, sizeof(szErr), "acpi32.dll", &hAcpi32 );
  if ( ulRC != NO_ERROR )
  {
    debug( "DosLoadModule(), rc = %u, err: %s", ulRC, &szErr );
    hAcpi32 = NULLHANDLE;
    return FALSE;
  }

  do
  {
    ulRC = DosQueryProcAddr( hAcpi32, 0, "AcpiTkValidateVersion",
                             (PFN *)&pAcpiTkValidateVersion );
    if ( ulRC != NO_ERROR )
      break;

    ulRC = DosQueryProcAddr( hAcpi32, 0, "AcpiTkEvaluateObject",
                             (PFN *)&pAcpiTkEvaluateObject );
    if ( ulRC != NO_ERROR )
      break;

    ulRC = DosQueryProcAddr( hAcpi32, 0, "AcpiTkStatusToStr",
                             (PFN *)&pAcpiTkStatusToStr );
    if ( ulRC != NO_ERROR )
      break;

    return TRUE;
  }
  while( FALSE );

  debug( "Entry not found." );
  DosFreeModule( hAcpi32 );
  hAcpi32 = NULLHANDLE;

  return FALSE;
}

VOID tzDone()
{
  if ( hAcpi32 != NULLHANDLE )
  {
    DosFreeModule( hAcpi32 );
    hAcpi32 = NULLHANDLE;
  }
}


ULONG tzQuery(PSZ pszPathname, ULONG ulMU, PULONG pulResult)
{
  ACPI_OBJECT		aObject[4];
  ACPI_BUFFER		stResults;
  ACPI_STATUS		status;
  ULONG			ulIdx;
  BOOL			fFound = FALSE;
  UINT64		Value;

  if ( ( hAcpi32 == NULLHANDLE ) ||
       pAcpiTkValidateVersion( ACPI_TK_VERSION_MAJOR, ACPI_TK_VERSION_MINOR ) !=
       AE_OK )
    return TZ_ACPI_INCOMP_VER;

  stResults.Length = sizeof(aObject);
  stResults.Pointer = &aObject;

  status = pAcpiTkEvaluateObject( NULL, pszPathname, NULL, &stResults );
  if ( status != AE_OK )
  {
    switch( status )
    {
      case AE_BAD_PATHNAME:	// An invalid character was found in a pathname
        return TZ_BAD_PATHNAME;
      case AE_NOT_FOUND:	// The name was not found in the namespace
        return TZ_NOT_FOUND;
      case AE_TYPE:		// The object type is incorrect
        return TZ_INVALID_TYPE;
    }

    debug( "AcpiTkEvaluateObject(), rc = 0x%X: %s\n",
           status, pAcpiTkStatusToStr( status ) );
    return TZ_ACPI_EVALUATE_FAIL;
  }

  for( ulIdx = 0; ulIdx < stResults.Length / sizeof(ACPI_OBJECT); ulIdx++ )
  {
    if ( aObject[ulIdx].Type == ACPI_TYPE_INTEGER )
    {
      fFound = TRUE;
      break;
    }
  }

  if ( !fFound )
  {
    debug( "Value type integer not found." );
    return TZ_INVALID_TYPE;
  }

  Value = aObject[ulIdx].Integer.Value;

  if ( ( Value >= 2732 ) && ( Value < 4732 ) )
  {
    // Standart - kelvin *10.
    *pulResult = ulMU == TZ_MU_CELSIUS ?
                 ( Value - 2732 ) : ( 9 * ( Value - 2732 ) / 5 + 320 );
    return TZ_OK;
  }

  if ( /*( Value >= 0 ) &&*/ ( Value < 2000 ) )
  {
    // Not standart - celsius *10 ?
    *pulResult = ulMU == TZ_MU_CELSIUS ? Value : ( 9 * Value / 5 + 320 );
    return TZ_OK;
  }

  debug( "Not valid value: %llu", Value );
  return TZ_INVALID_VALUE;
}
