BINPATH = ..\..\bin
BINFILE = rxgetver.dll

# --- Signature ---
AUTOR = Andrey Vasilkin
VERSION = 1.0.0
COMMENT = Version signature reader, REXX API.

CFLAGS = -i=$(%WATCOM)\H\OS2;$(%WATCOM)\H;..\getver -bt=os2 -q -w2 -d0 -bd

$(BINPATH)\$(BINFILE): rxgetver.obj bldlevel.obj rxgetver.lnk
  wlink @rxgetver.lnk
  if exist $(BINPATH)\rxgetver.cmd del $(BINPATH)\rxgetver.cmd
  copy rxgetver.cmd $(BINPATH) > nul

rxgetver.lnk: .ALWAYS
  @%create $@
  @%append $@ SYSTEM os2v2_dll INITINSTANCE TERMINSTANCE
  @%append $@ NAME $(BINPATH)\$(BINFILE)
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
  @%append $@ FILE rxgetver.obj
  @%append $@ FILE bldlevel.obj
  @%append $@ EXPORT GVLOADFUNCS             .1 = gvLoadFuncs
  @%append $@ EXPORT GVDROPFUNCS             .2 = gvDropFuncs
  @%append $@ EXPORT GVPARSE                 .3 = gvParse
  @%append $@ EXPORT GVGETFROMFILE           .4 = gvGetFromFile
#
# Build signature for BLDLEVEL.EXE and GETVER.EXE.
#
!ifdef %osdir
  #
  # eComStation have cool utility C:\KLIBC\BIN\date.exe.
  # We can get current date here.
  #
  @$(%osdir)\KLIBC\BIN\date +"OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$# %F %T      $(%HOSTNAME)::ru:RUS:::@@$(COMMENT)'" >>$^@
!else
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$#                          $(%HOSTNAME)::ru:RUS:0::@@$(COMMENT)'
!endif

bldlevel.obj: ..\getver\bldlevel.c
  wcc386 $(CFLAGS) -fo=.\ $<

.c.obj:
  wcc386 $(CFLAGS) $<

clean: .SYMBOLIC
  @if exist *.obj @del *.obj
  @if exist rxgetver.lnk @del rxgetver.lnk

