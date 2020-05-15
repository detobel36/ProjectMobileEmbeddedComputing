#!/usr/bin/python

import serial

def readSerial(serial):
    return str(serial.readline().decode('ascii')).strip()

if __name__=="__main__":
    print("Start")
    ser = serial.Serial()
    ser.baudrate = 115200
    ser.port = '/dev/pts/5'
    ser.open()

    line = readSerial(ser)
    while(line != ""):
        print("[DEBUG] Read line: " + line)
        if(line.startswith('[DATA]')):
            print("Get data: " + line)
            ser.write('Test\n'.encode('ascii'))
            print("Send data")

        line = readSerial(ser)

    ser.close()
