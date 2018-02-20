# Again we import the necessary socket python module
import socket
# Here we define the UDP IP address as well as the port number that we have 
# already defined in the client python script.
UDP_IP_ADDRESS = "127.0.0.1"
UDP_PORT_NO = 6789

# declare our serverSocket upon which
# we will be listening for UDP messages
serverSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# One difference is that we will have to bind our declared IP address
# and port number to our newly declared serverSock
serverSock.bind((UDP_IP_ADDRESS, UDP_PORT_NO))

print "Server: Waiting for new message"
while True:
    data, addr = serverSock.recvfrom(1024)
    print "Received Message: ", data
    break