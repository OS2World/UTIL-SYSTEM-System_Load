#define PT_MU_CELSIUS		0
#define PT_MU_FAHRENHEIT	1

#define PT_OK			0
#define PT_ACPI_INCOMP_VER	1
#define PT_ACPI_EVALUATE_FAIL	2
#define PT_INVALID_TYPE		3
#define PT_INVALID_VALUE	4
#define PT_BAD_PATHNAME		5
#define PT_NOT_FOUND		6

BOOL ptInit();
VOID ptDone();
ULONG ptQuery(PSZ pszPathname, ULONG ulMU, PULONG pulResult);
