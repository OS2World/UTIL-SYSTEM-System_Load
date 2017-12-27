# makefile for make

THIRDPARTY=D:\dev

# beginning of the version is defined only here
BLDVERSION:=0.01

# these should be defined by the build caler
SVNREV?=nosvn
BLDVENDOR?=dev4
BLDDATE?=0000-00-00
BLDTIME?=00:00:00

ifneq ($(FORMAL_BUILD),1)
BLDDEVMARK:=.dev
else
BLDDEVMARK:=
endif

BUILDLVL:=@\#OS/4 Team:$(BLDVERSION).$(SVNREV)$(BLDDEVMARK)\#@\#\#1\#\# $(BLDDATE) $(BLDTIME)      $(BLDVENDOR)::::0::@@ OS/4 GRAD driver SVN $(SVNREV)

PRJ_NAME=RDMSR

PRJ_SRC=.
PRJ_BLDROOT=../../bld
PRJ_BLD=$(PRJ_BLDROOT)/$(PRJ_NAME)
PRJ_BIN=../../bin

AFLAGS  = -q -i=$(THIRDPARTY)/share/inc
ASM     = wasm $(AFLAGS)

CFLAGS  = -q -za99 -bt=os2 -bd -ei -fpi87 -zp1 -d0 -fp6 -6s -mf -ox -oi -s -wx -zl -zm -zls -i=$(THIRDPARTY)/share/h -i=$(THIRDPARTY)/watcom/h/os2
CC      = wcc386 $(CFLAGS)

CF16    = -q -za99 -bt=os2 -bd -ei -fpi87 -zp1 -d0 -fp6 -ox -oi -wx -zl -zm -zls -i=$(THIRDPARTY)/share/h -i=$(THIRDPARTY)/watcom/h/os2 -nt=_TEXT16
C16     = wcc $(CF16)

LFLAGS  = format os2 lx phys option quiet,nostub,mixed1632,map,align=4
LINK    = wlink

WAT2MSMAP=wat2map.cmd

OBJSUF=.obj
HSUF=.h
INCSUF=.inc
LIBSUF=.lib

$(PRJ_BLD)/%$(OBJSUF): $(PRJ_SRC)/%.asm
	$(subst /,\,$(ASM) -fo=$@ $<)

$(PRJ_BLD)/%$(OBJSUF): $(PRJ_SRC)/%.c
	$(subst /,\,$(CC) -fo=$@ $<)

$(PRJ_BLD)/%$(OBJSUF): $(PRJ_SRC)/%.c16
	$(subst /,\,$(C16) -fo=$@ $<)

# ========== upper level ==========
.PHONY: all clean dirs
all: # default target

# ========== RDMSR component ==========

RDMSR_NAME=RDMSR
RDMSR_SRC=$(PRJ_SRC)
RDMSR_BLD=$(PRJ_BLD)

RDMSR_OBJ=\
    $(RDMSR_BLD)/entry$(OBJSUF)\
    $(RDMSR_BLD)/Init$(OBJSUF)\
    $(RDMSR_BLD)/InitComplete$(OBJSUF)\
    $(RDMSR_BLD)/IOCTL$(OBJSUF)\
    $(RDMSR_BLD)/Strat$(OBJSUF)

RDMSR_BASEBLD=$(RDMSR_BLD)/$(RDMSR_NAME)
RDMSR_BASEBIN=$(PRJ_BIN)/$(RDMSR_NAME)

RDMSR_WMAP=$(RDMSR_BASEBLD).wmap
RDMSR_MSMAP=$(RDMSR_BASEBLD).msmap
RDMSR_LNK=$(RDMSR_BASEBLD).wlnk
RDMSR_SYM=$(RDMSR_BASEBIN).sym
RDMSR_SYS=$(RDMSR_BASEBIN).sys

$(RDMSR_LNK): Makefile
	@echo $(LFLAGS) > $@
	@echo option description '$(BUILDLVL)' >> $@
	@echo order              >> $@
	@echo clname DATA        >> $@
	@echo segment _DDHEADER  >> $@
	@echo clname CODE        >> $@
	@echo segment _TEXT16    >> $@
	@echo segment _TEXT      >> $@
	@echo segment _TEXTEND   >> $@
	@echo segment _TEXTINIT  >> $@
	@echo name $(RDMSR_SYS) >> $@
	@echo file {$(RDMSR_OBJ)} >> $@
	@echo option map=$(RDMSR_WMAP) >> $@
	@echo segment type DATA SHARED PRELOAD >> $@
	@echo segment class CODE PRELOAD       >> $@
	@echo IMPORT Dos32FlatDS DOSCALLS.370  >> $@
	@echo IMPORT DOSPUTMESSAGE DOSCALLS.383 >> $@
	@echo library $(THIRDPARTY)\share\lib\ddk\kee.lib >> $@

$(RDMSR_SYS): $(RDMSR_LNK) $(RDMSR_OBJ)
	$(LINK) @$(RDMSR_LNK)
#	$(WAT2MSMAP) $(RDMSR_WMAP) $(RDMSR_MSMAP)
#	cd $(subst /,\,$(dir $(RDMSR_MSMAP)).) && mapsym $(subst /,\,$(notdir $(RDMSR_MSMAP)))
#	mv -f $(dir $(RDMSR_MSMAP))$(notdir $(RDMSR_SYM)) $(RDMSR_SYM)
#	lxLite /C:kernel $(RDMSR_SYS)

# ========== upper level implementation ==========
all: dirs $(RDMSR_SYS)

DIRS=$(PRJ_BIN) $(PRJ_BLDROOT) $(PRJ_BLD)
$(DIRS):
	mkdir $(subst /,\,$@)

dirs: $(DIRS)

clean:
	rm -rf $(PRJ_BLD)
