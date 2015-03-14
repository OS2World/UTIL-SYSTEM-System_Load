/*
   This is my attempt to unearth API provided by dynamic library FILEVER.DLL.
   Well known standard system program BLDLEVEL.EXE uses FILEVER.DLL. I was
   unable to find any documentation on this library. Only two functions
   described here, but there are others that I was not interested.

   Vasilkin Andrey, 2015.


   Functions described in this header file:

     FileVerGetVersionStringFromFile
     FileVerParseVersionString

   Other exports functions:

     FileVerCreateModuleVersion
     FileVerCompareVersions
     FileVerParseModuleVersion
     FileVerGetLANVerStringFromFile
     BaseModuleDependencyCheckPrerequisite
     FileVerModifyASDVersion
     AddDrive
     IsRemote
     IsRemovable
     GetValidDrives
     GetSyslevelFiles
     FindAllSyslevels
     AddDir
     FreeDirList
     AddSyslevel
     RM_REPLACEMODULE
     MergeArchiveWithFixtool
     CrippleOldFixtool
     checksum
     BaseModuleDependencyCheckFeature
     BaseModuleDependencyCheckAllFeatures
     FreeErrorList
     CSFCheckAllFeatures
     CSF_CHECKALLFEATURES
     IsServiceable
     CleanupSyslevelMem
     CheckIfBackoutBreaksAnybody
     CSF_CHECKBACKOUT
     IfNewer
     SetNewVersionStringFlag
*/

#define INCL_DOSERRORS    /* DOS error values */
#include <os2.h>

typedef struct _FILEVERINFO {
  ULONG		ulFlag1;	// I have see 0x00, 0x01 in ulFlag1
  ULONG		ulFlag2;	// and and 0x00, 0x40000000 in ulFlag2
				// with different signature formats.
  ULONG		ulMajor;
  ULONG		ulMinor;
  ULONG		ulRevision;
  CHAR		acVendor[32];
  CHAR		acDateTime[26];
  CHAR		acBuildMachine[12];
  CHAR		acASDFeatureID[12];
  CHAR		acLanguageCode[4];
  CHAR		acCountryCode[16];
  CHAR		acFileVersion[12];
  CHAR		acDescription[80];
} FILEVERINFO, *PFILEVERINFO;

#pragma aux FILEVERFUNC "*";
#pragma aux (FILEVERFUNC) FileVerGetVersionStringFromFile;
#pragma aux (FILEVERFUNC) FileVerParseVersionString;

// ULONG FileVerGetVersionStringFromFile(PSZ pszFile,
//                                       PCHAR pacVersionString,
//                                       ULONG ulUnknown,
//                                       PULONG pcbVersionString);
//
// Parameters:
//   pszFile          - An ASCIIZ string that contains the file name.
//   pacVersionString - Pointer to the caller's buffer area where the
//                      version string will be stored.
//   ulUnknown        - I do not know meaning of this parameter. It seems that
//                      its value does not affect the result.
//   pcbVersionString - Pointer to the maximum length of the buffer, in bytes.
//
// Returns:
//     0 NO_ERROR.
//     3 - File has no version string.
//   111 ERROR_BUFFER_OVERFLOW. Output pcbVersionString - required buffer size.
//   161 ERROR_BAD_PATHNAME.

// !!! Что-то не докопал с этой функцией - портит 11 байт в стеке приложения.

ULONG __cdecl FileVerGetVersionStringFromFile(PSZ pszFile,
                                                PCHAR pacVersionString,
                                                ULONG ulUnknown,
                                                PULONG pcbVersionString);

ULONG FileVerGetVersionStringFromFile2(PSZ pszFile, PCHAR pcVersionString,
                                    ULONG ulReserved, PULONG pcbVersionString);

// ULONG FileVerParseVersionString(PSZ pszVersionString,
//                                 ULONG ulInfoLevel,
//                                 ULONG ulUnknown,
//                                 PFILEVERINFO pFileVerInfo);
//
// Parameters:
//   pszVersionString - Version string, can be obtained by using the function
//                      FileVerGetVersionStringFromFile().
//   ulInfoLevel      - If this parameter is not equal to 1, the function
//                      returns error ERROR_INVALID_LEVEL.
//   ulUnknown        - I do not know meaning of this parameter. It seems that
//                      its value does not affect the result.
//   pFileVerInfo     - A pointer to the FILEVERINFO in which the version
//                      information is placed.
//
// Returns:
//     0 NO_ERROR.
//     4 - Version string cannot be parsed.
//   124 ERROR_INVALID_LEVEL.

ULONG __cdecl FileVerParseVersionString(PSZ pszVersionString,
                                        ULONG ulInfoLevel,
                                        ULONG ulUnknown,
                                        PFILEVERINFO pFileVerInfo);
