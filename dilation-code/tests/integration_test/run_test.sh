#!/bin/bash

sudo run_test 10 3 1000 1000000 > /tmp/integration_test.log
if grep -nr "Succeeded" /tmp/integration_test.log; then
	echo "STATUS: COMPLETED. Check Logs at /tmp/integration_test.log"
else
	echo "STATUS: FAILED. Check Logs at /tmp/integration_test.log"
fi
