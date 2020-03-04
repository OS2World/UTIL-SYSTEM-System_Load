# NAME - required
NAME = process

# AUTHOR - optional
#AUTHOR = Andrey Vasilkin

# VER_xxxxx - optional
VER_MAJOR    = 1
VER_MINOR    = 1
VER_REVISION = 0

# COMMENT - optional
COMMENT = SystemLoad plugin. Processes monitor.

# LIBPATH - optional
#LIBPATH = D:\my\lib\path

# LIBS - optional
#LIBS = 

# SRCS - required
SRCS = process.c sysstate.c explorer.c bldlevel.c properties.c

# RCFILE - optional
RCFILE = process.rc

# CFLAGS - optional
#CFLAGS =

# DEBUGCODE - optional
#DEBUGCODE = yes

!include ..\plugin.mif

# Need -zp2 for bsedos.h ...
sysstate.obj: sysstate.c
  $(CC) $*.c $(CFLAGS) -zp2
