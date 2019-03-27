#!/bin/bash
lxc-stop -n lxc-1
lxc-destroy -n lxc-1
ifconfig br-1 down
brctl delif br-1 tap-1
brctl delbr br-1
ifconfig tap-1 down
tunctl -d tap-1

lxc-stop -n lxc-2
lxc-destroy -n lxc-2
ifconfig br-2 down
brctl delif br-2 tap-2
brctl delbr br-2
ifconfig tap-2 down
tunctl -d tap-2

