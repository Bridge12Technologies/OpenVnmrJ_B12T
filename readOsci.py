"""

Script to trigger data and read out alot of traces to create data for noise determination

"""

import numpy as np
import time
import serial
import struct

import argparse
import datetime
import pathlib

parser=argparse.ArgumentParser()

# parse comport
parser.add_argument('--comport',type=str,nargs=1,default=['COM3'],help="Enter comport (e.g. COM8), default=COM3")
# how often to be triggered
parser.add_argument('--n_trigger',type=int,nargs=1,default=[50],help="Number of triggered acquisitions, default=50")
# transmitt data chunk
parser.add_argument('--transmitt_chunk',type=int,nargs=1,default=[4096],help="transmitt data chunk in serial (bytes), default=4096")
# save file
fmt='%Y_%m_%d_%H%M'
current_time=datetime.datetime.now()#.fromtimestamp(time.time())
print('current data is {0}'.format(current_time.strftime(fmt)))
parser.add_argument('--save',type=str,nargs=1,default=['./osci_data_{0}'.format(current_time.strftime(fmt))])
# short or long acq
parser.add_argument('--shortAcq',type=bool,default=[False],nargs=1,help='if any argemnt is proved a short acquisition is taken')
#define triggerlevel
parser.add_argument('--triggerlevel',type=float,default=[34e-3],nargs=1,help='Triggerlevel in V, default=34e-3V (34mV)')

args=parser.parse_args()


# get parsed data
port = args.comport[0]
n_trigger=args.n_trigger[0]
chunksize=args.transmitt_chunk[0]
filename=args.save[0]


# how this works:
# set trigger & trigger n times (argument)
#

command_idn = '*idn?\n'.encode('utf-8')
command_trigger= 'trigger:level {0}\n'.format(args.triggerlevel[0]).encode('utf-8')
command_triggerstate='trigger:state?\n'.encode('utf-8')
command_longacq = ':acq1:lmem?\n'.encode('utf-8')
command_shortgacq = ':acq1:mem?\n'.encode('utf-8')
command_singleAcq= ':single\n'.encode('utf-8')

# talk with oscilloscope
serConn = serial.Serial(port, timeout = 1)
time.sleep(0.2)
command = command_idn
serConn.write(command)
time.sleep(0.2)

#get idn info and print it
string = ''
while serConn.in_waiting:
    string += serConn.read().decode('utf-8')
print(string)


# oscilloscope readout function:

def read_osci(serConn,chunksize):
    # get data size and identifier
    # example
    # # 4 8008 are first 6 elements
    # get double cross
    _=serConn.read(1)
    for k in range(1):
        raw_bytes = serConn.read(1)
    digit=int(raw_bytes.decode('utf-8'))
    buffer_bytes=b''
    for k in range(digit):
        raw_bytes = serConn.read(1)
        buffer_bytes+=raw_bytes
    n_elem=int(buffer_bytes.decode('utf-8'))
    print('Reading digit {0} and {1} elements'.format(digit,n_elem))

    # get time_interval
    buffer_bytes=b''
    for k in range(4):
        raw_bytes = serConn.read(1)
        buffer_bytes+=raw_bytes
    #delta_t=struct.unpack('<e',buffer_bytes)
    delta_t=struct.unpack('>f',buffer_bytes)[0]
    print('delta_t as float32 (big endian?):',delta_t)

    #get channel indicator
    raw_bytes=serConn.read(1)
    print('Channel:',raw_bytes.decode('utf-8'))

    # read 3 bytes of unused data
    raw_bytes=serConn.read(3)

    chunkread=chunksize
    chunks=n_elem//chunkread
    remainder=n_elem%chunkread
    raw=b''
    for k in range(chunks):
        raw+=serConn.read(chunkread)
    raw+=serConn.read(remainder)

    print('Finished reading {0} data bytes'.format(len(raw)))
    n_elem_read=int(len(raw)/2)

    #conver to nonbiary
    data_short_be=struct.unpack('>'+n_elem_read*'h',raw)
    #data_short_le=struct.unpack('<'+n_elem_read*'h',raw)

    # convert to array
    data_short = np.array(data_short_be)
    timepoints=np.arange(data_short.size)*delta_t
    data=np.vstack((timepoints,data_short)).T

    return data


# loop over trigger receiving channels
data_accumulated=[]
for trig in range(n_trigger):

    serConn.write(command_trigger)
    time.sleep(0.2)
    serConn.write(command_singleAcq)
    time.sleep(0.2)
    while(True):
        time.sleep(0.2)
        serConn.write(command_triggerstate)
        string = ''
        time.sleep(0.2)
        string += serConn.read(2).decode('utf-8')
        try:
            if int(string)==1:
                print(string)
                break
        except ValueError:
            print('passing_loop')
            pass
    if args.shortAcq[0]:
        serConn.write(command_shortgacq)
    else:
        serConn.write(command_longacq)
    time.sleep(0.2)
    data=read_osci(serConn,chunksize)
    data_accumulated.append(data)

data_accumulated=np.array(data_accumulated)

savepath=pathlib.Path(filename)
np.save(filename,data_accumulated)
print('Saved data to {0}'.format(filename))
