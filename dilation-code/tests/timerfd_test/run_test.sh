=#!/bin/bash

sudo ./tfd_test 1 1 > /tmp/tfd_test.log
if grep -nr "Succeeded" /tmp/tfd_test.log; then
	echo "STATUS: COMPLETED. Check Logs at /tmp/tfd_test.log"
else
	echo "STATUS: FAILED. Check Logs at /tmp/tfd_test.log"
fi