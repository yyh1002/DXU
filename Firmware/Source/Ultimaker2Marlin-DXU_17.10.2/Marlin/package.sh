#!/usr/bin/env bash

# This script is to package the Marlin package for Arduino
# This script should run under Linux and Mac OS X, as well as Windows with Cygwin.

#############################
# CONFIGURATION
#############################

##Which version name are we appending to the final archive
export BUILD_NAME=17.09

#############################
# Actual build script
#############################

if [ -z `which make` ]; then
	MAKE=mingw32-make
else
	MAKE=make
fi


# Change working directory to the directory the script is in
# http://stackoverflow.com/a/246128
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# #For building under MacOS we need gnutar instead of tar
# if [ -z `which gnutar` ]; then
# 	TAR=tar
# else
# 	TAR=gnutar
# fi

#############################
# Build the required firmwares
#############################

if [ -d "D:/arduino-1.8.1" ]; then
	ARDUINO_PATH=D:/arduino-1.8.1
	ARDUINO_VERSION=181
elif [ -d "/Applications/Arduino.app/Contents/Resources/Java" ]; then
	ARDUINO_PATH=/Applications/Arduino.app/Contents/Resources/Java
	ARDUINO_VERSION=105
elif [ -d "D:/Arduino" ]; then
	ARDUINO_PATH=D:/Arduino
	ARDUINO_VERSION=165
else
	ARDUINO_PATH=/usr/share/arduino
	ARDUINO_VERSION=105
fi


#Build the Ultimaker2 firmwares.
# gitClone https://github.com/TinkerGnome/Ultimaker2Marlin.git _Ultimaker2Marlin
# cd _Ultimaker2Marlin/Marlin

# USE_CHANGE_TEMPERATURE

# read optional tool change scripts from sd card
#define TCSDSCRIPT

# -----------------------
# UM2+
# -----------------------
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2Dual clean
sleep 2
mkdir _Mark2Dual
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2Dual DEFINES="'STRING_CONFIG_H_AUTHOR=\"Tinker_Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 FILAMENT_SENSOR_PIN=30 BABYSTEPPING UM2PLUS TCSDSCRIPT HOTEND_FAN2_PIN=-1 FAN2_PIN=-1 DEFAULT_POWER_BUDGET=160 HEATER_0_MAXTEMP=315 HEATER_1_MAXTEMP=315 HEATER_2_MAXTEMP=315 'EEPROM_VERSION=\"V12\"'"

cp _Mark2Dual/Marlin.hex resources/firmware/Tinker-Mark2-dual-${BUILD_NAME}.hex


$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualFan clean
sleep 2
mkdir _Mark2DualFan
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualFan DEFINES="'STRING_CONFIG_H_AUTHOR=\"Tinker_Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 FILAMENT_SENSOR_PIN=30 BABYSTEPPING UM2PLUS TCSDSCRIPT HOTEND_FAN2_PIN=-1 FAN2_PIN=8 DEFAULT_POWER_BUDGET=160 HEATER_0_MAXTEMP=315 HEATER_1_MAXTEMP=315 HEATER_2_MAXTEMP=315 'EEPROM_VERSION=\"V12\"'"

cp _Mark2DualFan/Marlin.hex resources/firmware/Tinker-Mark2-dual-fan-${BUILD_NAME}.hex


$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExpansionBoard clean
sleep 2
mkdir _Mark2DualExpansionBoard
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExpansionBoard DEFINES="'STRING_CONFIG_H_AUTHOR=\"Tinker_Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 FILAMENT_SENSOR_PIN=30 BABYSTEPPING UM2PLUS TCSDSCRIPT FAN2_PIN=6 HOTEND_FAN2_PIN=14 DEFAULT_POWER_BUDGET=160 HEATER_0_MAXTEMP=315 HEATER_1_MAXTEMP=315 HEATER_2_MAXTEMP=315 'EEPROM_VERSION=\"V12\"'"

cp _Mark2DualExpansionBoard/Marlin.hex resources/firmware/Tinker-Mark2-expansion-board-${BUILD_NAME}.hex



# -----------------------
# UM2ext+
# -----------------------
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtended clean
sleep 2
mkdir _Mark2DualExtended
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtended DEFINES="'STRING_CONFIG_H_AUTHOR=\"Tinker_Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 FILAMENT_SENSOR_PIN=30 BABYSTEPPING UM2PLUS TCSDSCRIPT HOTEND_FAN2_PIN=-1 FAN2_PIN=-1 Z_MAX_POS=325 DEFAULT_POWER_BUDGET=160 HEATER_0_MAXTEMP=315 HEATER_1_MAXTEMP=315 HEATER_2_MAXTEMP=315 'EEPROM_VERSION=\"V12\"'"

cp _Mark2DualExtended/Marlin.hex resources/firmware/Tinker-Mark2-extended-dual-${BUILD_NAME}.hex


$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtendedFan clean
sleep 2
mkdir _Mark2DualExtendedFan
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtendedFan DEFINES="'STRING_CONFIG_H_AUTHOR=\"Tinker_Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 FILAMENT_SENSOR_PIN=30 BABYSTEPPING UM2PLUS TCSDSCRIPT HOTEND_FAN2_PIN=-1 FAN2_PIN=8 Z_MAX_POS=325 DEFAULT_POWER_BUDGET=160 HEATER_0_MAXTEMP=315 HEATER_1_MAXTEMP=315 HEATER_2_MAXTEMP=315 'EEPROM_VERSION=\"V12\"'"

cp _Mark2DualExtendedFan/Marlin.hex resources/firmware/Tinker-Mark2-extended-dual-fan-${BUILD_NAME}.hex


$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtExpansionBoard clean
sleep 2
mkdir _Mark2DualExtExpansionBoard
$MAKE -j 3 HARDWARE_MOTHERBOARD=72 ARDUINO_INSTALL_DIR=${ARDUINO_PATH} ARDUINO_VERSION=${ARDUINO_VERSION} BUILD_DIR=_Mark2DualExtExpansionBoard DEFINES="'STRING_CONFIG_H_AUTHOR=\"Tinker_Mark2_${BUILD_NAME}\"' TEMP_SENSOR_1=20 EXTRUDERS=2 FILAMENT_SENSOR_PIN=30 BABYSTEPPING UM2PLUS TCSDSCRIPT FAN2_PIN=6 HOTEND_FAN2_PIN=14 Z_MAX_POS=325 DEFAULT_POWER_BUDGET=160 HEATER_0_MAXTEMP=315 HEATER_1_MAXTEMP=315 HEATER_2_MAXTEMP=315 'EEPROM_VERSION=\"V12\"'"

cp _Mark2DualExtExpansionBoard/Marlin.hex resources/firmware/Tinker-Mark2-extended-expansion-board-${BUILD_NAME}.hex
