#!/bin/bash

find . -name \* -type f ! -path "*.sh" ! -path "*.txt" -print | cut -d'/' -f2- > differences.txt
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cat differences.txt | while read LINE
do

SRC_FILE=$SCRIPT_DIR/$LINE
DST_DIR=/src/linux-4.4.5/$(dirname $LINE)
echo Copying $SRC_FILE to $DST_DIR
mkdir -p $DST_DIR
cp -v $SRC_FILE $DST_DIR
done