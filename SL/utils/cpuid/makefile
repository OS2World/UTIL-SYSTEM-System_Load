BINPATH = ..\..\bin
BINFILE = $(BINPATH)\cpuid.exe

CFLAGS = -i=$(%WATCOM)\H\OS2;$(%WATCOM)\H -bt=os2 -q -d0 -w2

SRCS = cpuid.c

OBJS = $(SRCS:.c=.obj)

$(BINFILE): $(OBJS)
  wlink SYS os2v2 op q file {$(OBJS:.obj=)} name $*

.c.obj:
  wcc386 $(CFLAGS) $<

clean: .SYMBOLIC
  @if exist *.obj @del *.obj
  @if exist $(BINFILE) @del $(BINFILE)
