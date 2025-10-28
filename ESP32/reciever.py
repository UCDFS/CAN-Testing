# Import socket module 
import socket             
import time
# Create a socket object 
s = socket.socket()         

# Define the port on which you want to connect 
host="192.168.4.1"
port = 80                

s.connect((host,port))

request= b"GET / HTTP/1.1\r\nHost:" + host.encode() + b"\r\n\r\n"

s.sendall(request)
time.sleep(0.5)
while True:
    time.sleep(0.2)
    data=s.recv(4096)
    print(data.decode())
print("bruh")