
import sys
import datetime
import os
import time
import argparse



tmr_array = [ [1,0], [0,1000], [5, 5000000]]

DEF_NSEC = 0
DEF_SEC = 1
BASE_MICROSEC = 1000
BASE_MILLISEC = 1000000
BASE_SEC = 1


parser = argparse.ArgumentParser()
parser.add_argument("--log_file", dest="log_file", help="log_file")
parser.add_argument("--sec", dest="sec", help="Seconds ")
parser.add_argument("--nsec", dest="nsec", help="Nano Seconds ")
args = parser.parse_args()

if args.log_file:
	log_file = str(args.log_file)
else:
	log_file = None

if log_file != None :
	with open(log_file,"w") as f :
		pass

time_sec = DEF_SEC
time_nsec =  DEF_NSEC

if args.nsec:
	time_nsec =  int(args.nsec)

if args.sec:
	time_sec = int(args.sec)

print >>sys.stderr, '\n**** Time Slice :',  time_sec
curr_cmd =  "./ttimerfd " + str(time_sec) + " "+ str(time_nsec)
print >>sys.stderr,   curr_cmd

#os.system(curr_cmd)

