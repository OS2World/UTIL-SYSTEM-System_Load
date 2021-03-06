;
;    OS/4 RDMSR$
;
;    Copyright (c) OS/4 Team 2016
;    All Rights Reserved
;   컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴�
;
;   declaration and ordering of driver segments
;
_DDHEADER          Segment Dword Public USE16 'DATA'
_DDHEADER          EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
CONST              Segment Dword Public USE32 'DATA'
CONST              EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
CONST2             Segment Dword Public USE32 'DATA'
CONST2             EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
_DATA              Segment Dword Public USE32 'DATA'
_DATA              EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
_BSS               Segment Dword Public USE32 'BSS'
_BSS               EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
DGROUP             GROUP _DDHEADER,CONST,CONST2,_DATA,_BSS


; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
_TEXT16            Segment Para  Public USE16 'CODE'
_TEXT16            EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
_TEXT              Segment Dword Public USE32 'CODE'
_TEXT              EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
_TEXTEND           Segment Byte  Public USE32 'CODE'
_TEXTEND           EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
_TEXTINIT          Segment Dword Public USE32 'CODE'
_TEXTINIT          EndS
; 袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴袴�
CGROUP             GROUP _TEXT16,_TEXT,_TEXTEND,_TEXTINIT
