# Copyright (c) 2015 SpinCore Technologies, Inc.
# http://www.spincore.com
#
# This software is provided 'as-is', without any express or implied warranty. 
# In no event will the authors be held liable for any damages arising from the 
# use of this software.
#
# Permission is granted to anyone to use this software for any purpose, 
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
#
# 1. The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software. If you use this software in a
# product, an acknowledgement in the product documentation would be appreciated
# but is not required.
# 2. Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 3. This notice may not be removed or altered from any source distribution.

"""
* Version altered by Karl Rieger
* Copyright(c) 2022 Bridge12Technologies
* -> includes search path for spinapi library
"""

import ctypes
#added
import pathlib
import sys
import warnings
import logging
import time
import builtins

handler=logging.StreamHandler(stream=sys.stdout)
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
handler.setFormatter(formatter)
handler.setLevel(logging.INFO)
#logger.addHandler(handler)

logging.basicConfig( level=logging.INFO,handlers=[handler])
logger=logging.getLogger(__name__)

spinapi=None
SPINAPIPATH="/home/Documents/Python/ovnmrj" # needed in spinapi
spinapi_path=pathlib.Path(SPINAPIPATH)
try:
	spinapi=ctypes.CDLL(str(spinapi_path.joinpath("libspinapi.so")))
except:
	try:
		spinapi=ctypes.CDLL(str(spinapi_path.joinpath("spinapi64")) )
	except:
		warnings.warn("spinapi not found in SPINAPIPATH {0}, trying to find it in executing folder".format(spinapi_path))
		spinapi=None

if spinapi is None:
	try:
		spinapi = ctypes.CDLL("libspinapi")
	except:
		try:
			spinapi = ctypes.CDLL("libspinapi.so")
		except:
			raise ImportError("Failed to load spinapi library.")

"""
Utility fuction by Karl Rieger @ Bridge12Technologies
"""
def chan_to_bit(chanlist:list):
    """
    channels that are to be turned on or off
    """

    logger.debug("chanlist is {0} with type {1}, the comparison type(chanlist)==type(__builtins__.list()) is {2}".format(chanlist,type(chanlist),type(chanlist)== builtins.list))
    if type(chanlist)!= builtins.list:
        chanlist=[chanlist]
        logger.info("chanlist is no list, assuming that it is a int ({0}).".format(chanlist))
    reslist=[0]*len(chanlist)
    for ind,channel_num in enumerate(chanlist):
        reslist[ind]=int( 2**(int(channel_num)) )
        if reslist[ind] in reslist[:ind]:
            warnings.warn('Be careful a channel has been selected twice this is not defined and most likely the pulseblaster will do nothing')
    return sum(reslist)


PULSE_PROGRAM = 0
FREQ_REGS = 1  

# User friendly names for the phase registers of the cos and sin channels
PHASE000 = 0
PHASE090 = 1
PHASE180 = 2
PHASE270 = 3

# User friendly names for the control bits
TX_ENABLE       =1
TX_DISABLE      =0
PHASE_RESET     =1
NO_PHASE_RESET  =0
DO_TRIGGER      =1
NO_TRIGGER      =0
NO_DATA         =0
NO_SHAPE        =0
AMP0            =0

STATUS_STOPPED = 0
STATUS_RESET = (1 << 1)
STATUS_RUNNING = (1<<2)
STATUS_WAITING = (1<<3)
STATUS_SCANNING = (1<<4)

#Defines for start_programming

FREQ_REGS      =1

PHASE_REGS    =2
TX_PHASE_REGS = 2
PHASE_REGS_1   =2

RX_PHASE_REGS  =3
PHASE_REGS_0   =3

# These are names used by RadioProcessor
COS_PHASE_REGS =51
SIN_PHASE_REGS =50

# For specifying which device in pb_dds_load
DEVICE_SHAPE =0x099000
DEVICE_DDS   =0x099001

#Defines for enabling analog output
ANALOG_ON =1
ANALOG_OFF= 0
TX_ANALOG_ON= 1
TX_ANALOG_OFF= 0
RX_ANALOG_ON =1
RX_ANALOG_OFF= 0

def enum(**enums):
    return type('Enum', (), enums)
		
ns = 1.0
us = 1000.0
ms = 1000000.0

MHz = 1.0
kHz = 0.001
Hz = 0.000001
		
