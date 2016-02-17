import zmq

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:5556")

socket.setsockopt(zmq.SUBSCRIBE, "MONROE")

while True:
    message = socket.recv()
    print message
