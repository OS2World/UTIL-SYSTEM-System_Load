#
# This makefile for Open Watcom will be generate next files:
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
BINPATH = ..\bin

# Uncomment next line and execute WMAKE clean / WMAKE to create a debug version.
#DEBUGCODE = YES

# --- Signatures ---

AUTOR = Andrey Vasilkin
DLL_NAME = cputemp
EXE_NAME = cputemp
DLL_VERSION = 2.0.3
DLL_COMMENT = Intel CPU temperature sensors access
EXE_VERSION = 1.1.1
EXE_COMMENT = System Load utility. Processor temperature monitor

# ------------------


EXE_C32FLAGS = -i=$(%WATCOM)\h;$(%WATCOM)\h\os2 -bt=os2 -zq -s -wx -d0
DLL_C32FLAGS = $(EXE_C32FLAGS) -bd -bm

DLL_OBJS = cputemp.obj

!ifdef DEBUGCODE
DLL_NAME +=d
EXE_NAME +=d
DLL_C32FLAGS += -DDEBUG_FILE="$(DLL_NAME).dbg"
DLL_OBJS += debug.obj
DLL_COMMENT += (debug)
!endif

DLL_FILE = $(BINPATH)\$(DLL_NAME).dll
LIB_FILE = $(BINPATH)\$(DLL_NAME).lib
EXE_FILE = $(BINPATH)\$(EXE_NAME).exe

EXE_C32 = @wcc386 $(EXE_C32FLAGS)
DLL_C32 = @wcc386 $(DLL_C32FLAGS)

.SUFFIXES:
.SUFFIXES: .obj .c .asm

all:    .SYMBOLIC $(DLL_FILE) $(LIB_FILE) $(EXE_FILE)


#	CPUTEMP.DLL
#	-----------

$(LIB_FILE): $(DLL_FILE)
  @wlib -b -n -q $* $(DLL_FILE)

$(DLL_FILE): $(DLL_OBJS) cputemp.lnk
  @wlink @cputemp.lnk

cputemp.lnk: .ALWAYS
  @%create $@
  @%append $@ SYSTEM os2v2_dll INITINSTANCE TERMINSTANCE
  @%append $@ NAME $(DLL_FILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION MANYAUTODATA
  @%append $@ OPTION MAP=cputemp_dll.map
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
  @for %i in ($(DLL_OBJS)) do @%append $@ FILE %i
  @%append $@ IMPORT DosGetProcessorStatus           DOSCALLS .447
  @%append $@ IMPORT DosSetProcessorStatus           DOSCALLS .448
  @%append $@ IMPORT DosQueryThreadAffinity          DOSCALLS .563
  @%append $@ IMPORT DosSetThreadAffinity            DOSCALLS .564
  @%append $@ EXPORT cputempQuery                      .1
!ifdef %unixroot
  @$(%unixroot)\usr\libexec\bin\date.exe +"OPTION DESCRIPTION '@$#$(AUTOR):$(DLL_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(DLL_COMMENT)'" >>$^@
!else ifdef %osdir
  @$(%osdir)\KLIBC\BIN\date.exe +"OPTION DESCRIPTION '@$#$(AUTOR):$(DLL_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(DLL_COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(DLL_VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(DLL_COMMENT)'
!endif

cputemp.obj: cputemp.c
  $(DLL_C32) $*.c

debug.obj: debug.c
  $(DLL_C32) $*.c


#	CPUTEMP.EXE
#	-----------

$(EXE_FILE): util.obj util.lnk
  @wlink @util.lnk

util.lnk: .ALWAYS
  @%create $@
  @%append $@ SYSTEM os2v2
  @%append $@ NAME $(EXE_FILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION MAP=cputemp_exe.map
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
  @%append $@ FILE util.obj
  @%append $@ IMPORT cputempQuery                 $(DLL_NAME) .1
!ifdef %unixroot
  @$(%unixroot)\usr\libexec\bin\date.exe +"OPTION DESCRIPTION '@$#$(AUTOR):$(EXE_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(EXE_COMMENT)'" >>$^@
!else ifdef %osdir
  @$(%osdir)\KLIBC\BIN\date.exe +"OPTION DESCRIPTION '@$#$(AUTOR):$(EXE_VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(EXE_COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(EXE_VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(EXE_COMMENT)'
!endif

util.obj: util.c
  $(EXE_C32) $*.c

clean: .SYMBOLIC
  @if exist *.obj @del *.obj
  @if exist *.err @del *.err
  @if exist *.map @del *.map
  @if exist *.lnk @del *.lnk
  @if exist $(DLL_FILE) @del $(DLL_FILE)
  @if exist $(LIB_FILE) @del $(LIB_FILE)
  @if exist $(EXE_FILE) @del $(EXE_FILE)
