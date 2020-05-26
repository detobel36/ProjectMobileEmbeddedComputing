#!/usr/bin/python
# -*- coding: utf-8 -*-

import numpy as np

def leastSquareRoot(listValues):
    x = np.array([i for i in range(len(listValues))])
    y = np.array(listValues)
    A = np.vstack([x, np.ones(len(x))]).T
    m, c = np.linalg.lstsq(A, y, rcond=None)[0]
    return m