import socket
import struct
import time

s = socket.create_connection(("localhost", 9000))

payload = b""
length_prefix = struct.pack(">I", len(payload))

s.sendall(length_prefix)
time.sleep(0.5)
s.sendall(payload)

response = s.recv(1024)
print(response)