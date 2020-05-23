#!/usr/bin/python

from constants import DEBUG

class Log:

    def __init__(self, listAllowPrefix = ["INFO", "WARN", "SEVERE"]):
        if(DEBUG):
            listAllowPrefix.append("DEBUG")
        self.listAllowPrefix = listAllowPrefix

    def severe(self, message):
        self._log("SEVERE", message)

    def warn(self, message):
        self._log("WARN", message)

    def debug(self, message):
        self._log("DEBUG", message)

    def info(self, message):
        self._log("INFO", message)

    def _log(self, prefix, message):
        if(prefix in self.listAllowPrefix):
            print("[" + str(prefix) + "] " + str(message))