#Instruction enum -> changed
Instruction = enum(
	CONTINUE = 0,
	STOP = 1,
	LOOP = 2,
	END_LOOP = 3,
	JSR = 4,
	RTS = 5,
	BRANCH = 6,
	LONG_DELAY = 7,
	WAIT = 8,
	RTI = 9
)
spinapi.pb_get_version.restype = (ctypes.c_char_p)
spinapi.pb_get_error.restype = (ctypes.c_char_p)

spinapi.pb_count_boards.restype = (ctypes.c_int)

spinapi.pb_init.restype = (ctypes.c_int)

spinapi.pb_select_board.argtype = (ctypes.c_int)
spinapi.pb_select_board.restype = (ctypes.c_int)

spinapi.pb_set_debug.argtype = (ctypes.c_int)
spinapi.pb_set_debug.restype = (ctypes.c_int)

spinapi.pb_set_defaults.restype = (ctypes.c_int)

spinapi.pb_core_clock.argtype = (ctypes.c_double)
spinapi.pb_core_clock.restype = (ctypes.c_int)

spinapi.pb_write_register.argtype = (ctypes.c_int, ctypes.c_int)
spinapi.pb_write_register.restype = (ctypes.c_int)

spinapi.pb_start_programming.argtype = (ctypes.c_int)
spinapi.pb_start_programming.restype = (ctypes.c_int)

spinapi.pb_stop_programming.restype = (ctypes.c_int)

spinapi.pb_start.restype = (ctypes.c_int)
spinapi.pb_stop.restype = (ctypes.c_int)
spinapi.pb_reset.restype = (ctypes.c_int)
spinapi.pb_close.restype = (ctypes.c_int)


spinapi.pb_inst_pbonly.argtype = (
        ctypes.c_int, #flags
	ctypes.c_int, #inst
	ctypes.c_int, #inst data
	ctypes.c_double, #length (double)
)
spinapi.pb_inst_pbonly.restype = (ctypes.c_int)


spinapi.pb_inst_radio.argtype = (
        ctypes.c_int, #Frequency register 
	ctypes.c_int, #Cosine phase
	ctypes.c_int, #Sin phase
	ctypes.c_int, #tx phase
	ctypes.c_int, #tx enable
	ctypes.c_int, #phase reset
	ctypes.c_int, #trigger scan
	ctypes.c_int, #flags
	ctypes.c_int, #inst
	ctypes.c_int, #inst data
	ctypes.c_double, #length (double)
)
spinapi.pb_inst_radio.restype = (ctypes.c_int)


spinapi.pb_inst_dds2.argtype = (
	ctypes.c_int, #Frequency register DDS0
	ctypes.c_int, #Phase register DDS0
	ctypes.c_int, #Amplitude register DDS0
	ctypes.c_int, #Output enable DDS0
	ctypes.c_int, #Phase reset DDS0
	ctypes.c_int, #Frequency register DDS1
	ctypes.c_int, #Phase register DDS1
	ctypes.c_int, #Amplitude register DDS1
	ctypes.c_int, #Output enable DDS1,
	ctypes.c_int, #Phase reset DDS1,
	ctypes.c_int, #Flags
	ctypes.c_int, #inst
	ctypes.c_int, #inst data
	ctypes.c_double, #timing value (double)
)
spinapi.pb_inst_dds2.restype = (ctypes.c_int)

###
# additional defines of argtypes
###
spinapi.pb_set_num_points.argtype=(ctypes.c_int)
spinapi.pb_set_num_points.restype = (ctypes.c_int) #???

spinapi.pb_set_scan_segments.argtype=(ctypes.c_int)
spinapi.pb_set_scan_segments.restype=(ctypes.c_int)

spinapi.pb_set_freq.argtype=(ctypes.c_double)
spinapi.pb_set_freq.restype=(ctypes.c_double)

spinapi.pb_inst_radio_shape.argtype=(
	ctypes.c_int, #Frequency register
	ctypes.c_int, #Cosine phase
	ctypes.c_int, #Sin phase
	ctypes.c_int, #tx phase
	ctypes.c_int, #tx enable
	ctypes.c_int, #phase reset
	ctypes.c_int, #trigger scan
	ctypes.c_int, #use_shape
	ctypes.c_int, #amp
	ctypes.c_int, #flags
	ctypes.c_int, #inst
	ctypes.c_int, #inst data
	ctypes.c_double, #length (double)
)
spinapi.pb_inst_radio_shape.restype = (ctypes.c_int)

spinapi.pb_read_status.restype = (ctypes.c_int32)

