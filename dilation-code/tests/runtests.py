
from timekeeper_functions import *

import argparse
import sys
import subprocess
import os
import time
import signal



TDF = 2
ENABLE_TK = 1
TIMESLICE = 500000 

def signal_handler(signal, frame):
	print 'You pressed Ctrl+C! Stopping Experiment'
	if ENABLE_TK == 1 :
		stopExp()
		time.sleep(2)
		print "Killing all processes"
		os.system("sudo killall python")	
	sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
def main():
 	parser = argparse.ArgumentParser()
	parser.add_argument("--script_name", dest="script_name", help=" Filename to run", required=True)
	parser.add_argument("--no", dest="no", help="Number of scripts to launch ")
	parser.add_argument("--tdfactor", dest="tdfactor", help="Dialation Factor ")
	args = parser.parse_args()
	script_name = str(args.script_name)
	if args.no :
		n_processes = int(args.no)
	else:
		n_processes = 1

	if args.tdfactor :
		my_tdf = int(args.tdfactor)
	else :
		my_tdf = 2

	print "TD factor", my_tdf 
	
	
	for i in xrange(0, n_processes):
		curr_cmd = "python " + script_name + " --log_file=log" +  str(i) + ".txt" + " --c=" +str(i)
		print "Current Command is ",curr_cmd
		proc = subprocess.Popen(curr_cmd, shell=True)

		print "New process ", i, " pid = ", proc.pid
		if ENABLE_TK == 1:	
			dilate_all(int(proc.pid), my_tdf)
			addToExp(int(proc.pid))

	if ENABLE_TK == 1:
		curr_cmd = "python spinner.py"
		proc = subprocess.Popen(curr_cmd,shell = True)
		print "Spinner pid = ", proc.pid
		dilate_all(int(proc.pid), my_tdf)
		addToExp(int(proc.pid))
		set_cbe_experiment_timeslice(TIMESLICE*my_tdf)
		print "Sychronize and Freeze"
		synchronizeAndFreeze()
		time.sleep(2)
		print "Starting Experiment"
		startExp()
		time.sleep(900)
		print "Stopping Experiment"
		stopExp()
		time.sleep(2)
	else:
		time.sleep(50)	

	print "Killing all processes"
	os.system("sudo killall python")

main()
