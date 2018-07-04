#!/bin/bash

sudo ./integration_test 10 3 > /tmp/integration_test.log
if grep -nr "Succeeded" /tmp/integration_test.log; then
	echo "STATUS: COMPLETED. Check Logs at /tmp/integration_test.log"
else
	echo "STATUS: FAILED. Check Logs at /tmp/integration_test.log"
fi