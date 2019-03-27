#this script will set the EXP_CPUS option, then compile the TLKM
import sys
import os

f = open("dilation_module.h", 'r')
tmp = open("tmp.c", 'w');
for line in f:
	if "#define EXP_CPUS" in line:
		input = raw_input("How many vCPUs do you want to allocate to an experiment? (leave at least 2 vCPUs for background tasks) (ie if you have 8 vCPUs, do not allocate more than 6\n")
		tmp.write("#define EXP_CPUS " + str(input) + "\n")
	else:
		tmp.write(line)

f.close()
tmp.close()
os.system("mv tmp.c dilation_module.h")

os.system("make")
