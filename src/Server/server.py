#!/usr/bin/python

import serial
import argparse
import traceback
import re
from log import Log
from constants import DEBUG

from sensor import Sensor


class Server:

    def __init__(self, serialNumber):
        self.listSensor = dict()

        self.log = Log()

        if(DEBUG):
            self.log.info("Run in debug mode")

        self.serial = serial.Serial()
        self.serial.baudrate = 115200
        self.serial.port = '/dev/pts/' + str(serialNumber)

    def start(self):
        self.log.info("Start server")
        self.serial.open()

    def listen(self):
        line = self._readSerial()
        while(line):
            matchRegex = re.match("\[DATA\] ([0-9\.]*) - ([0-9]{1,2})", line)
            if(matchRegex and len(matchRegex.groups()) == 2):
                address, value = matchRegex.groups()
                self.log.info("Get value " + value + " from " + address)

                if(address in self.listSensor):
                    self.listSensor[address].addValue(value)
                else:
                    self.listSensor[address] = Sensor(address)

                openValve = self.listSensor[address].getOpenValve()
                if(openValve != None and openValve):
                    self._writeSerial(address)

            line = self._readSerial()

    def stop(self):
        self.log.info("Stopping server")
        self.serial.close()

    def _readSerial(self):
        line = str(self.serial.readline().decode('ascii')).strip()
        # if(DEBUG):
        #     print("[DEBUG] Read line: " + line)
        return line

    def _writeSerial(self, message):
        self.log.debug("Send data: " + str(message))
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
