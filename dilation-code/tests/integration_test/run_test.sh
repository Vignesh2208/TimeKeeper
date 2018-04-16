#!/bin/bash

for i in {1..10}
do
	sudo ./integration_test 10 3
	sleep 1
done