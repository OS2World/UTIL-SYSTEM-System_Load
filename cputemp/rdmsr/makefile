#
# This makefile for Open Watcom will be generate next files:
#
# RDMSR.SYS         Driver for accessing to the Model-specific registers.
#
# RDMSR-TEST.EXE    Program for testing the driver. Will execute different
#                   requests to the driver.
#

# --- Paths ---

# BINPATH - binaries output path.
BINPATH = ..\bin

# THIRDPARTY - OS/4 headers and other stuff.
THIRDPARTY = D:\dev

# --- Signatures ---

DRV_AUTOR    = Andrey Vasilkin
DRV_VERSION  = 2.0.1
DRV_COMMENT  = Model-Specific Registers reader.
DRV_FILEBASE = rdmsr

#DEBUGCODE = YES


EXE_C32FLAGS = -i=$(%WATCOM)\H\OS2;$(%WATCOM)\H -bt=os2 -q -d0 -w2

DRV_C32FLAGS = -q -za99 -bt=os2 -bd -ei -fpi87 -zp1 -d0 -fp6 -6s -mf -ox -oi -s &
               -wx -zl -zm -zls &
               -i=$(THIRDPARTY)\share\h;$(THIRDPARTY)\watcom\h\os2 &
               -i=$(%WATCOM)\h;$(%WATCOM)\h\os2

DRV_C16FLAGS = -q -za99 -bt=os2 -bd -ei -fpi87 -zp1 -d0 -fp6 -ox -oi -wx -zl -zm &
               -zls -nt=_TEXT16 &
               -i=$(THIRDPARTY)\share\h;$(THIRDPARTY)\watcom\h\os2 &
               -i=$(%WATCOM)\h;$(%WATCOM)\h\os21x

DRV_AFLAGS   = -q -i=$(THIRDPARTY)\share\inc


!ifdef DEBUGCODE
EXE_C32FLAGS += -DDEBUGCODE
DRV_C32FLAGS += -DDEBUGCODE
DRV_C16FLAGS += -DDEBUGCODE
!endif

EXE_C32      = @wcc386 $(EXE_C32FLAGS)
DRV_C32      = @wcc386 $(DRV_C32FLAGS)
DRV_C16      = @wcc $(DRV_C16FLAGS)
DRV_ASM      = @wasm $(DRV_AFLAGS)


.SUFFIXES:
.SUFFIXES: .obj .c .c16 .asm

DRV_FILE = $(BINPATH)\$(DRV_FILEBASE).sys
DRV_LNKFILE = $(DRV_FILEBASE).lnk
DRV_MAPFILE = $(DRV_FILEBASE).map
TESTEXE_FILE = $(BINPATH)\$(DRV_FILEBASE)-test.exe

all:    .SYMBOLIC $(DRV_FILE) $(TESTEXE_FILE)


#	RDMSR.SYS
#	---------

DRVOBJS = entry.obj ioctl.obj strat.obj init.obj

$(DRV_FILE) : $(DRVOBJS) $(DRV_LNKFILE)
  @wlink @$(DRV_LNKFILE)

$(DRV_LNKFILE) : .ALWAYS
  @%create $@
  @%append $@ name $(DRV_FILE)
  @%append $@ format os2 LX PHYSDEVICE
  @%append $@ option nostub
  @%append $@ option quiet
  @%append $@ option mixed1632
  @%append $@ option align=4
  @%append $@ option stack=0
  @%append $@ option map=$(DRV_MAPFILE)
  @%append $@ order clname DATA segment _DDHEADER
  @%append $@ clname CODE segment _TEXT16 segment _TEXT segment _TEXTEND segment _TEXTINIT
  @%append $@ file {$(DRVOBJS)}
  @%append $@ segment type DATA SHARED PRELOAD
  @%append $@ segment class CODE PRELOAD
  @%append $@ IMPORT Dos32FlatDS DOSCALLS.370
  @%append $@ IMPORT DOSPUTMESSAGE DOSCALLS.383
  @%append $@ library $(THIRDPARTY)\share\lib\ddk\kee.lib
!ifdef %unixroot
  @$(%unixroot)\usr\libexec\bin\date.exe +"OPTION DESCRIPTION '@$#$(DRV_AUTOR):$(DRV_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(DRV_COMMENT)'" >>$^@
!else ifdef %osdir
  @$(%osdir)\KLIBC\BIN\date.exe +"OPTION DESCRIPTION '@$#$(DRV_AUTOR):$(DRV_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(DRV_COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(DRV_AUTOR):$(DRV_VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(DRV_COMMENT)'
!endif


#	RDMSR-TEST.EXE
#	--------------

$(TESTEXE_FILE) : rdmsr-test.c
  $(EXE_C32) rdmsr-test.c
  @wlink SYS os2v2 op q file {rdmsr-test} name $*


.c.obj: .AUTODEPEND
  $(DRV_C32) $*.c

.c16.obj: .AUTODEPEND
  $(DRV_C16) $*.c16

.asm.obj: .AUTODEPEND
  $(DRV_ASM) $*.asm

clean: .SYMBOLIC
  @if exist *.obj @del *.obj
  @if exist *.err @del *.err
  @if exist $(DRV_LNKFILE) @del $(DRV_LNKFILE)
  @if exist $(DRV_MAPFILE) @del $(DRV_MAPFILE)
  @if exist $(DRV_FILE) @del $(DRV_FILE)
  @if exist $(TESTEXE_FILE) @del $(TESTEXE_FILE)
