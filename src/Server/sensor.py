#!/usr/bin/python

import numpy as np

DEBUG = True
#NUMBER_VALUE_TO_EVAL = 30
NUMBER_VALUE_TO_EVAL = 5  # For test
# THRESHOLD = -5
THRESHOLD = 1000000


"""
TODO:
- Remove old unused values
"""


class Sensor:

    def __init__(self, address):
        self.address = address
        self.listValues = list()

    def addValue(self, value):
        self.listValues.append(int(value))
        if(DEBUG):
            print("[DEBUG] (" + str(self.address) + ") Add value " + str(value) + " " + \
                "(total " + str(len(self.listValues)) + ")")

    def _computeLeastSquareRoot(self):
        if(len(self.listValues) < NUMBER_VALUE_TO_EVAL):
            print("[DEBUG] (" + str(self.address) + ") No enought value for valve " + \
                "(total " + str(len(self.listValues)) + ")")
            return None

        x = np.array([i for i in range(NUMBER_VALUE_TO_EVAL)])
        y = np.array(self.listValues[len(self.listValues)-NUMBER_VALUE_TO_EVAL:])
        A = np.vstack([x, np.ones(len(x))]).T
        m, c = np.linalg.lstsq(A, y, rcond=None)[0]

        return m < THRESHOLD


    # TODO update this name
    def getOpenValve(self):
        openValve = self._computeLeastSquareRoot()
        if(DEBUG):
            print("[DEBUG] (" + str(self.address) + ") Need open valve: " + str(openValve))

        return openValve
