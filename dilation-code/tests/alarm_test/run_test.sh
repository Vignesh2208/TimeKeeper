#!/bin/bash

sudo run_test 1 1 10000 1000000 > /tmp/alarm_test.log
if grep -nr "Succeeded" /tmp/alarm_test.log; then
	echo "STATUS: COMPLETED. Check Logs at /tmp/alarm_test.log"
else
	echo "STATUS: FAILED. Check Logs at /tmp/alarm_test.log"
fi
