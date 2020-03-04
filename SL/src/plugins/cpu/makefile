# NAME - required
NAME = cpu

# AUTHOR - optional
#AUTHOR = Andrey Vasilkin

# VER_xxxxx - optional
VER_MAJOR    = 1
VER_MINOR    = 3
VER_REVISION = 0

# COMMENT - optional
COMMENT = SystemLoad plugin. CPU monitor.

# LIBPATH - optional
#LIBPATH = .\

# LIBS - optional
#LIBS =

# SRCS - required
SRCS = cpu.c properties.c ctxmenu.c termzone.c features.c

# RCFILE - optional
RCFILE = cpu.rc

# IPF - optional
# Source of help file. Will be compiled to .hlp
IPFFILE = cpu.ipf

# INCPATH - optional
# Header files from ACPI Toolkit.
INCPATH +=.\h;

# CFLAGS - optional
# ACPI Toolkit samples uses flag -zp4.
CFLAGS += -zp4

# DEBUGCODE - optional
#DEBUGCODE = yes

!include ..\plugin.mif

