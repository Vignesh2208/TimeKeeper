#!/bin/bash
brctl addbr br-1
tunctl -t tap-1
ifconfig tap-1 0.0.0.0 promisc up
brctl addif br-1 tap-1
ifconfig br-1 up
lxc-create -n lxc-1 -f tmp/lxc-1.conf

brctl addbr br-2
tunctl -t tap-2
ifconfig tap-2 0.0.0.0 promisc up
brctl addif br-2 tap-2
ifconfig br-2 up
lxc-create -n lxc-2 -f tmp/lxc-2.conf

