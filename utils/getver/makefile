BINPATH = ..\..\bin
BINFILE = getver.exe

# --- Signature ---
AUTOR = Andrey Vasilkin
VERSION = 1.0.0
COMMENT = System Load utility. Version signature reader.

CFLAGS = -i=$(%WATCOM)\H\OS2;$(%WATCOM)\H -bt=os2 -q -w2 -d0

$(BINPATH)\$(BINFILE): getver.obj bldlevel.obj getver.lnk
  wlink @getver.lnk

getver.lnk: .ALWAYS
  @%create $@
  @%append $@ SYSTEM os2v2
  @%append $@ NAME $(BINPATH)\$(BINFILE)
  @%append $@ OPTION QUIET
  @%append $@ OPTION OSNAME='OS/2 and eComStation'
  @%append $@ FILE getver.obj
  @%append $@ FILE bldlevel.obj
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

.c.obj:
  wcc386 $(CFLAGS) $<

clean: .SYMBOLIC
  @if exist *.obj @del *.obj
  @if exist getver.lnk @del getver.lnk

