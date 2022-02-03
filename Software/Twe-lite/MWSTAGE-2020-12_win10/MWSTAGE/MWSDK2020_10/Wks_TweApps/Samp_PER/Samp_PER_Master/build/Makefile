##############################################################################
# Copyright (C) 2019 Mono Wireless Inc. All Rights Reserved.
# Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE
# AGREEMENT). 
##############################################################################
# USER PROJECT BUILD DEFINITION.
##############################################################################

#####################################################################
### set TWELITE model
TWELITE ?= BLUE
#TWELITE ?= RED

### OPTION SELECT
# オプション
USE_LCD ?= 1
NO_CCA ?= 0

#####################################################################
### set application version (MUST SET THIS.)
include ../../Version.mk

#####################################################################
### set an additional source file
###   the default file name is dirname.

### for C files compiled with gcc (must have .c suffix)

ifeq ($(USE_LCD),1)
APPSRC += LcdFont.c
APPSRC += LcdPrint.c
APPSRC += LcdDraw.c
APPSRC += LcdDriver.c
APPSRC += LcdExtras.c
CFLAGS += -DUSE_LCD
endif

### Additional Src/Include Path
# if set, find source files from given dirs.
#
APP_COMMON_SRC_DIR_ADD1 = ../../Common
#APP_COMMON_SRC_DIR_ADD2 = 
#APP_COMMON_SRC_DIR_ADD3 = 
#APP_COMMON_SRC_DIR_ADD4 = 

#####################################################################
### set misc option for compiler

### C flags passed to gcc
# e.g. CFLAGS += -DMY_DEFS
CFLAGS += -DDEBUG_OUTPUT

ifeq ($(USE_LCD),0)
	OBJDIR_SUFF += _NOLCD
	TARGET_SUFF += _NOLCD
endif

ifeq ($(NO_CCA),1)
	CFLAGS += -DNO_CCA
	OBJDIR_SUFF += _NOCCA
	TARGET_SUFF += _NOCCA
endif

### include opts
# e.g. INCFLAGS += -I../my_common_src/
#INCFLAGS +=

### optimize flag (default is -Os, normally no need to change)
#OPTFLAG=-O2

#####################################################################
### set MWSDK install path and include makefile.
MWSDK_PATH=$(realpath $(MWSDK_ROOT))
### must include mwx.mk (the makefile body part.)
include $(MWSDK_PATH)/MkFiles/mw.mk
#####################################################################