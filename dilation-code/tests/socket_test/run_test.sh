#!/bin/bash

sudo ./socket_test > /tmp/socket_test.log
if grep -nr "Succeeded" /tmp/socket_test.log; then
	echo "STATUS: COMPLETED. Check Logs at /tmp/socket_test.log"
else
	echo "STATUS: FAILED. Check Logs at /tmp/socket_test.log"
fi