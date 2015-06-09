#
# This makefile for Open Watcom will be generate next files:
#
# RDMSR.SYS
#
# Driver for accessing to the Model-specific registers. Compatible with similar
# nickk's driver.
#
# CPUTEMP.DLL
#
# Dynamic library for query Intel processor temperature sensors. It uses
# the driver RDMSR.SYS.
#
# CPUTEMP.EXE
#
# Utility to display the CPU temperature.
#

# BINPATH - binaries output path.
BINPATH = ..\..\bin

# --- Signatures ---
AUTOR = Andrey Vasilkin
DRV_VERSION = 1.0.0
DRV_COMMENT = Model-Specific Registers reader.
DLL_VERSION = 1.1.0
DLL_COMMENT = Intel CPU temperature sensors access.
EXE_VERSION = 1.1.0
EXE_COMMENT = System Load utility. Processor temperature monitor.


DRV_FILE = $(BINPATH)\rdmsr.sys
DLL_FILE = $(BINPATH)\cputemp.dll
EXE_FILE = $(BINPATH)\cputemp.exe

C32FLAGS = -i=$(%WATCOM)\h;$(%WATCOM)\h\os2 -bt=os2 -zq -s -wx -d0
WC32 = wcc386 $(C32FLAGS)

C16FLAGS = -i=$(%WATCOM)\h;$(%WATCOM)\h\os21x -bt=os2 -ms -5 -omi -s -zdp -zff -zgf -zu -zl -zq -wx -d0
WC16 = wcc $(C16FLAGS)

AFLAGS = -zq
ASM = wasm $(AFLAGS)

.SUFFIXES:
.SUFFIXES: .obj .c .asm

all:    .SYMBOLIC $(DRV_FILE) $(DLL_FILE) $(EXE_FILE)


#	RDMSR.SYS
#	---------

DRVOBJS = devsegs.obj header.obj strategy.obj drvinit.obj
# debug.obj

$(DRV_FILE) : $(DRVOBJS) rdmsr.lnk
  wlink @rdmsr.lnk

rdmsr.lnk : .ALWAYS
  @%create $@
  @%append $@ name $(DRV_FILE)
  @%append $@ sys os2 dll initglobal
  @%append $@ option protmode
  @%append $@ option quiet
  @%append $@ option stack=0
  @%append $@ lib os2
  @for %f in ($(DRVOBJS)) do @%append $@ file %f
  @%append $@ segment type DATA SHARED PRELOAD
  @%append $@ segment '_TEXT' PRELOAD IOPL
  @%append $@ segment '_INITCODE' PRELOAD IOPL
!ifdef %osdir
  @$(%osdir)\KLIBC\BIN\date +"OPTION DESCRIPTION '@$#$(AUTOR):$(DRV_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(DRV_COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(DRV_VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(DRV_COMMENT)'
!endif

.c.obj: .AUTODEPEND
  $(WC16) $*.c

.asm.obj: .AUTODEPEND
  $(ASM) $*.asm


#	CPUTEMP.DLL
#	-----------

$(DLL_FILE): cputemp.obj cputemp.lnk
  wlink @cputemp.lnk

cputemp.lnk: .ALWAYS
  @%create $@
  @%append $@ SYSTEM os2v2_dll INITINSTANCE TERMINSTANCE
  @%append $@ NAME $(DLL_FILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION MANYAUTODATA
  @%append $@ OPTION MAP=cputemp_dll.map
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
  @%append $@ FILE cputemp.obj
  @%append $@ IMPORT DosGetProcessorStatus           DOSCALLS .447
  @%append $@ IMPORT DosSetProcessorStatus           DOSCALLS .448
  @%append $@ IMPORT DosQueryThreadAffinity          DOSCALLS .563
  @%append $@ IMPORT DosSetThreadAffinity            DOSCALLS .564
  @%append $@ EXPORT cputempQuery                      .1
!ifdef %osdir
  @$(%osdir)\KLIBC\BIN\date +"OPTION DESCRIPTION '@$#$(AUTOR):$(DLL_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(DLL_COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(DLL_VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(DLL_COMMENT)'
!endif

cputemp.obj: cputemp.c
  $(WC32) -bd -bm $*.c


#	CPUTEMP.EXE
#	-----------

$(EXE_FILE): util.obj util.lnk
  wlink @util.lnk

util.lnk: .ALWAYS
  @%create $@
  @%append $@ SYSTEM os2v2
  @%append $@ NAME $(EXE_FILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION MAP=cputemp_exe.map
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
  @%append $@ FILE util.obj
  @%append $@ IMPORT cputempQuery                 CPUTEMP .1
!ifdef %osdir
  @$(%osdir)\KLIBC\BIN\date +"OPTION DESCRIPTION '@$#$(AUTOR):$(EXE_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(EXE_COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(EXE_VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(EXE_COMMENT)'
!endif

util.obj: util.c
  $(WC32) $*.c

clean: .SYMBOLIC
  @if exist *.obj @del *.obj
  @if exist *.err @del *.err
  @if exist *.map @del *.map
  @if exist *.lnk @del *.lnk
