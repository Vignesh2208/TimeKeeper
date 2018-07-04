

import select
import socket
import sys
import Queue
import time


print 'Client program starting ans sleeping for 0.5 sec'
print time.time()
sys.stdout.flush()

# Do not block forever (milliseconds)
TIMEOUT = 1000

server_address = ('localhost',10000)

# Create a TCP/IP socket
sock =  socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
	time.sleep(0.5)
except IOError as err:
	print("Unexpected error:", sys.exc_info()[0]) 


print 'Client program woke up from sleep!'
print time.time()
sys.stdout.flush()


# Connect the socket to the port where the server is listening
print 'connecting to %s port %s' % server_address
sys.stdout.flush()
sock.connect(server_address)




# Read from File and send

with open('/home/vignesh/TimeKeeper/dilation-code/tests/socket_test/sock_testdata') as f:
	array = []
	for line in f:
		array.append(line)

amount_expected = len(''.join(array))


try :
	for message in array:
		sock.sendall(message)
		# time.sleep(1)

	print '\n**** Client File Over **** \n'
	sys.stdout.flush()

	amount_received = 0
	while amount_received < amount_expected:
		data = sock.recv(128)
		amount_received += len(data)
		print '%s' % data,
		sys.stdout.flush()

	#while 1 :
	#	pass
finally:
	print >>sys.stderr, 'Closing Socket'
	#sock.close()	
	sock.close()
while 1 :
	pass
'''
# Commonly used flag setes
READ_ONLY = select.POLLIN | select.POLLPRI | select.POLLHUP | select.POLLERR
READ_WRITE = READ_ONLY | select.POLLOUT

# Set up the poller
poller = select.poll()
poller.register(sock, READ_WRITE)

# Map file descriptors to socket objects
fd_to_socket = { sock.fileno(): sock,
               }


print >>sys.stderr, '\n**** Client **** \n'

while True:

	# Wait for at least one of the sockets to be ready for processing
	print >>sys.stderr, '&',
	events = poller.poll(TIMEOUT)

	for fd, flag in events:

        # Retrieve the actual socket from its file descriptor
		s = fd_to_socket[fd]

        # Handle inputs
		if flag & (select.POLLIN | select.POLLPRI):
			data = s.recv(1024)

			if data:
				# A readable client socket has data
				print >>sys.stderr, '"%s"' % data
			else:
			# Interpret empty result as closed connection
				print >>sys.stderr, 'closing', client_address, 'after reading no data'
			# Stop listening for input on the connection
				poller.unregister(s)
				s.close()
		elif flag & select.POLLHUP:
			# Client hung up
			print >>sys.stderr, 'closing', client_address, 'after receiving HUP'
			# Stop listening for input on the connection
			poller.unregister(s)
			s.close()
		elif flag & select.POLLOUT:
			# Socket is ready to send data, if there is any to send.
			try:
				next_msg = message_queues[s].get_nowait()
			except Queue.Empty:
				# No messages waiting so stop checking for writability.
				print >>sys.stderr, 'output queue for', s.getpeername(), 'is empty'
				poller.modify(s, READ_ONLY)
			else:
				print >>sys.stderr, '~ ' 
				s.send(next_msg)

		elif flag & select.POLLERR:
			print >>sys.stderr, 'handling exceptional condition for', s.getpeername()
			# Stop listening for input on the connection
			poller.unregister(s)
			s.close()
'''
