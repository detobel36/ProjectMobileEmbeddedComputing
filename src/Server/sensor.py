#!/usr/bin/python
# -*- coding: utf-8 -*-

from time import time
from log import Log
from constants import DEBUG
from leastSquareRoot import leastSquareRoot


NUMBER_VALUE_TO_EVAL = 30 # 5  # For test
THRESHOLD = 0

# Time before delete a sensor
MAX_DATA_DIFFERENCE = 190 # If we don't have any news since more than 3 minute (thus we lost 3 values)



class Sensor:

    def __init__(self, address):
        self.address = address
        self.listValues = list()
        self.lastUpdate = int(time())

        self.log = Log()

    """
    Add a value but removed all old values if the time between the last value and the current value
    is too long.

    @param value: the value (between 0 and 100) that must be add
    @return: None
    """
    def addValue(self, value):
        if(value < 0 or 100 > value):
            self.log.severe("(" + str(self.address) + " Receive invalid value: " + str(value))
            return

        if((self.lastUpdate - int(time())) > MAX_DATA_DIFFERENCE):
            # Reset list value
            self.listValues = list()
            self.log.info("(" + str(self.address) + ") Reset all values because saved values " + \
                "are too old")

        if(self.listValues == NUMBER_VALUE_TO_EVAL):
            self.listValues.pop(0)

        self.listValues.append(int(value))
        self.log.debug("(" + str(self.address) + ") Add value " + str(value) + " " + \
            "(total " + str(len(self.listValues)) + ")")
        self.lastUpdate = int(time())

    def _computeLeastSquareRoot(self):
        if(len(self.listValues) < NUMBER_VALUE_TO_EVAL):
            self.log.debug("(" + str(self.address) + ") No enought value for valve " + \
                "(total " + str(len(self.listValues)) + ")")
            return None
        if(len(self.listValues) > NUMBER_VALUE_TO_EVAL):
            self.log.warn("(" + str(self.address) + ") Number of data is to high (" + \
                str(len(self.listValues)) + ")")

        self.log.debug("(" + str(self.address) + ") Compute least square root of value: " + \
            str(self.listValues))
        m = leastSquareRoot(self.listValues)

        self.log.info("(" + str(self.address) + ") Least Square Root value: " + str(round(m, 2)))

        return m > THRESHOLD


    """
    Compute a least square root

    return None if not enought data, False if valve doesn't need to be open and True if the valve
    need to be open
    """
    def checkIfValveMustBeOpen(self):
        openValve = self._computeLeastSquareRoot()
        self.log.debug("(" + str(self.address) + ") Need open valve: " + str(openValve))

        return openValve
