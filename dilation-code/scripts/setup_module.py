#this script will set the EXP_CPUS option, then compile the TLKM
import sys
import os

nCpus = sys.argv[1]
scriptDir = os.path.dirname(os.path.realpath(__file__))
print "Compiling TimeKeeper kernel modules ..."
f = open(scriptDir + "/../src/core/dilation_module.h", 'r')
tmp = open("tmp.c", 'w');
for line in f:
	if "#define EXP_CPUS" in line:
		print "You have " + nCpus + " VCPUs"
		input = raw_input("How many vCPUs do you want to allocate to an experiment? (leave at least 2 vCPUs for background tasks)\n")
		tmp.write("#define EXP_CPUS " + str(input) + "\n")
	else:
		tmp.write(line)

f.close()
tmp.close()
os.system("mv tmp.c " + scriptDir + "/../src/core/dilation_module.h")
os.system("sudo chmod 777 " + scriptDir + "/../src/core/dilation_module.h")
