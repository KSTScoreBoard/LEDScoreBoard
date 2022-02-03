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

APPSRC += adc.c
APPSRC += SMBus.c
APPSRC += SPI.c
APPSRC += BH1715.c
APPSRC += SHT21.c
APPSRC += SHT31.c
APPSRC += SHTC3.c
APPSRC += BME280.c
APPSRC += S1105902.c
APPSRC += ADT7410.c
APPSRC += ADXL345.c
APPSRC += ADXL345_LowEnergy.c
APPSRC += ADXL345_AirVolume.c
APPSRC += ADXL345_FIFO.c
APPSRC += MPL115A2.c
APPSRC += LIS3DH.c
APPSRC += L3GD20.c
APPSRC += TSL2561.c
APPSRC += MAX31855.c
APPSRC += sensor_driver.c

APPSRC += Interactive.c
APPSRC += input_string.c

APPSRC += sercmd_ascii.c
APPSRC += sercmd_chat.c
APPSRC += sercmd_plus3.c
APPSRC += sercmd_binary.c

APPSRC += ProcessEv_Standard.c
APPSRC += ProcessEv_Button.c
APPSRC += ProcessEv_Config.c
APPSRC += ProcessEv_ConfigMaster.c
APPSRC += ProcessEv_Door_Timer.c
APPSRC += ProcessEv_Door_Timer_Nwk.c
APPSRC += ProcessEv_SHT21.c
APPSRC += ProcessEv_SHT31.c
APPSRC += ProcessEv_SHTC3.c
APPSRC += ProcessEv_BME280.c
APPSRC += ProcessEv_S1105902.c
APPSRC += ProcessEv_ADT7410.c
APPSRC += ProcessEv_TSL2561.c
APPSRC += ProcessEv_ADXL345.c
APPSRC += ProcessEv_ADXL345_LowEnergy.c
APPSRC += ProcessEv_ADXL345_AirVolume.c
APPSRC += ProcessEv_ADXL345_FIFO.c
APPSRC += ProcessEv_MPL115A2.c
APPSRC += ProcessEv_LIS3DH.c
APPSRC += ProcessEv_L3GD20.c
APPSRC += ProcessEv_MAX31855.c
APPSRC += ProcessEv_UART.c
APPSRC += ProcessEv_MultiSensor.c

### Additional Src/Include Path
# if set, find source files from given dirs.
#
APP_COMMON_SRC_DIR_ADD1 = ../../App_Tag_EndDevice_Input/source
APP_COMMON_SRC_DIR_ADD2 = ../../Common/Sensor
APP_COMMON_SRC_DIR_ADD3 = ../../Common/Source
APP_COMMON_SRC_DIR_ADD4 = ../../Common/Source_User
 
#####################################################################
### set misc option for compiler
 
### C flags passed to gcc
# e.g. CFLAGS += -DMY_DEFS
CFLAGS += -DDEBUG_OUTPUT
CFLAGS += -DENDDEVICE_INPUT

BOARD ?=

ifeq ($(BOARD),LITE2525A)
  CFLAGS += -DLITE2525A
  OBJDIR_SUFF += _LITE2525A
  TARGET_SUFF += _LITE2525A
endif
ifeq ($(BOARD),SWING)
  CFLAGS += -DSWING
  OBJDIR_SUFF += _SWING
  TARGET_SUFF += _SWING
endif
ifeq ($(BOARD),OTA)
  CFLAGS += -DOTA -DUSE_MONOSTICK
  OBJDIR_SUFF += _OTA
  TARGET_SUFF += _OTA
endif

### include opts
# e.g. INCFLAGS += -I../my_common_src/
#INCFLAGS +=

LDLIBS += m
 
### optimize flag (default is -Os, normally no need to change)
#OPTFLAG=-O2
 
#####################################################################
### set MWSDK install path and include makefile.
MWSDK_PATH=$(realpath $(MWSDK_ROOT))
### must include mwx.mk (the makefile body part.)
include $(MWSDK_PATH)/MkFiles/mw.mk
#####################################################################
