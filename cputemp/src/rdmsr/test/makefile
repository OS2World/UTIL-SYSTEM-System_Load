#
# Open Watcom makefile
#
# Creates a program ..\..\bin\rdmsrtst.exe to verify rdmsr driver.
#

!include ..\..\..\..\global.mif

# Current version
VER_MAJOR    = 0
VER_MINOR    = 0
VER_REVISION = 1

# Comment for the bldlevel information
COMMENT = Test RDMSR driver

# The name of the binary file without the extension
NAME = rdmsrtst

# C Sources
SRCS = rdmsrtst.c

BINPATH = ..\..\..\bin
BINFILE = $(BINPATH)\$(NAME).exe
LNKFILE = $(NAME).lnk

INCPATH +=..;

$(BINFILE): $(BINPATH) $(OBJS) $(LNKFILE)
  $(LINK) @$(LNKFILE)

$(LNKFILE): makefile
  @%create $@
  @%append $@ SYSTEM os2v2
  @%append $@ NAME $(BINFILE)
  @%append $@ OPTION ELIMINATE
  @%append $@ OPTION MAP=$(NAME)
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2, eComStation and ArcaOS'
  @for %i in ($(OBJS)) do @%append $@ FILE %i
  @%append $@ IMPORT cputempQuery                 cputemp .1
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
  @if exist $(BINFILE) del $(BINFILE)
