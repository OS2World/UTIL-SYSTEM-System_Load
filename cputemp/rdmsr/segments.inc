;
;    OS/4 RDMSR$
;
;    Copyright (c) OS/4 Team 2016
;    All Rights Reserved
;   ฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤฤ
;
;   declaration and ordering of driver segments
;
_DDHEADER          Segment Dword Public USE16 'DATA'
_DDHEADER          EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
CONST              Segment Dword Public USE32 'DATA'
CONST              EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
CONST2             Segment Dword Public USE32 'DATA'
CONST2             EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
_DATA              Segment Dword Public USE32 'DATA'
_DATA              EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
_BSS               Segment Dword Public USE32 'BSS'
_BSS               EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
DGROUP             GROUP _DDHEADER,CONST,CONST2,_DATA,_BSS


; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
_TEXT16            Segment Para  Public USE16 'CODE'
_TEXT16            EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
_TEXT              Segment Dword Public USE32 'CODE'
_TEXT              EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
_TEXTEND           Segment Byte  Public USE32 'CODE'
_TEXTEND           EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
_TEXTINIT          Segment Dword Public USE32 'CODE'
_TEXTINIT          EndS
; อออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ
CGROUP             GROUP _TEXT16,_TEXT,_TEXTEND,_TEXTINIT
