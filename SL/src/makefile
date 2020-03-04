#
# Open Watcom makefile
#
# Creates the main program of the SystelLoad project: ..\bin\sl.exe
#

# Uncomment the following line to explicitly define the debugging (yes) or
# working (no) version. May be reassigned in a global.mif file.
# You need to run WMAKE clean / WMAKE to switch to/from the debug version.
#DEBUGCODE = yes

INCPATH =.\h;

!include ..\..\global.mif

# Current version
VER_MAJOR    = 1
VER_MINOR    = 1
VER_REVISION = 0

# Comment for the bldlevel information
COMMENT = SystemLoad

# The name of the executable file without the extension
NAME = sl

.c: .\common

# C Sources
SRCS = sl.c mainwin.c list.c details.c hint.c items.c srclist.c graph.c &
       utils.c ctrl.c settings.c properties.c hmem.c

!ifeq DEBUGCODE yes
SRCS += debug.c
!endif

BINPATH = ..\bin
BINFILE = $(BINPATH)\$(NAME).exe
LNKFILE = $(NAME).lnk
RESFILE = $(NAME).res
DBGFILE = $(NAME).dbg

CFLAGS += -bw -DVER_MAJOR=$(VER_MAJOR) -DVER_MINOR=$(VER_MINOR) &
          -DVER_REVISION=$(VER_REVISION)

$(BINFILE): $(BINPATH) $(OBJS) $(RESFILE) $(LNKFILE)
  $(INFLINK)
  $(LINK) @$(LNKFILE)
  $(LXLITE) $@ >nul
  $(RCL) $(RESFILE) $@

$(LNKFILE): makefile
  @%create $@
  @%append $@ SYSTEM os2v2_pm
  @%append $@ NAME $(BINFILE)
  @%append $@ OPTION MAP=$(NAME)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2, eComStation and ArcaOS'
!ifdef DATEUTIL
  @$(DATEUTIL) +"OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(BLCOMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(BLCOMMENT)'
!endif
!ifdef LIBPATH
  @%append $@ LIBPATH $(LIBPATH)
!endif
!ifdef LIBS
  @for %i in ($(LIBS)) do @%append $@ LIB %i
!endif
  @for %i in ($(OBJS)) do @%append $@ FILE %i

clean: .SYMBOLIC
  @if exist *.obj del *.obj
  @if exist *.err del *.err
  @if exist $(RESFILE) del $(RESFILE)
  @if exist $(LNKFILE) del $(LNKFILE)
  @if exist $(NAME).map del $(NAME).map
  @if exist $(BINPATH)\$(DBGFILE) del $(BINPATH)\$(DBGFILE)
  @if exist $(BINFILE) del $(BINFILE)
  @if exist $(BINPATH)\SL.INI del $(BINPATH)\SL.INI
