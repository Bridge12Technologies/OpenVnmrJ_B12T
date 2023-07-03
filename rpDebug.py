"""
#
#
# Small script to test tune mode
#
# NOTE: sopme setup is still missing! (use shape, exp amp? amp register?)
# NOTE: b12proc has shape=1, we use 0 for hard pulse
#
"""

import warnings
import logging
import sys
import time
import builtins

"""
SET UP LOGGING
"""
loglevel=logging.INFO

loggerLaserFlash=logging.getLogger(__name__)
loggerLaserFlash.setLevel(loglevel)
stdoutHandler = logging.StreamHandler(sys.stdout)
stdoutHandler.setLevel(loglevel)
loggerLaserFlash.addHandler(stdoutHandler)

#do spinapi stuff here
# spinapi needs to be in the path!
# needs to be modified spinapi to find the spinapi.dll
import spinapi as sapi#(iens)... hopefully not
ms=sapi.ms
us=sapi.us
ns=sapi.ns

###
# small aggregator class (struct)
###
class StructObj(object):
    def __init__(self,**kwargs):
        for key,val in kwargs.items():
            setattr(self,key,val)

###
#
# HARDCODED VALUES FOR TESTING
#
###
TUNESTART=10.0 #MHz
TUNESTOP=20.0 #MHz
NTUNE=3 #NSEGMENTS
DTUNE=(TUNESTOP-TUNESTART)/(NTUNE-1)

DELAY=500 #in ns #0.01 # from b12out
MIN_DELAY       =500.0  # nanosec
MTUNE_DELAY     =1000.0 # nanosec


MPS_BIT      =0
RECV_BIT     =1
AMP_BIT      =2
AMPBLANK_BIT =3

RECV_UNBLANK    =(1 << RECV_BIT)
AMP_UNBLANK     =(1 << AMP_BIT)
OUTBLANK        =(1 << AMPBLANK_BIT)


# FROM POWER 1 command
exps=StructObj(useShape = 0, useAmp = 1) #from POWER 1
globVars = StructObj(complex_points=NTUNE)

# tune mode example commands
# similar to https://github.com/Bridge12Technologies/OpenVnmrJ_B12T/blob/master/src/b12proc/b12proc.c

#sapi.pb_set_debug(1)
sapi_version=sapi.pb_get_version()
print("spinapi version {0}".format(sapi_version) )
print("Found some boards n=(%d)" % sapi.pb_count_boards())
#assert float(sapi_version)>=20171214


if not sapi.pb_count_boards():
    raise IOError("No board found")
    exit() #if you manage to get there somehow

# assume only one baord is found
myboard_id=sapi.pb_select_board(0) #starts from 0!

if sapi.pb_init():
    raise IOError("Error whileinitialzing board {0}".format(myboard_id))
print("Right after init status",sapi.pb_read_status())
sapi.pb_set_defaults()

# set clock
# NOTE: this should be the same as in b12proc.c
# it is accoridng to line 191
sapi.pb_core_clock(75.0)


ct=1 #scan counter inital dummy

# reimplement software from dan
if sapi.pb_start_programming(myboard_id) != 0:
    raise IOError("start_programming failed!")
sapi.pb_scan_count(1)
sapi.pb_set_num_points(1)
sapi.pb_set_scan_segments(NTUNE)# hardcoded for now
sapi.pb_stop_programming()


sapi.pb_start_programming(sapi.FREQ_REGS)
sapi.pb_set_freq(TUNESTART)
sapi.pb_stop_programming()



# powers command
# POWERS 2 1000 10 -1 -1
sapi.pb_set_amp(2/1000,0)
sapi.pb_set_amp(1000/1000,1)
sapi.pb_set_amp(2/1000,2)
sapi.pb_set_amp(2/1000,3)

"""
sapi.pb_start_programming(sapi.PULSE_PROGRAM)
# now programm a simple pulse
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
  sapi.TX_ENABLE, sapi.NO_PHASE_RESET,
  sapi.NO_TRIGGER, sapi.NO_SHAPE, sapi.AMP0,
  0,
  sapi.Instruction.CONTINUE, sapi.NO_DATA, DELAY )

sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
  sapi.TX_ENABLE, sapi.NO_PHASE_RESET,
  sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
  0,
  sapi.Instruction.CONTINUE, sapi.NO_DATA, DELAY )

sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
  sapi.TX_DISABLE, sapi.NO_PHASE_RESET,
  sapi.NO_TRIGGER, sapi.NO_SHAPE, sapi.AMP0,
  0,
  sapi.Instruction.STOP, sapi.NO_DATA, DELAY )

sapi.pb_stop_programming()
sapi.pb_reset()
pb_status=sapi.pb_read_status()
print("after reset",pb_status)
sapi.pb_start()
pb_status=sapi.pb_read_status()
print("after start",pb_status)
sapi.pb_stop()
pb_status=sapi.pb_read_status()
print("after stop",pb_status)
exit(0)
"""

# now start programming the sweep pulse
start=sapi.pb_start_programming(sapi.PULSE_PROGRAM)

# do nothing 2us
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
                  sapi.TX_DISABLE, sapi.PHASE_RESET,
                  sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
		          AMP_UNBLANK ,
                  sapi.Instruction.CONTINUE, sapi.NO_DATA, 2000 )
