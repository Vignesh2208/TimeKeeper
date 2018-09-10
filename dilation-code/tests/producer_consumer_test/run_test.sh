#!/bin/bash

sudo run_test 1 1 10000 1000000 > /tmp/pc_test.log
if grep -nr "Succeeded" /tmp/pc_test.log; then
	echo "STATUS: COMPLETED. Check Logs at /tmp/pc_test.log"
else
	echo "STATUS: FAILED. Check Logs at /tmp/pc_test.log"
fi
