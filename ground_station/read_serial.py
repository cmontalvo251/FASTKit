#!/usr/bin/python3

import sys
import time
import numpy as np
sys.path.append('../libraries/')
from UART.uart import UART as U
ser = U(57600,"/dev/ttyUSB0",period=1.0) #Set the baudrate, port and period in seconds
##Make a number array for telemetry
fastkit_packet = np.zeros(6)

while True:
    time.sleep(0.1)
    print('Checking for new data',time.monotonic())
    value,position = ser.SerialGetNumber(0)
    print('Value Received, Position = ',value,position)
