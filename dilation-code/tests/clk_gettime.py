
import sys
import datetime
import os
import time
import argparse


tmr_array = [ [1000,10], [50000,1000], [150000,1000], [1000000,1000] ]


parser = argparse.ArgumentParser()
parser.add_argument("--log_file", dest="log_file", help="log_file")
parser.add_argument("--c", dest="c", help="Loop Count ")
args = parser.parse_args()

if args.log_file:
	log_file = str(args.log_file)
else:
	log_file = "/tmp/log"

if log_file != None :
	with open(log_file,"w") as f :
		pass

if args.c:
	loop_cnt = int(args.c)
else:
	loop_cnt =  0

k = len(tmr_array)

if( loop_cnt < k) :

	print >>sys.stderr, '\n**** Len : Loop Count :',  k, loop_cnt

	curr_cmd = "./bin/usleep " + str(tmr_array[loop_cnt][0]) + " " +  str(tmr_array[loop_cnt][1]) + " > "+log_file

	print >>sys.stderr, '\n cmd is :',  curr_cmd
	os.system(curr_cmd)
else:
	print >>sys.stderr, "Out of index"
