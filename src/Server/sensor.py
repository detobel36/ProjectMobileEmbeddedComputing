#!/usr/bin/python

class Sensor:

    def __init__(self, address):
        self.address = address
        self.listValues = list()

    def addValue(self, value):
        self.listValues.append(value)

    def _computeLeastSquareRoot(self):
        # TODO
        return 0

    # TODO update this name
    def getOpenValve(self):
        if(len(self.listValues) >= 30):
            return self._computeLeastSquareRoot()
        return None
