#ifndef TERMZONE_H
#define TERMZONE_H 1

#define TZ_MU_CELSIUS		0
#define TZ_MU_FAHRENHEIT	1

#define TZ_OK			0
// tzQuery() result codes:
#define TZ_ACPI_INCOMP_VER	1
#define TZ_ACPI_EVALUATE_FAIL	2
#define TZ_INVALID_TYPE		3
#define TZ_INVALID_VALUE	4
#define TZ_BAD_PATHNAME		5
#define TZ_NOT_FOUND		6

BOOL tzInit();
VOID tzDone();
ULONG tzQuery(PSZ pszPathname, ULONG ulMU, PULONG pulResult);

#endif // TERMZONE_H
