

import select
import socket
import sys
import Queue
import time
import argparse
import os
import datetime 

from datetime import datetime

parser = argparse.ArgumentParser()
parser.add_argument("--log_file", dest="log_file", help="log_file")
parser.add_argument("--c", dest="c", help="log_file")
args = parser.parse_args()

if args.log_file:
	log_file = str(args.log_file)
else:
	log_file = None

if log_file != None :
	with open(log_file,"w") as f :
		pass

if args.c:
	cnt = int(args.c)
else:
	cnt = 1

# Create a TCP/IP Socket

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setblocking(0)

# Bind the socket to the port
server_address = ('localhost', 10000)
print >>sys.stderr, 'starting up on %s port %s' % server_address
server.bind(server_address)

# Listen for incoming connections
server.listen(5)


# Sockets from which we expect to read
#inputs = [ server ]

# Sockets to which we expect to write
#outputs = [ ]

# Outgoing message queues (socket:Queue)

message_queues = {}


# Do not block forever (milliseconds)
TIMEOUT = 1000

# Commonly used flag setes
READ_ONLY = select.POLLIN | select.POLLPRI | select.POLLHUP | select.POLLERR
READ_WRITE = READ_ONLY | select.POLLOUT

# Set up the poller
poller = select.poll()
poller.register(server, READ_ONLY)

# Map file descriptors to socket objects
fd_to_socket = { server.fileno(): server,
               }


print >>sys.stderr, '\n**** Server **** \n'

while True:

	# Wait for at least one of the sockets to be ready for processing
	#print >>sys.stderr, '!',
	# time.sleep(1)
	events = poller.poll(TIMEOUT)

	# It is coming out because of timeout. In that case log the time
	os.system("./gtod > /tmp/tm")
	if log_file == None :
		print datetime.now()
	else:
		text = ""
		with open('/tmp/tm', 'r') as myfile :
			text = myfile.read().replace('\n','')
			myfile.close()
		with open(log_file,"a") as f :
			f.write(text +"<--Real *** Dialated -->")
			f.write(str(datetime.now()) + "\n")
	

	for fd, flag in events:

	# Retrieve the actual socket from its file descriptor
		s = fd_to_socket[fd]

	# Handle inputs
		if flag & (select.POLLIN | select.POLLPRI):

			if s is server:
		# A "readable" server socket is ready to accept a connection
				connection, client_address = s.accept()
				print >>sys.stderr, 'new connection from', client_address
				connection.setblocking(0)
				fd_to_socket[ connection.fileno() ] = connection
				poller.register(connection, READ_ONLY)

				# Give the connection a queue for data we want to send
				message_queues[connection] = Queue.Queue()
			else:
				data = s.recv(1024)

				if data:
					# A readable client socket has data
					st = data.upper()
					# print >>sys.stderr, '%s' % st
					message_queues[s].put(st)
					# Add output channel for response
					poller.modify(s, READ_WRITE)
				else:
				# Interpret empty result as closed connection
					print >>sys.stderr, 'closing', client_address, 'after reading no data'
				# Stop listening for input on the connection
					poller.unregister(s)
					s.close()
				# Remove message queue
					del message_queues[s]
					break
		elif flag & select.POLLHUP:
			# Client hung up
			print >>sys.stderr, 'closing', client_address, 'after receiving HUP'
			# Stop listening for input on the connection
			poller.unregister(s)
			s.close()
			break
		if flag & select.POLLOUT:
			# Socket is ready to send data, if there is any to send.
			try:
				next_msg = message_queues[s].get_nowait()
			except Queue.Empty:
				# No messages waiting so stop checking for writability.
				print >>sys.stderr, 'output queue for', s.getpeername(), 'is empty'
				poller.modify(s, READ_WRITE)
			else:
				print >>sys.stderr, '~ ' 
				s.send(next_msg)

		elif flag & select.POLLERR:
			print >>sys.stderr, 'handling exceptional condition for', s.getpeername()
			# Stop listening for input on the connection
			poller.unregister(s)
			s.close()

			# Remove message queue
			del message_queues[s]



