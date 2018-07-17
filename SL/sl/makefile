VER_MAJOR    = 1
VER_MINOR    = 0
VER_REVISION = 6

AUTOR = Andrey Vasilkin
COMMENT = System load monitor
PROG = sl

# Uncomment next line and execute WMAKE clean / WMAKE to create a debug version.
# DEBUGCODE = YES

# -------------------

!ifeq RCTK 1

# You can use Resource Compiler from IBM OS/2 Developer's Toolkit.
# WMAKE rctk=1
RCC = rc -r -n -x2
RCL = rc -n

!else

# Open Watcom Resource Compiler Version 2.0 beta must be used. Older versions
# creates incorrect .RES files.
RCC = wrc -q -r
RCL = wrc -q

!endif


SRCS = sl.c mainwin.c list.c details.c hint.c items.c srclist.c graph.c &
       utils.c ctrl.c settings.c properties.c hmem.c

#LIBS =
#LIBPATH =

VERSION = $(VER_MAJOR).$(VER_MINOR).$(VER_REVISION)
BINPATH = ..\bin
BINFILE = $(BINPATH)\$(PROG).exe
LNKFILE = $(PROG).lnk
RESFILE = $(PROG).res
DBGFILE = $(PROG).dbg

CFLAGS = -i=$(%WATCOM)\H\OS2;$(%WATCOM)\H -bt=os2 -q -d0 -w2 -bw
CFLAGS += -DVER_MAJOR=$(VER_MAJOR) -DVER_MINOR=$(VER_MINOR) -DVER_REVISION=$(VER_REVISION)

!ifdef DEBUGCODE
CFLAGS += -DDEBUG_FILE="$(DBGFILE)"
SRCS += debug.c
COMMENT += (debug)
!else
.SILENT
!endif

OBJS = $(SRCS:.c=.obj)

$(BINFILE): infCompiling $(OBJS) $(RESFILE) $(LNKFILE)
  @echo *  Linking: $@
  wlink @sl.lnk
  $(RCL) $(RESFILE) $@
  @echo.

$(LNKFILE): .ALWAYS
  @echo *  Creating file: $@
  @%create $@
  @%append $@ SYSTEM os2v2_pm
  @%append $@ NAME $(BINFILE)
  @%append $@ OPTION MAP=$(PROG)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
!ifdef LIBPATH
  @%append $@ LIBPATH $(LIBPATH)
!endif
!ifdef LIBS
  @for %i in ($(LIBS)) do @%append $@ LIB %i
!endif
  @for %i in ($(OBJS)) do @%append $@ FILE %i
!ifdef %osdir
  #
  # eComStation have cool utility C:\KLIBC\BIN\date.exe.
  # We can get current date/time  here.
  #
  @$(%osdir)\KLIBC\BIN\date +"OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(COMMENT)'
!endif

.c.obj:
  wcc386 $(CFLAGS) $<

$(RESFILE): sl.rc sl.h
  @echo *  Compiling resources: $@
  $(RCC) $*.rc $@

infCompiling: .SYMBOLIC
  @echo *  Compiling: $(COMMENT) $(VERSION) ...

clean: .SYMBOLIC
  @if exist *.obj del *.obj
  @if exist $(RESFILE) del $(RESFILE)
  @if exist $(LNKFILE) del $(LNKFILE)
  @if exist $(PROG).map del $(PROG).map
  @if exist $(BINPATH)\$(DBGFILE) del $(BINPATH)\$(DBGFILE)
  @if exist $(BINFILE) del $(BINFILE)