# do send out a pulse for 10 usec
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
        sapi.TX_ENABLE, sapi.NO_PHASE_RESET,
        sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
         RECV_UNBLANK + AMP_UNBLANK,
        sapi.Instruction.CONTINUE, sapi.NO_DATA, 50000 )
# do nothing (500ns) and wait command  
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
        sapi.TX_DISABLE, sapi.NO_PHASE_RESET,
        sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
        AMP_UNBLANK ,
        sapi.Instruction.WAIT, sapi.NO_DATA, 500 ) #war mal WAIT CONTINUE
# do nothing 2us
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
                  sapi.TX_DISABLE, sapi.PHASE_RESET,
                  sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
		          AMP_UNBLANK ,
                  sapi.Instruction.CONTINUE, sapi.NO_DATA, 2000 )
# do send out a pulse for 10 usec
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
        sapi.TX_ENABLE, sapi.NO_PHASE_RESET,
        sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
         RECV_UNBLANK + AMP_UNBLANK,
        sapi.Instruction.CONTINUE, sapi.NO_DATA, 50000 )
# do nothing (500ns) and wait command  
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
        sapi.TX_DISABLE, sapi.NO_PHASE_RESET,
        sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
        AMP_UNBLANK ,
        sapi.Instruction.CONTINUE, sapi.NO_DATA, 500 ) #war mal wait

# do nothing 2us
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
                  sapi.TX_DISABLE, sapi.PHASE_RESET,
                  sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
		          AMP_UNBLANK ,
                  sapi.Instruction.WAIT, sapi.NO_DATA, 2000 )
# do send out a pulse for 10 usec
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
        sapi.TX_ENABLE, sapi.NO_PHASE_RESET,
        sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
         RECV_UNBLANK + AMP_UNBLANK,
        sapi.Instruction.CONTINUE, sapi.NO_DATA, 50000 )
# do nothing (500ns) and wait command  
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
        sapi.TX_DISABLE, sapi.NO_PHASE_RESET,
        sapi.NO_TRIGGER, exps.useShape, exps.useAmp,
        AMP_UNBLANK ,
        sapi.Instruction.CONTINUE, sapi.NO_DATA, 1000 )

# STOP
sapi.pb_inst_radio_shape (0, sapi.PHASE090, sapi.PHASE000, 0,
        sapi.TX_DISABLE, sapi.NO_PHASE_RESET,
        sapi.NO_TRIGGER, sapi.NO_SHAPE, sapi.AMP0,
        0,
        sapi.Instruction.STOP, sapi.NO_DATA, 500 )

sapi.pb_stop_programming()

pb_status=sapi.pb_read_status()
print("after init",pb_status)

sapi.pb_reset()
time.sleep(10e-6)

pb_status=sapi.pb_read_status()
print("after reset",pb_status)


inc=0
cmax=64
start=time.time()
sapi.pb_start() #first start
#time.sleep(10e-5)
#pb_status=sapi.pb_read_status()
#print('Status after first sleep:',pb_status) #scanning?
#exit(0)
#sapi.pb_stop() #first start
#sapi.pb_start() #first start
#time.sleep(5e-5)
#pb_stop_return=sapi.pb_stop()
#sapi.pb_start() #first start

delta_time=[]
pb_start_stime=[]

while (inc < 2 ):
    before_read=time.time()
    pb_status=sapi.pb_read_status()
    after_read=time.time()
    delta_time.append(after_read-before_read)
    wait_status=( int(pb_status,2) & int(bin(8),2) )
    c=0
    while (not (wait_status and sapi.STATUS_WAITING)):
        print('in while waiting loop')
        pb_status=sapi.pb_read_status()
        wait_status=( int(pb_status,2) & int(bin(8),2) )
        time.sleep(1e-4)
        c+=1
        if c>cmax:
            break
    freq=TUNESTART + DTUNE*(inc+1)
    sapi.pb_start_programming(sapi.FREQ_REGS)
    sapi.pb_set_freq(freq)
    sapi.pb_stop_programming()
    #print('Call ob_Start()')
    before_start=time.time()
    sapi.pb_start()
    after_start=time.time()
    pb_start_stime.append(after_start-before_start)
    inc+=1

#sapi.pb_start()
#sapi.pb_start()

time.sleep(10e-6)
pb_stop_return=sapi.pb_stop()
if pb_stop_return != 0:
    print('Stop did not work!')
pb_status=sapi.pb_read_status()

print('Finishing with status:',pb_status)

print(delta_time,pb_start_stime)
"""
# now get data once
intmax=50
while (sapi.pb_read_status() != sapi.STATUS_STOPPED):
    time.sleep(0.1) #to make sure that the experiment is finished
    print('sleeping')
    intmax-=1
    if intmax<0:
        break
ret,data_real,data_imag=sapi.pb_get_data(globVars.complex_points)
data_abs=[]
import math
for k in range(len(data_real)):
    data_abs.append(math.sqrt(data_real[k]**2+data_imag[k]**2))


import matplotlib.pyplot as plt
plt.plot(freq_var,data_abs)
plt.show()
"""
