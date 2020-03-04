#
# Open Watcom makefile
#
# Creates a XCenter Intel CPU Temperature widget: ..\..\bin\cput.dll
#

!include ..\..\..\global.mif

# Current version
VER_MAJOR    = 1
VER_MINOR    = 1
VER_REVISION = 2

# Comment for the bldlevel information
COMMENT = XCenter Intel CPU Temperature plugin DLL

# The name of the binary file without the extension
NAME = cput

# C Sources
SRCS = widget.c

BINPATH = ..\..\bin
BINFILE = $(BINPATH)\$(NAME).dll
LNKFILE = $(NAME).lnk
RESFILE = $(NAME).res

INCPATH +=$(XWPINCPATH);..\cputemp;

CFLAGS += -bd -bm -s -dVERSION="$(VERSION)"

$(BINFILE): $(OBJS) $(LNKFILE) $(RESFILE) $(BINPATH)\cputemp.lib
  $(INFLINK)
  $(LINK) @$(LNKFILE)
  $(RCL) $(RESFILE) $@

$(LNKFILE): makefile
  @%create $@
  @%append $@ SYSTEM os2v2_dll
  @%append $@ NAME $(BINFILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION MANYAUTODATA
  @%append $@ SEGMENT TYPE DATA	LOADONCALL NONSHARED READWRITE
  @%append $@ SEGMENT TYPE CODE	LOADONCALL
  @%append $@ OPTION MAP=$(NAME)
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2, eComStation and ArcaOS'
  @%append $@ LIBPATH $(BINPATH)
  @%append $@ LIB $(BINPATH)\cputemp.lib
  @for %i in ($(OBJS)) do @%append $@ FILE %i
!ifdef DATEUTIL
  @$(DATEUTIL) +"OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(BLCOMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(BLCOMMENT)'
!endif
  @%append $@ EXPORT WgtInitModule         .1
  @%append $@ EXPORT WgtUnInitModule       .2
  @%append $@ EXPORT WgtQueryVersion       .3

widget.obj: widget.c cput.h ..\cputemp\cputemp.h

clean: .SYMBOLIC
  @if exist *.obj del *.obj
  @if exist *.err del *.err
  @if exist $(LNKFILE) del $(LNKFILE)
  @if exist $(NAME).map del $(NAME).map
  @if exist $(BINFILE) del $(BINFILE)
  @if exist $(RESFILE) del $(RESFILE)
