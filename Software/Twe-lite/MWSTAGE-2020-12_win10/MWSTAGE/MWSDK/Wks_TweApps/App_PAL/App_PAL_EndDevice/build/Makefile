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
#include ../../Version.mk

BOARD ?= 

ifeq ($(BOARD),OTA)
include ../Version.mk
else
  ifeq ($(BOARD),CUE)
    include ../Version.mk
  else
    VERSION_MAIN = 1
    VERSION_SUB  = 3
    VERSION_VAR  = 2

    # 1.3.1 ホールIC起点でOTAする機能を実装(CUEのみ)
    # 1.3.0 CUEに対応
    # 1.2.1 送信間隔の刻みを1秒に修正
    # 1.2.0 NOTICE PALに対応
    # 1.1.0 ビルドライブラリの修正
    # 1.0.2 加速度の計算が間違っている点を修正
    # 1.0.1 MOT PALに対応
    # 1.0.0 リリース版
    # 0.1.0 App_TagをベースにTWELITE PAL用のピン配置などを定義し、ピンの機能に応じて初期化などを行った
  endif
endif
 
#####################################################################
### set an additional source file
###   the default file name is dirname.
 
### for C files compiled with gcc (must have .c suffix)
APPSRC += common.c
APPSRC += flash.c

APPSRC += adc.c
APPSRC += SMBus.c
APPSRC += SPI.c
APPSRC += SHTC3.c
APPSRC += LTR308ALS.c
APPSRC += MC3630.c
APPSRC += PCA9632.c
APPSRC += accel_event.c
APPSRC += sensor_driver.c

APPSRC += AddrKeyAry.c
APPSRC += Interactive.c
APPSRC += input_string.c

APPSRC += sercmd_ascii.c
APPSRC += sercmd_chat.c
APPSRC += sercmd_plus3.c
APPSRC += sercmd_binary.c

APPSRC += ProcessEv_NOC.c
APPSRC += ProcessEv_MAG.c
APPSRC += ProcessEv_AMB.c
APPSRC += ProcessEv_MOT.c
APPSRC += ProcessEv_MOT_Event.c
APPSRC += ProcessEv_Accel_Event.c
### Additional Src/Include Path
# if set, find source files from given dirs.
#


ifeq ($(BOARD),OTA)
  APPSRC += ProcessEv_ConfigMaster.c

  APP_COMMON_SRC_DIR_ADD1 = ../App_PAL/App_PAL_EndDevice/source
  APP_COMMON_SRC_DIR_ADD2 = ../App_PAL/Common/Sensor
  APP_COMMON_SRC_DIR_ADD3 = ../App_PAL/Common/Source
  APP_COMMON_SRC_DIR_ADD4 = ../App_PAL/Common/Source_User

  CFLAGS += -DUSE_CUE -DOTA
else
  ifeq ($(BOARD),CUE)
    APPSRC += ProcessEv_CUE.c
    APPSRC += ProcessEv_CUEConfig.c
    APPSRC += ProcessEv_OTA.c

    APP_COMMON_SRC_DIR_ADD1 = ../App_PAL/App_PAL_EndDevice/source
    APP_COMMON_SRC_DIR_ADD2 = ../App_PAL/Common/Sensor
    APP_COMMON_SRC_DIR_ADD3 = ../App_PAL/Common/Source
    APP_COMMON_SRC_DIR_ADD4 = ../App_PAL/Common/Source_User

    CFLAGS += -DUSE_CUE
  else
    APPSRC += ProcessEv_LED.c
    APPSRC += ProcessEv_Config.c
    APP_COMMON_SRC_DIR_ADD1 = ../../../App_PAL/App_PAL_EndDevice/source
    APP_COMMON_SRC_DIR_ADD2 = ../../../App_PAL/Common/Sensor
    APP_COMMON_SRC_DIR_ADD3 = ../../../App_PAL/Common/Source
    APP_COMMON_SRC_DIR_ADD4 = ../../../App_PAL/Common/Source_User
  endif
endif
 
#####################################################################
### set misc option for compiler
 
### C flags passed to gcc
# e.g. CFLAGS += -DMY_DEFS
CFLAGS += -DDEBUG_OUTPUT
CFLAGS += -DENDDEVICE

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
