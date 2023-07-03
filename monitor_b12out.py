import sys
import time
import pathlib


hardcoded_b12out = '/vnmr/tmp/b12out'

p=pathlib.Path(hardcoded_b12out)

i=0
while(True):
	with open(p,'r') as f:
		data=f.read()
	if i==1:
		raw_data=data
		print(data[:400])
		i+=1
	else:
		if i == 0:
			time.sleep(2)
			i+=1
			continue
		if data[:400] != raw_data[:400]:
			print(data)
			raise IOError('datafile changed!')
	time.sleep(1)
