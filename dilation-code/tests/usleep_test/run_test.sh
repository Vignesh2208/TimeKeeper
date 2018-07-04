#!/bin/bash

sudo ./usleep_test 1 > /tmp/usleep_test.log
if grep -nr "Succeeded" /tmp/usleep_test.log; then
	echo "STATUS: COMPLETED. Check Logs at /tmp/usleep_test.log"
else
	echo "STATUS: FAILED. Check Logs at /tmp/usleep_test.log"
fi