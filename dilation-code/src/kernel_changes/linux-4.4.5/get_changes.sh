#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cat differences.txt | while read LINE
do

DST_DIR=$SCRIPT_DIR$(dirname /$LINE )
echo Copying $LINE to $DST_DIR
mkdir -p $DST_DIR
cp -v /src/linux-4.4.5/$LINE $DST_DIR
done