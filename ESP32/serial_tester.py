import serial


s= serial.Serial(port='COM3',baudrate=115200)
data="---START---"
while True:
    print("data")
    s.write("data".encode())
    print("sent")
