# PLUGIN - required
PLUGIN = process

# AUTOR - required
AUTOR = Andrey Vasilkin

# VERSION - optional
VERSION = 1.0.0

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
#DEBUGCODE = YES


!include ..\plugin.mif

# Need -zp2 for bsedos.h ...
sysstate.obj: sysstate.c
  wcc386 $*.c $(CFLAGS) -zp2
