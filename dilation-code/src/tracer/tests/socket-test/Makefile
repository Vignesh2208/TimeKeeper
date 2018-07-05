current_dir=$(shell pwd)


build:
	@echo "python udp_server.py\npython udp_client.py" > cmds.txt

run:
	@echo "Starting Socket test ..."
	tracer_test -f cmds.txt
	@echo "Socket Test Finished ..."

start: run
