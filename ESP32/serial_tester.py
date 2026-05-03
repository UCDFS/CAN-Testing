import serial
import time

s= serial.Serial(port='COM3',baudrate=115200)
data="---START---"
count=0
while True:
    count+=1
    print("data")
    s.write(b"data:" +str(count).encode()+b"\r\n")
    time.sleep(0.1)
