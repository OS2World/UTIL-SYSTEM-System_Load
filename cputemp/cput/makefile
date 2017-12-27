#
# Open Watcom makefile.
#
# Usage:
#   wmake [RCTK=1]
#   wmake install
#   wmake clean
#
# Where:
#   RCTK=1     Use Resource Compiler from IBM OS/2 Developer's Toolkit instead
#              of Open Watcom Resource Compiler.
#
#   install    Install compiled plugin to XWP.
#              Note that you must have CPUTEMP.DLL somewhere in LIBPATH and
#              installed driver RDMSR.SYS.
#

# Point here path to XWP headers:
XWP_INC_PATH = $(%HOME)\projects\XWP\xworkplace\include

# Point here path to XWP plugins.
XWP_PLUGINS_PATH = $(%OSDIR)\SYSTEM\eWPS\PLUGINS\XCENTER

# Headers for cputemp.dll
CPUTEMP_PATH = ..\cputemp

AUTOR = Andrey Vasilkin
VERSION = 1.1.2
COMMENT = XCenter CPU temperature plugin DLL

DLL_NAME = cput
BIN_PATH = ..\bin

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


DLL_FILE = $(BIN_PATH)\$(DLL_NAME).dll
OBJS = widget.obj

INCPATH = $(%WATCOM)\H\OS2;$(%WATCOM)\H;$(XWP_INC_PATH);$(CPUTEMP_PATH);.

CFLAGS = -i=$(INCPATH) -bt=OS2 -d0 -q -s -bm -dVERSION="$(VERSION)" -bd

$(DLL_FILE): $(OBJS) $(DLL_NAME).lnk $(DLL_NAME).res
  wlink @$(DLL_NAME).lnk
  $(RCL) $(DLL_NAME).res $@

$(DLL_NAME).lnk
  @%create $@
  @%append $@ SYSTEM OS2V2_DLL
  @%append $@ OPTION MANYAUTODATA
!ifdef %unixroot
  @$(%unixroot)\usr\libexec\bin\date.exe +"OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(COMMENT)'" >>$^@
!else ifdef %osdir
  @$(%osdir)\KLIBC\BIN\date.exe +"OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(COMMENT)'
!endif
  @%append $@ NAME $(DLL_FILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION QUIET
  @%append $@ SEGMENT TYPE DATA	LOADONCALL NONSHARED READWRITE
  @%append $@ SEGMENT TYPE CODE	LOADONCALL
  @%append $@ LIBPATH ..\bin
  @%append $@ LIB cputemp.lib
  @for %i in ($(OBJS)) do @%append $@ FILE %i
  @%append $@ EXPORT WgtInitModule		.1
  @%append $@ EXPORT WgtUnInitModule		.2
  @%append $@ EXPORT WgtQueryVersion		.3

$(DLL_NAME).res: cput.rc cput.h
  $(RCC) $*.rc $@

.c.obj:
  wcc386 $(CFLAGS) $<

install: .SYMBOLIC
  unlock $(XWP_PLUGINS_PATH)\$(DLL_NAME).dll >nul
  copy $(BIN_PATH)\$(DLL_NAME).dll /b $(XWP_PLUGINS_PATH)

uninstall: .SYMBOLIC
  unlock $(XWP_PLUGINS_PATH)\$(DLL_NAME).dll >nul
  del $(XWP_PLUGINS_DIR)\$(XWP_PLUGINS_PATH).dll

clean: .SYMBOLIC
  @if exist *.obj @del *.obj
  @if exist *.err @del *.err
  @if exist $(DLL_NAME).lnk @del $(DLL_NAME).lnk
  @if exist $(DLL_NAME).res @del $(DLL_NAME).res
  @if exist $(DLL_FILE) @del $(DLL_FILE)
