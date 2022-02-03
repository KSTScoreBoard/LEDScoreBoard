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

#####################################################################
### set application version (MUST SET THIS.)
VERSION_MAIN = 1
VERSION_SUB  = 3
VERSION_VAR  = 1

#####################################################################
### set an additional source file
###   the default file name is dirname.

### for C files compiled with gcc (must have .c suffix)
APPSRC += Interactive.c
APPSRC += common.c
APPSRC += flash.c
APPSRC += modbus_ascii.c
APPSRC += input_string.c
APPSRC += duplicate_checker.c
APPSRC += Pairing.c


### Additional Src/Include Path
# if set, find source files from given dirs.
#
APP_COMMON_SRC_DIR_ADD1 = ../source
#APP_COMMON_SRC_DIR_ADD2 = 
#APP_COMMON_SRC_DIR_ADD3 = 
#APP_COMMON_SRC_DIR_ADD4 =

#####################################################################
### set misc option for compiler

### C flags passed to gcc
# e.g. CFLAGS += -DMY_DEFS
CFLAGS += -DDEBUG_OUTPUT

ifeq ($(TWE_DEVKIT),002L)
  CFLAGS += -DUSE_DEV_KIT_002_L
  OBJDIR_SUFF += _KIT002L
  TARGET_SUFF += _KIT002L
endif

ifneq ($(VERSION_DESC),)
  TARGET_SUFF += $(VERSION_DESC)
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
