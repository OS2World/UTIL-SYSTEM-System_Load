#
# Open Watcom makefile
#
# Creates the dynamic library for query Intel processor temperature sensors:
# ..\bin\cputemp.dll.
# It uses the driver RDMSR.SYS.
#

# Uncomment the following line to explicitly define the debugging (yes) or
# working (no) version. May be reassigned in a global.mif file.
# You need to run WMAKE clean / WMAKE to switch to/from the debug version.
#DEBUGCODE = no

!include ..\..\..\global.mif

# Current version
VER_MAJOR    = 2
VER_MINOR    = 0
VER_REVISION = 2

# Comment for the bldlevel information
COMMENT = Intel CPU temperature sensors access

# The name of the binary file without the extension
NAME = cputemp

# C Sources
SRCS = cputemp.c

!ifeq DEBUGCODE yes
SRCS += debug.c
!endif

BINPATH = ..\..\bin
BINFILE = $(BINPATH)\$(NAME).dll
LIBFILE = $(BINPATH)\$(NAME).lib
LNKFILE = $(NAME).lnk
DBGFILE = $(NAME).dbg
CFLAGS += -bd -bm


$(LIBFILE): $(BINPATH) $(BINFILE)
  $(IMPLIB) $@ $(BINFILE)

$(BINFILE): $(OBJS) $(LNKFILE)
  $(INFLINK)
  $(LINK) @$(LNKFILE)

$(LNKFILE): makefile
  @%create $@
  @%append $@ SYSTEM os2v2_dll INITINSTANCE TERMINSTANCE
  @%append $@ NAME $(BINFILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION MANYAUTODATA
  @%append $@ OPTION MAP=$(NAME)
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2, eComStation and ArcaOS'
  @for %i in ($(OBJS)) do @%append $@ FILE %i
  @%append $@ IMPORT DosGetProcessorStatus           DOSCALLS .447
  @%append $@ IMPORT DosSetProcessorStatus           DOSCALLS .448
  @%append $@ IMPORT DosQueryThreadAffinity          DOSCALLS .563
  @%append $@ IMPORT DosSetThreadAffinity            DOSCALLS .564
  @%append $@ EXPORT cputempQuery                    .1
!ifdef DATEUTIL
  @$(DATEUTIL) +"OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(BLCOMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(BLCOMMENT)'
!endif

$(BINPATH): .EXISTSONLY
  md $(BINPATH)

clean: .SYMBOLIC
  @if exist *.obj del *.obj
  @if exist *.err del *.err
  @if exist $(LNKFILE) del $(LNKFILE)
  @if exist $(NAME).map del $(NAME).map
  @if exist $(BINPATH)\$(DBGFILE) del $(BINPATH)\$(DBGFILE)
  @if exist $(BINFILE) del $(BINFILE)
  @if exist $(LIBFILE) del $(LIBFILE)