spinapi.pb_get_data.argtype = (ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int) )
spinapi.pb_get_data.restype = (ctypes.c_int)

spinapi.pb_scan_count.argtype=(ctypes.c_int)
spinapi.pb_scan_count.restype=(ctypes.c_int)

spinapi.pb_set_amp.argtype=(ctypes.c_double,ctypes.c_int)
spinapi.pb_set_amp.restype=(ctypes.c_int)

spinapi.pb_setup_filters.argtype=(ctypes.c_double,ctypes.c_int,ctypes.c_int)
spinapi.pb_setup_filters.restype=(ctypes.c_int)

###
# END OF ADDITIONAL ARGTYPES
###


def pb_get_version():
	"""Return library version as UTF-8 encoded string."""
	ret = spinapi.pb_get_version()
	return str(ctypes.c_char_p(ret).value.decode("utf-8"))

def pb_get_error():
	"""Return library error as UTF-8 encoded string."""
	ret = spinapi.pb_get_error()
	return str(ctypes.c_char_p(ret).value.decode("utf-8"))
	
def pb_count_boards():
	"""Return the number of boards detected in the system."""
	return spinapi.pb_count_boards()
	
def pb_init():
	"""Initialize currently selected board."""
	return spinapi.pb_init()
	
def pb_set_debug(debug):
	return spinapi.pb_set_debug(debug)
	
def pb_select_board(board_number):
	"""Select a specific board number"""
	return spinapi.pb_select_board(board_number)
	
def pb_set_defaults():
	"""Set board defaults. Must be called before using any other board functions."""
	return spinapi.pb_set_defaults()
	
def pb_core_clock(clock):
	return spinapi.pb_core_clock(ctypes.c_double(clock))
	
def pb_write_register(address, value):
	return spinapi.pb_write_register(address, value)
	
def pb_start_programming(target):
	return spinapi.pb_start_programming(target)

def pb_stop_programming():
	return spinapi.pb_stop_programming()

def pb_inst_pbonly(*args):
        t = list(args)
        #Argument 3 must be a double
        t[3] = ctypes.c_double(t[3])
        args = tuple(t)
        return spinapi.pb_inst_pbonly(*args)

def pb_inst_radio(*args):
        t = list(args)
        #Argument 10 must be a double
        t[10] = ctypes.c_double(t[10])
        args = tuple(t)
        return spinapi.pb_inst_radio(*args)
	
def pb_inst_dds2(*args):
	t = list(args)
	#Argument 13 must be a double
	t[13] = ctypes.c_double(t[13])
	args = tuple(t)
	return spinapi.pb_inst_dds2(*args)

def pb_start():
	return spinapi.pb_start()
	
def pb_stop():
	return spinapi.pb_stop()
	
def pb_reset(): 
	return spinapi.pb_reset()
	
def pb_close():
	return spinapi.pb_close()

#
# USED FOR RADIOPROCESSOR BOARD
#
def pb_scan_count(c_int):
    return spinapi.pb_set_num_points(c_int)

def pb_set_num_points(c_int):
	return spinapi.pb_set_num_points(c_int)

def pb_set_scan_segments(c_int):
	return spinapi.pb_set_scan_segments(c_int)

def pb_inst_radio_shape(*args):
	t=list(args)
	t[12]=ctypes.c_double(t[12])
	args=tuple(t)
	return spinapi.pb_inst_radio_shape(*args)

def pb_set_freq(c_double):
	ret=spinapi.pb_set_freq(ctypes.c_double(c_double))
	return ret

def pb_read_status():
    ret=bin(spinapi.pb_read_status())
    return ret

intP=ctypes.POINTER(ctypes.c_int)
def pb_get_data(*args):
    num_points=int(args[0])
    data_real = (ctypes.c_int * num_points)()
    data_imag = (ctypes.c_int * num_points)()
    ret = spinapi.pb_get_data(num_points, ctypes.cast(data_real,intP), ctypes.cast(data_imag,intP) )
    data_imag_python=[data_imag[k] for k in range(num_points)]
    data_real_python=[data_real[k] for k in range(num_points)]
    return ret, data_real_python, data_imag_python

def pb_set_amp(*args):
    t=list(args)
    t[0]=ctypes.c_double(t[0])
    args=tuple(t)
    ret=spinapi.pb_set_amp(*args)
    return ret

def pb_setup_filters(*args):
    t=list(args)
    t[0]=ctypes.c_double(t[0])
    args=tuple(t)
    ret=spinapi.pb_setup_filters(*args)
    return ret

