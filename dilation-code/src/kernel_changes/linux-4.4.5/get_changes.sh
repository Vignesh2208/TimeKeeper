#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
#diff -rqa /src/linux-4.4.5 $HOME/linux-4.4.5 | grep differ | cut -d' ' -f2 > differences.txt

cat differences.txt | while read LINE
do

#DST_DIR=$SCRIPT_DIR-unmod/$(dirname $LINE | cut -d'/' -f4-)
#DST_DIR=$SCRIPT_DIR-unmod/$LINE
DST_DIR=$SCRIPT_DIR-mod$(dirname /$LINE )
echo Copying $LINE to $DST_DIR
mkdir -p $DST_DIR
cp -v /src/linux-4.4.5-insVT/$LINE $DST_DIR
done