
import sys
import datetime 
import os
import subprocess

from datetime import datetime

import time
import argparse


BASE_TIMESLICE = 1

parser = argparse.ArgumentParser()
parser.add_argument("--log_file", dest="log_file", help="log_file")
parser.add_argument("--c", dest="c", help="Process Count ")
args = parser.parse_args()

if args.log_file:
	log_file = str(args.log_file)
else:
	log_file = None

if log_file != None :
	with open(log_file,"w") as f :
		pass

if args.c:
	time_slice = BASE_TIMESLICE + float(args.c)
else:
	time_slice = BASE_TIMESLICE
print >>sys.stderr, '\n**** Time Slice :',  time_slice

while 1:
	time.sleep(time_slice)
	
	os.system("./bin/gtod > /tmp/tm")
	if log_file == None :
		print datetime.now()
	else:
		with open('/tmp/tm', 'r') as myfile :
			text = myfile.read().replace('\n','')
			myfile.close()
		with open(log_file,"a") as f :
			f.write(text +"<--Real *** Dialated -->")
			f.write(str(datetime.now()) + "\n")
	

