#!/usr/bin/python

from log import Log
from constants import DEBUG
from leastSquareRoot import leastSquareRoot


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

        self.log = Log()

    def addValue(self, value):
        self.listValues.append(int(value))
        self.log.debug("(" + str(self.address) + ") Add value " + str(value) + " " + \
            "(total " + str(len(self.listValues)) + ")")

    def _computeLeastSquareRoot(self):
        if(len(self.listValues) < NUMBER_VALUE_TO_EVAL):
            self.log.debug("(" + str(self.address) + ") No enought value for valve " + \
                "(total " + str(len(self.listValues)) + ")")
            return None

        dataForLeastSquareRoot = self.listValues[len(self.listValues)-NUMBER_VALUE_TO_EVAL:]
        self.log.debug("(" + str(self.address) + ") Compute least square root of value: " + \
            str(dataForLeastSquareRoot))
        m = leastSquareRoot(dataForLeastSquareRoot)

        # x = np.array([i for i in range(NUMBER_VALUE_TO_EVAL)])
        # y = np.array(self.listValues[len(self.listValues)-NUMBER_VALUE_TO_EVAL:])
        # A = np.vstack([x, np.ones(len(x))]).T
        # m, c = np.linalg.lstsq(A, y, rcond=None)[0]
        self.log.info("(" + str(self.address) + ") Leas Square Root value: " + str(round(m, 2)))

        return m < THRESHOLD


    # TODO update this name
    def getOpenValve(self):
        openValve = self._computeLeastSquareRoot()
        self.log.debug("(" + str(self.address) + ") Need open valve: " + str(openValve))

        return openValve
