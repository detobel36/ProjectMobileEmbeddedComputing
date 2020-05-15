#!/usr/bin/python

import serial
import argparse
import traceback

from sensor import Sensor


class Server:

    def __init__(self, serialNumber):
        self.listSensor = dict()

        self.serial = serial.Serial()
        self.serial.baudrate = 115200
        self.serial.port = '/dev/pts/' + str(serialNumber)

    def start(self):
        print("Start server")
        self.serial.open()

    def listen(self):
        line = self._readSerial()
        while(line):
            print("[DEBUG] " + line)
            if(line.startswith('[DATA]')):
                print("Get data: " + line)
                self.serial.write('Test\n'.encode('ascii'))
                print("Send data")

            line = self._readSerial()

    def stop(self):
        print("Stopping server")
        self.serial.close()

    def _readSerial(self):
        return str(self.serial.readline().decode('ascii')).strip()

    def _writeSerial(self, message):
        self.serial.write((message.strip() + '\n').encode('ascii'))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('serial', type=int,
                    help='Serial device of border node (/dev/pts/<serial>)')
    args = parser.parse_args()

    server = Server(args.serial)
    server.start()

    try:
        server.listen()
    except KeyboardInterrupt:
        server.stop()
    except:
        print("Unexpected error: " + str(traceback.format_exc()))
