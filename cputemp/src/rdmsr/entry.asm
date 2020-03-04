;
;    OS/4 RDMSR$
;
;    Copyright (c) OS/4 Team 2016
;    All Rights Reserved
;    ддддддддддддддддддддддддддддддддддддддд
;
;   driver header and standard entry points
;
                   .686P
                   .SEQ
                   include segments.inc
                   include devhdr.inc
                   include macro.inc
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
                   Extrn Dos32FlatDS:ABS
                   Extrn Strat_Init_:Far
                   Extrn DriverEntry:Near
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
_DDHEADER          Segment                                           ;
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
Public             _MainEntry                                        ;
                   dd    -1                                          ; NextHdr
                   dw    DEV_CHAR_DEV+DEVLEV_3                       ; DrvFlags
_MainEntry         dw    _TEXT16:Strat_Init_                         ;
                   dw    0                                           ; IDCEntry
                   db    'RDMSR$  '                                  ; DriverName
                   dw    0                                           ; ProtCS
                   dw    0                                           ; ProtDS
                   dw    0                                           ; RealCS
                   dw    0                                           ; RealDS
                   dd    DEV_INITCOMPLETE+DEV_ADAPTER_DD+DEV_16MB+DEV_IOCTL2; Capabilities
                   dw    0                                           ; Rsvd
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
_DDHEADER          EndS                                              ;
; ммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммм
_TEXT16            Segment                                           ;
                   Assume ds:_DDHEADER,es:NOTHING                    ;
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
; 16-bit strategy                                                    ;
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
Public             _Strat16                                          ;
_Strat16           Proc  Far                                         ;
                   jmp   Far Ptr FLAT:g_Strategy32                   ;
_Strat16           EndP                                              ;
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
_TEXT16            EndS                                              ;
; ммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммм
_TEXT              Segment                                           ;
                   Assume ds:_DDHEADER,es:NOTHING                    ;
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
; 32-bit part of Strategy entry thunk                                ;
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
g_Strategy32       Proc    Far
                   SaveReg <ax,ds,es>
                   db      66h
                   push    es
                   push    bx
                   SetFlat32 <ds,es>,ax
                   call    DriverEntry
                   add     esp, 4
                   RestReg <es,ds,ax>
                   db      66h
                   retf
g_Strategy32       EndP
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
_TEXT              EndS                                              ;
; ммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммммм
_TEXTEND           Segment                                           ;
                   Public  _CodeEnd                                  ;
_CodeEnd           Label   Byte                                      ;
_TEXTEND           EndS                                              ;
; ддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддддд
                   END
