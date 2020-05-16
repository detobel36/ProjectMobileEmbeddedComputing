#!/usr/bin/python

import serial
import argparse
import traceback
import re

from sensor import Sensor

DEBUG = True


class Server:

    def __init__(self, serialNumber):
        self.listSensor = dict()

        self.serial = serial.Serial()
        self.serial.baudrate = 115200
        self.serial.port = '/dev/pts/' + str(serialNumber)

    def start(self):
        print("[INFO] Start server")
        self.serial.open()

    def listen(self):
        line = self._readSerial()
        while(line):
            matchRegex = re.match("\[DATA\] ([0-9\.]*) - ([0-9]{1,2})", line)
            if(matchRegex and len(matchRegex.groups()) == 2):
                address, value = matchRegex.groups()
                print("[INFO] Get value " + value + " from " + address)

                if(address in self.listSensor):
                    self.listSensor[address].addValue(value)
                else:
                    self.listSensor[address] = Sensor(address)

                openValve = self.listSensor[address].getOpenValve()
                if(openValve != None and openValve):
                    self._writeSerial(address)

            line = self._readSerial()

    def stop(self):
        print("[INFO] Stopping server")
        self.serial.close()

    def _readSerial(self):
        line = str(self.serial.readline().decode('ascii')).strip()
        if(DEBUG):
            print("[DEBUG] Read line: " + line)
        return line

    def _writeSerial(self, message):
        if(DEBUG):
            print("[DEBUG] Send data: " + str(message))
        self.serial.write((str(message).strip() + '\n').encode('ascii'))


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
    # except:
    #     print("[SEVERE] Unexpected error: " + str(traceback.format_exc()))
