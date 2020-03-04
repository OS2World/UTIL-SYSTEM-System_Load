INCPATH +=..\..\h;
!include ..\..\..\..\global.mif

# Current version
VER_MAJOR    = 0
VER_MINOR    = 0
VER_REVISION = 2

# Comment for the bldlevel information
COMMENT = Version signature reader, REXX API

.c: ..\..\common

# C Sources
SRCS = bldlevel.c rxgetver.c

CFLAGS += -bd

# The name of the DLL file without the extension
NAME = rxgetver

BINPATH = ..\..\..\bin
BINFILE = $(BINPATH)\$(NAME).dll
LNKFILE = $(NAME).lnk

$(BINFILE): $(OBJS) $(LNKFILE)
  $(INFLINK)
  $(LINK) @$(LNKFILE)
  $(LXLITE) $@ >nul
  copy rxgetver.cmd $(BINPATH) >nul

$(LNKFILE): makefile
  @%create $@
  @%append $@ SYSTEM os2v2_dll INITINSTANCE TERMINSTANCE
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
  @%append $@ EXPORT GVLOADFUNCS             .1 = gvLoadFuncs
  @%append $@ EXPORT GVDROPFUNCS             .2 = gvDropFuncs
  @%append $@ EXPORT GVPARSE                 .3 = gvParse
  @%append $@ EXPORT GVGETFROMFILE           .4 = gvGetFromFile

clean: .SYMBOLIC
  @if exist *.obj del *.obj
  @if exist *.err del *.err
  @if exist $(LNKFILE) del $(LNKFILE)
  @if exist $(NAME).map del $(NAME).map
  @if exist $(BINFILE) del $(BINFILE)
  @if exist $(BINPATH)\rxgetver.cmd del $(BINPATH)\rxgetver.cmd
