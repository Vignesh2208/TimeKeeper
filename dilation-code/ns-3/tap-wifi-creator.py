import sys
import os


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print "Usage: python tap-wifi-creator #LXCs SimTDF"
        sys.exit()
    PATH_TO_NS3_TAP_SRC = "/home/jlamps/ns-allinone-3.21/ns-3.21/src/tap-bridge/examples" 
    numLXCs = sys.argv[1]
    tdf = int(float(sys.argv[2])*1000)
    template = open("template-wifi.cc", 'r')
    output = open(PATH_TO_NS3_TAP_SRC + "/tap-wifi-virtual-machine.cc", 'w')
    nodesVar = "x"
    netDeviceVar = "x"
    tapBridgeVar = "x"
    positionAllocatorVar = "x"
    for line in template:
        line = line.strip()
        if "NodeContainer" in line:
            lineSplit = line.split(' ')
            nodesVar = (lineSplit[1].strip())[:-1]
#            nodesVar = nodesVar.replace(' ', '')
        if "NetDeviceContainer" in line:
            lineSplit = line.split(' ')
            netDeviceVar = lineSplit[1].strip()
        if "TapBridgeHelper" in line:
            lineSplit = line.split(' ')
            tapBridgeVar = (lineSplit[1].strip())[:-1]
        if "Ptr<ListPositionAllocator>" in line:
            lineSplit = line.split(' ')
            positionAllocatorVar = lineSplit[1].strip()
        if "@@@" in line:
            line = line.replace('@@@', numLXCs)
        if "###" in line:
            line = line.replace('###', str(tdf))
        if "$$$" in line:
            for i in range(0,int(numLXCs)):
                output.write(tapBridgeVar + ".SetAttribute (\"DeviceName\", StringValue (\"tap-" + str(i+1) + "\"));\n")
                output.write(tapBridgeVar + ".Install (" + nodesVar + ".Get (" + str(i) + "), " + netDeviceVar + ".Get (" + str(i) + "));\n")
            continue
        if "%%%" in line:
            for i in range(0,int(numLXCs)):
                output.write(positionAllocatorVar + "->Add (Vector (" + str(i*10.0) + ", 0.0, 0.0));\n")
            continue
        output.write(line + "\n")
    template.close()
    output.close()    
    #print nodesVar, "|", netDeviceVar,"|", tapBridgeVar,"|",positionAllocatorVar,"|"
