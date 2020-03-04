#
# Open Watcom makefile
#
# RDMSR.SYS - driver for accessing to the Model-specific registers.
#

!include ..\..\..\global.mif

# Current version
VER_MAJOR    = 2
VER_MINOR    = 0
VER_REVISION = 1

AUTHOR = OS/4 Team

# Comment for the bldlevel information
COMMENT = Model-Specific Registers reader

BINPATH = ..\..\bin

NAME = rdmsr

CFLAGS = -q -za99 -bt=os2 -bd -ei -fpi87 -zp1 -d0 -fp6 -ox -oi -wx -zl &
         -zm -zls -6s -mf -s -i=$(DDK4PATH)\h;$(OWPATH)\H\OS2;$(OWPATH)\H;

C16FLAGS = -q -za99 -bt=os2 -bd -ei -fpi87 -zp1 -d0 -fp6 -ox -oi -wx -zl &
           -zm -zls -nt=_TEXT16 -i=$(DDK4PATH)\h;$(OWPATH)\H\OS21X;$(OWPATH)\H;

ASMFLAGS = -q -i=$(DDKPATH)\base\inc;

BINFILE = $(BINPATH)\$(NAME).sys
LNKFILE = $(NAME).lnk
MAPFILE = $(NAME).map

OBJS = entry.obj ioctl.obj strat.obj init.obj

!ifndef DDK4PATH

$(BINFILE): .EXISTSONLY
  echo WARNING! A $(BINFILE) file must exist or the DDK4PATH variable must be specified in global.mif.

!else

$(BINFILE): $(BINPATH) $(OBJS) $(LNKFILE)
  $(INFLINK)
  $(LINK) @$(LNKFILE)

!endif

$(LNKFILE): makefile
  @%create $@
  @%append $@ name $(BINFILE)
  @%append $@ format os2 LX PHYSDEVICE
  @%append $@ option nostub
  @%append $@ option quiet
  @%append $@ option mixed1632
  @%append $@ option align=4
  @%append $@ option stack=0
  @%append $@ option map=$(NAME)
  @%append $@ order clname DATA segment _DDHEADER
  @%append $@ clname CODE segment _TEXT16 segment _TEXT segment _TEXTEND segment _TEXTINIT
  @%append $@ file {$(OBJS)}
  @%append $@ segment type DATA SHARED PRELOAD
  @%append $@ segment class CODE PRELOAD
  @%append $@ IMPORT Dos32FlatDS DOSCALLS.370
  @%append $@ IMPORT DOSPUTMESSAGE DOSCALLS.383
  @%append $@ library $(DDK4PATH)\lib\kee.lib
!ifdef DATEUTIL
  @$(DATEUTIL) +"OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(BLCOMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTHOR):$(VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(BLCOMMENT)'
!endif

.c16.obj: .AUTODEPEND
  $(CC16) $*.c16 $(C16FLAGS)

.asm.obj: .AUTODEPEND
  $(ASMC) $[* $(ASMFLAGS)

$(BINPATH): .EXISTSONLY
  md $(BINPATH)

clean: .SYMBOLIC
  @if exist *.obj del *.obj
  @if exist *.err del *.err
  @if exist $(LNKFILE) del $(LNKFILE)
  @if exist $(NAME).map del $(NAME).map
!ifdef DDK4PATH
  @if exist $(BINFILE) del $(BINFILE)
!endif
