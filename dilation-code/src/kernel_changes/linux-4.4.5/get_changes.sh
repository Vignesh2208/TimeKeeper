#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
diff -rqa /src/linux-4.4.5 $HOME/linux-4.4.5 | grep differ | cut -d' ' -f2 > differences.txt

cat differences.txt | while read LINE
do

DST_DIR=$SCRIPT_DIR/$(dirname $LINE | cut -d'/' -f4-)
echo Copying $LINE to $DST_DIR
mkdir -p $DST_DIR

cp -vi $LINE $DST_DIR
done