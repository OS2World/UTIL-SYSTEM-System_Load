#ifndef SYSINFO_H
#define SYSINFO_H

// Properties dialog
#define IDD_DSPROPERTIES         1000
#define IDD_GB_VAR               1101
#define IDD_CB_VARAVAIL          1102
#define IDD_PB_ADD               1103
#define IDD_EF_COMMENT           1104
#define IDD_LB_VAR               1105
#define IDD_PB_UP                1106
#define IDD_PB_DOWN              1107
#define IDD_PB_REMOVE            1108
#define IDD_ST_PRESENTATION      1109
#define IDD_CB_PRESENTATION      1110
#define IDD_ST_COMMENTSET        1111
#define IDD_EF_COMMENTSET        1112
#define IDD_PB_COMMENTSET        1113
#define IDD_GB_SHOW              1201
#define IDD_RB_SHOW_FULL         1202
#define IDD_RB_SHOW_NAME         1203
#define IDD_RB_SHOW_COMMENT      1204
#define IDD_ST_INTERVAL          1205
#define IDD_SB_INTERVAL          1206
#define IDD_ST_SECONDS           1207

#define IDS_COPY_NAME            1
#define IDS_COPY_VALUE           2

#define QSV_MAX_OS4              33

#define SYSVAR_COUNT             27

#define CONV_COUNT               16
#define SI_CONV_DEFAULT          0
#define SI_CONV_MSECHMS          1
#define SI_CONV_BYTESAUTO        2
#define SI_CONV_DRIVE            3
#define SI_CONV_DYNPRIORITY      4
#define SI_CONV_SECHMS           5
#define SI_CONV_VERNUM           6
#define SI_CONV_VERNAME          7
#define SI_CONV_DEFAULTLL        8
#define SI_CONV_KB               9
#define SI_CONV_MB               10
#define SI_CONV_TNTHS_MSEC       11
#define SI_CONV_HEX              12
#define SI_CONV_BYTES            13
#define SI_CONV_VALAUTO          14
#define SI_CONV_VALMB            15

// sysinfo.c, ulShowItems - items show style.
#define SI_ITEMS_FULL            0
#define SI_ITEMS_NAME            1
#define SI_ITEMS_COMMENT         2

typedef LONG (*PFNVALTOSTR)(PULONG pulValue, ULONG cbBuf, PCHAR pcBuf);

typedef struct _CONVVAL {
  PSZ                  pszTitle;
  PFNVALTOSTR          pFn;
} CONVVAL, *PCONVVAL;

typedef struct _SYSVAR {
  ULONG      ulOrdinal;
  PSZ        pszName;
  PSZ        pszComment;
  LONG       aConv[6]; // Indexes of aValToStr[] list ended with -1.
} SYSVAR, *PSYSVAR;

typedef struct _USERVAR {
  ULONG      ulSysVarId;
  PSZ        pszComment;
  ULONG      ulConvFunc;         // Index of afnValToStr[] : SI_CONV_xxxxx
} USERVAR, *PUSERVAR;

#endif // SYSINFO_H
