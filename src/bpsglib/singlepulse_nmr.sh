#!/bin/bash
# *********************************************************
# 	singlepulse_nmr_example.bat 
# 	This file is intended as an example of using the singlepulse_nmr.exe executable with a batch file.

# 	SpinCore Technologies, Inc.
# 	2017/07/07 11:00:00
# *********************************************************


# FOR /F "TOKENS=1* DELIMS= " %%A IN ('DATE/T') DO SET CDATE=%%B
# FOR /F "TOKENS=1,2 eol=/ DELIMS=/ " %%A IN ('DATE/T') DO SET mm=%%B
# FOR /F "TOKENS=1,2 DELIMS=/ eol=/" %%A IN ('echo %CDATE%') DO SET dd=%%B
# FOR /F "TOKENS=2,3 DELIMS=/ " %%A IN ('echo %CDATE%') DO SET yyyy=%%B
# SET date=%yyyy%%mm%%dd% 

# TITLE SpinCore RadioProcessor Singlepulse NMR Example

# set -x
# Set arguments

BOARD_NUMBER=0
BLANK_BIT=2
ADC_FREQUENCY=75.0
ENABLE_TX=1
USE_SHAPE=0
PULSE90_PHASE=0
ENABLE_RX=1
BYPASS_FIR=1
BLANKING_DELAY=100.00

FILE_NAME=$1
shift
NUMBER_POINTS=$1
shift
NUMBER_OF_SCANS=$1
shift
SPECTROMETER_FREQUENCY=$1
shift
SPECTRAL_WIDTH=$1
shift
PULSE90_TIME=$1
shift
AMPLITUDE=$1
shift
REPETITION_DELAY=$1
shift
TRANS_DELAY=$1
shift
DEBUG=$1


# ------------------------------------BOARD SETTINGS------------------------------------



# FILE_NAME is the name of the output file the data will be acquired data will be stored in. File extensions will be appended automatically.

# Board Parameters

# BOARD_NUMBER is the number of the board in your system to be used by spinnmr. If you have multiple boards attached to your system, please make sure this value is correct.

# BLANK_BIT specifies which TTL Flag to use for the power amplifier blanking signal.
# Refer to your products Owner's Manual for additional information

# DEBUG Enables the debug output log.


# Frequency Parameters

# ADC_FREQUENCY (MHz) is the analog to digital converter frequency of the RadioProcessor board selected.

# SPECTROMETER_FREQUENCY (MHz) must be between 0 and 100.

# SPECTRAL_WIDTH (kHz) must be between 0.150 and 10000


# Pulse Parameters

# If ENABLE_TX is set to 0, the transmitter is disabled. If it is set to 1, the transmitter is enabled.

# USE_SHAPE will control the shaped pulse feature of the RadioProcessor. Setting SHAPED_PULSE to 1 will enable this feature. 0 will disabled this feature.

# AMPLITUDE of the excitation signal. Must be between 0.0 and 1.0.

# PULSE90_TIME (microseconds) must be atleast 0.065.

# PULSE_PHASE (degrees) must be greater than or equal to zero.



# Acquisition Parameters

# If ENABLE_RX is set to 0, the receiver is disabled. If it is set to 1, the receiver is enabled.

# BYPASS_FIR will disabled the FIR filter if set to 1. Setting BYPASS_FIR to 0 will enable the FIR filter.

# NUMBER_POINTS is the number of NMR data points the board will acquire during the scan. It must be between 0 and 16384. If it is not a power of 2, it will be rounded up to the nearest power of 2.

# NUMBER_OF_SCANS is the number of consecutive scans to run. There must be at least one scan. Due to latencies, scan count may not be consecutive.



# Delay Parameters

# TTL Blanking Delay (in milliseconds) must be atleast 0.000065.

# TRANS_DELAY (microseconds) must be atleast 0.065.

# REPETITION_DELAY (s)  is the time between each consecutive scan. It must be greater than 0.


# ------------------------------------END BOARD SETTINGS---------------------------------

seq=$(basename $0 .sh)

echo "/vnmr/psglib/$seq $FILE_NAME $BOARD_NUMBER $BLANK_BIT $DEBUG $ADC_FREQUENCY $SPECTROMETER_FREQUENCY $SPECTRAL_WIDTH $ENABLE_TX $USE_SHAPE $AMPLITUDE $PULSE90_TIME $PULSE90_PHASE $ENABLE_RX $BYPASS_FIR $NUMBER_POINTS $NUMBER_OF_SCANS $BLANKING_DELAY $TRANS_DELAY $REPETITION_DELAY"
/vnmr/psglib/$seq $FILE_NAME $BOARD_NUMBER $BLANK_BIT $DEBUG $ADC_FREQUENCY $SPECTROMETER_FREQUENCY $SPECTRAL_WIDTH $ENABLE_TX $USE_SHAPE $AMPLITUDE $PULSE90_TIME $PULSE90_PHASE $ENABLE_RX $BYPASS_FIR $NUMBER_POINTS $NUMBER_OF_SCANS $BLANKING_DELAY $TRANS_DELAY $REPETITION_DELAY
# PAUSE
