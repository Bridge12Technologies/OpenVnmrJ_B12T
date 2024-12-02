#!/bin/python3
import warnings
# try to import serial
try:
	import serial
except ImportError as ie:
	warnings.warn("Could not Import serial (pyserial: https://pypi.org/project/pyserial/), to set the attenuation pyserial is needed!")
	exit(1)
import sys
import pickle
import time
from pathlib import Path

# basefolder of pickled comport file
# usually /vnmr/tmp
basefolder=Path('/vnmr/tmp')
# filename to search for
COMPORT_FILENAME='B12T_SCN_ATTENUATOR_CNTRL_PORT'
# corresponding file
comport_file = basefolder.joinpath(COMPORT_FILENAME)

#define device type
checkString='Adaura Technologies R3 Series 1-Channel RF Attenuator (95dB, 6GHz)'

#define string to test for sucess
success='Channel 1 successfully set to'

connectID='INFO\n'

#get commands
cmd=''
for k in sys.argv[1:]:
    cmd+=k.strip()+' '
cmd=cmd.strip()
cmd=cmd.split(';')
for ind,sub_cmd in enumerate(cmd):
    sub_cmd='SET 1 '+sub_cmd.strip() # hack for attenuator control
    if not sub_cmd.endswith('\n'):
        sub_cmd+='\n'
        cmd[ind]=sub_cmd

# try to find pickled data of comports
def get_comport(filename:str,searchCMD:str='idn?\n',checkString:str=checkString,force:bool=False):
    try:
        if force:
            raise FileNotFoundError
        with open(filename,'rb') as f:
            comports=pickle.load(f)
            return comports
    # now look for the correct comport
    except FileNotFoundError as e:
        import serial.tools.list_ports
        PossiblePorts=serial.tools.list_ports.comports()
        comports=[]
        for portInfo in PossiblePorts:
            with serial.Serial(timeout=0.1) as ser:
                ser.port=portInfo.device
                ser.open()
                try:
                    ser.write(searchCMD.encode())
                except serial.serialutil.SerialTimeoutException:
                    ser.close()
                    continue
                time.sleep(0.01) #to get a longer answer
                n_waiting=ser.in_waiting
                data=ser.read(n_waiting).decode('utf-8')
                if checkString in data:
                    comports.append(portInfo.device)
                ser.close()
        with open(filename,'wb') as f:
            pickle.dump(comports,f)
    return comports


def sendCMD(comports:list,cmd:list,sendCheck:str='',nmax=max(int(2*len(success)),128) ):
    exit_error_flag=0
    if len(comports)==0:
        raise ValueError
    data="-1"
    for port in comports:
        serialC=serial.Serial(timeout=0.025)
        serialC.port=port
        serialC.baudrate=115200 #B12T standard
        with serialC as ser:
            for ind,sub_cmd in enumerate(cmd):
                serialC.write(sub_cmd.encode())
                data=serialC.read(nmax).decode()
                if not (sendCheck in data):
                    exit_error_flag-=2**ind
    return data,exit_error_flag

###
### MAIN FUNCTIONALIY
###


comports=get_comport(comport_file,searchCMD=connectID,checkString=checkString)
exit_error_flag=0

try:
    data,exit_error_flag=sendCMD(comports,cmd,success)
except Exception as e:
    warnings.warn("Exception {0} occured when trying to write to comports {1}".format(e,comports))
    try:
        comports=get_comport(comport_file,searchCMD=connectID,checkString=checkString,force=True)
        data,exit_error_flag=sendCMD(comports,cmd,success)
        if exit_error_flag<= -1:
            import glob
            import os
            pnames=glob.glob('../tmp/B12T_SCN*')
            for k in pnames:
                try:
                    os.remove(pnames)
                except OSError:
                    pass
        comports=get_comport(comport_file,searchCMD=connectID,checkString=checkString,force=True)
        data,exit_error_flag=sendCMD(comports,cmd,success)
    except Exception as e2:
        warnings.warn("Exception {0} occured when trying to write to rechecked comports {1}, setting of the device failed".format(e,comports))
        if exit_error_flag==0:
            exit_error_flag= -999

if exit_error_flag<0:
    print(str(exit_error_flag))
    warnings.warn("exit_error_flag was raised, setting of device failed")
    exit(exit_error_flag)
else:
    print(data.strip().strip("'").strip('"').strip('"').strip("'").strip())
    exit(0)