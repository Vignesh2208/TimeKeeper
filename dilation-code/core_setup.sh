#!/bin/sh

echo "Starting CORE Setup"

sudo apt-get install bash bridge-utils ebtables iproute libev-dev python tcl8.5 tk8.5 libtk-img autoconf automake gcc libev-dev make python-dev libreadline-dev pkg-config imagemagick help2man
cd core
./bootstrap.sh
./configure
make
sudo make install
chmod +x core-daemon
chmod +x /etc/init.d/core-daemon
cp core-daemon /usr/local/sbin/
cd ..

echo "CORE Setup Complete"
