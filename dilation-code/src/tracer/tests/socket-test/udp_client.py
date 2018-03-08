import socket

UDP_IP_ADDRESS = "127.0.0.1"
UDP_PORT_NO = 6789
Message = "Hello, Server"

clientSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print "Client Sending message ..."
clientSock.sendto(Message, (UDP_IP_ADDRESS, UDP_PORT_NO))

print "Client Sent message ..."