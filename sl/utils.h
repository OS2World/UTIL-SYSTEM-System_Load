#ifndef UTILS_H
#define UTILS_H

#define INCL_WIN
#define INCL_GPI
#include <os2.h>

//#define CLR_SL_TRANSPARENT	CLR_ERROR

// utilCharStringRect(,,,,ulFlags)
#define SL_CSR_CUTFADE		0
#define SL_CSR_CUTDOTS		1
#define SL_CSR_VCENTER		2

#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))

// UTIL_IS_HIGH_PTR( p )
// Returns TRUE if p points to the high memory. Useful with utilMemAlloc().
#define UTIL_IS_HIGH_PTR(p)   ((ULONG)(p) >= (512*1024*1024))

typedef unsigned long long	ULLONG;
typedef ULLONG			*PULLONG;

BOOL utilGetTextSize(HPS hps, ULONG cbText, PSZ pszText, PSIZEL pSize);
VOID util3DFrame(HPS hps, PRECTL pRect, LONG lLTColor, LONG lRBColor);
VOID utilBox(HPS hps, PRECTL pRect, LONG lColor);
BOOL utilWriteResStr(HPS hps, HMODULE hMod, ULONG ulId,
                     ULONG cVal, PSZ *ppszVal);
BOOL utilWriteResStrAt(HPS hps, PPOINTL pptPos, HMODULE hMod, ULONG ulId,
                       ULONG cVal, PSZ *ppszVal);
VOID utilCharStringRect(HPS hps, PRECTL pRect, ULONG cbBuf, PCHAR pcBuf,
                        ULONG ulFlags);
BOOL utilSetFontFromPS(HPS hpsDst, HPS hpsSrc, LONG llcid);
VOID utilSetColorsFromPS(HPS hpsDst, HPS hpsSrc);

BOOL utilWriteProfileLong(HINI hIni, PSZ pszApp, PSZ pszKey, LONG lData);
BOOL utilWriteProfileULong(HINI hIni, PSZ pszApp, PSZ pszKey, ULONG ulData);
BOOL utilWriteProfileStr(HINI hIni, PSZ pszApp, PSZ pszKey, PSZ pszData);
BOOL utilWriteProfileData(HINI hIni, PSZ pszApp, PSZ pszKey, PVOID pBuf,
                          ULONG cbBuf);
LONG utilQueryProfileLong(HINI hIni, PSZ pszApp, PSZ pszKey, LONG lDefault);
ULONG utilQueryProfileULong(HINI hIni, PSZ pszApp, PSZ pszKey, ULONG ulDefault);
ULONG utilQueryProfileStr(HINI hIni, PSZ pszApp, PSZ pszKey, PSZ pszDefault,
                          PCHAR pcBuf, ULONG cbBuf);
BOOL utilQueryProfileData(HINI hIni, PSZ pszApp, PSZ pszKey, PVOID pBuf,
                           PULONG pcbBuf);
BOOL utilQueryProfileSize(HINI hIni, PSZ pszApp, PSZ pszKey, PULONG pcbData);

LONG utilMixRGB(LONG lColor1, LONG lColor2, ULONG ulMix);
ULONG utilLoadInsertStr(HMODULE hMod,		// module handle
                       BOOL fStrMsg, 		// string (1) / message (0)
                       ULONG ulId,		// string/message id
                       ULONG cVal, PSZ *ppszVal,// count and pointers to values
                       ULONG cbBuf, PCHAR pcBuf);// result buffer
BOOL utilTestFont(PSZ pszFont);
ULONG utilQueryProgPath(ULONG cbBuf, PCHAR pcBuf);

// Strings

LONG strFromSec(ULONG ulVal, ULONG cbBuf, PCHAR pcBuf);
LONG strFromBits(ULONG ulVal, ULONG cbBuf, PCHAR pcBuf, BOOL fSlashSec);
LONG strFromBytes(ULLONG ullVal, ULONG cbBuf, PCHAR pcBuf, BOOL fSlashSec);
ULONG strLoad(HMODULE hModule, ULONG ulStrId, ULONG cbBuf, PCHAR pcBuf);
ULONG strLoad2(HMODULE hModule, ULONG ulStrId, PULONG pcbBuf, PCHAR *ppcBuf);
ULONG strFromULL(PCHAR pcBuf, ULONG cbBuf, ULLONG ullVal);
VOID strRemoveMnemonic(ULONG cbBuf, PCHAR pcBuf, PSZ pszText);

#endif // UTILS_H
