BINPATH = .
BINFILE = getver.exe

# --- Signature ---
AUTOR = Andrey Vasilkin
VERSION = 1.0.0
COMMENT = OS/2 filever.dll research.

CFLAGS = -i=$(%WATCOM)\H\OS2;$(%WATCOM)\H -bt=os2 -q -w2

$(BINPATH)\$(BINFILE): getver.obj getver.lnk
  wlink @getver.lnk

getver.lnk: .ALWAYS
  @%create $@
  @%append $@ SYSTEM os2v2
  @%append $@ NAME $(BINPATH)\$(BINFILE)
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
  @%append $@ LIB filever.lib
  @%append $@ FILE getver.obj
#
# Build signature for BLDLEVEL.EXE and GETVER.EXE.
#
!ifdef %osdir
  #
  # eComStation have cool utility C:\KLIBC\BIN\date.exe.
  # We can get current date here.
  #
#  @$(%osdir)\KLIBC\BIN\date +"OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$#%F               $(%HOSTNAME)::ru:RUS:::@@$(COMMENT)'" >>$^@
!else
#  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$#Sun Mar 01 01:30:00 2015 $(%HOSTNAME)::ru:RUS:0::@@$(COMMENT)'
!endif
  @%append $@ OPTION DESCRIPTION '@$#$(AUTOR):$(VERSION)$#@$#$#1$#$#Sun Mar 01 01:30:00 2015 $(%HOSTNAME)::ru:RUS:0::@@$(COMMENT)'

.c.obj:
  wcc386 $(CFLAGS) $<

.SILENT
clean: .SYMBOLIC
  if exist getver.obj del getver.obj
  if exist getver.lnk del getver.lnk
  if exist $(BINPATH)\$(BINFILE) del $(BINPATH)\$(BINFILE)
