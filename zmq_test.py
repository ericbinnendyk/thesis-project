#
#   Hello World server in Python
#   Binds REP socket to tcp://*:5555
#   Expects b"Hello" from client, replies with b"World"
#

import time
import zmq

context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind("tcp://*:5555")

def receive_string_and_send_reply():
    #  Wait for next request from client
    message = socket.recv() # message is a bit string, not a normal string
    #print("Received request: {}".format(''.join(message.split(b'\0')[0])))

    #  Send pointless reply back to client
    socket.send(b"Received")
    return message.split(b'\0')[0].decode('ascii')

while True:
    print(receive_string_and_send_reply())

while True:
    #  Wait for next request from client
    message = socket.recv() # message is a bit string, not a normal string
    print(message.split(b'\0')[0].decode('ascii'))
    #print("Received request: {}".format(''.join(message.split(b'\0')[0])))

    #  Send pointless reply back to client
    socket.send(b"Received")