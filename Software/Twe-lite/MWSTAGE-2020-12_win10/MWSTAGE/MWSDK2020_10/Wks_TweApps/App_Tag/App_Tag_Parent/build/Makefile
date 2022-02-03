##############################################################################
# Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.
# Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE
# AGREEMENT).
##############################################################################
# USER PROJECT BUILD DEFINITION.
##############################################################################
 
#####################################################################
### set TWELITE model
TWELITE ?= BLUE
#TWELITE = RED
 
#####################################################################
### set application version (MUST SET THIS.)
include ../../Version.mk
 
#####################################################################
### set an additional source file
###   the default file name is dirname.
 
### for C files compiled with gcc (must have .c suffix)
APPSRC += common.c
APPSRC += flash.c
APPSRC += AddrKeyAry.c

APPSRC += input_string.c
APPSRC += Interactive.c
APPSRC += sercmd_ascii.c
APPSRC += sercmd_chat.c
APPSRC += sercmd_plus3.c
APPSRC += sercmd_binary.c

APPSRC += LcdDraw.c
APPSRC += LcdDriver.c
APPSRC += LcdExtras.c
APPSRC += LcdFont.c
APPSRC += LcdPrint.c

### Additional Src/Include Path
# if set, find source files from given dirs.
#
APP_COMMON_SRC_DIR_ADD1 = ../../App_Tag_Parent/source
APP_COMMON_SRC_DIR_ADD2 = ../../Common/Sensor
APP_COMMON_SRC_DIR_ADD3 = ../../Common/Source
APP_COMMON_SRC_DIR_ADD4 = ../../Common/Source_User

#####################################################################
### set misc option for compiler
 
### C flags passed to gcc
# e.g. CFLAGS += -DMY_DEFS
CFLAGS += -DDEBUG_OUTPUT
CFLAGS += -DPARENT

ifeq ($(TWE_BOARD_MODEL),MONOSTICK)
  CFLAGS += -DUSE_MONOSTICK
  OBJDIR_SUFF += _MONOSTICK
  TARGET_SUFF += _MONOSTICK
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
