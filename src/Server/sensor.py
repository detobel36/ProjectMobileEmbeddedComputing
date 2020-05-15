#!/usr/bin/python

DEBUG = True

"""
TODO:
- Made computation
- Remove old unused values
"""


class Sensor:

    def __init__(self, address):
        self.address = address
        self.listValues = list()

    def addValue(self, value):
        self.listValues.append(value)
        if(DEBUG):
            print("[DEBUG] (" + str(self.address) + ") Add value " + str(value) + " " + \
                "(total " + str(len(self.listValues)) + ")")

    def _computeLeastSquareRoot(self):
        # TODO
        return False

    # TODO update this name
    def getOpenValve(self):
        if(len(self.listValues) >= 30):
            openValve = self._computeLeastSquareRoot()
            if(DEBUG):
                print("[DEBUG] (" + str(self.address) + ") Need open valve: " + str(openValve))
            return openValve
        if(DEBUG):
            print("[DEBUG] (" + str(self.address) + ") No enought value for valve " + \
                "(total " + str(len(self.listValues)) + ")")
        return None
