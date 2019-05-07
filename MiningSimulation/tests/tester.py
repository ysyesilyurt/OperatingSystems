#!/usr/bin/env python3

# tester for automated testing of MiningSimulation
# Created by Yavuz Selim Yesilyurt on 28 April 2019

# "tests" directory needs to be in the same dir with your makefile and stuff.

import datetime
import os
import re
from subprocess import Popen


def runTests():
    inpCount = 0
    wrongForm = 0
    os.chdir("..")
    p = Popen(['make', 'all'])
    retCode = p.wait()
    if not retCode:
        for inp in os.listdir("tests/inputs"):
            out = re.sub("inp(?P<num>[0-9]).txt", "out\g<num>.txt", inp)
            if out == inp:
                wrongForm += 1
                continue
            else:
                with open("tests/inputs/" + inp, "r") as inpFile:
                    inpCount += 1
                    with open("tests/outputs/{}".format(out), "w") as outFile:
                        p = Popen(['./simulator'], stdin=inpFile, stdout=outFile)
                        p.wait()
        with open("tests/tester.log", "a") as log:
            if not wrongForm:
                log.write("Ran {0} tests successfully at {1}."
                          .format(inpCount, datetime.datetime.now().strftime("%Y-%m-%d-%H:%M:%S")) + '\n')
            else:
                log.write("Ran {0} tests successfully at {1}."
                          .format(inpCount, datetime.datetime.now().strftime("%Y-%m-%d-%H:%M:%S")) + 'Could not run {} '
                          'tests due to wrong form of their names'.format(wrongForm))
    else:
        with open("tests/tester.log", "a") as log:
            log.write("make failed at {}".format(datetime.datetime.now().strftime("%Y-%m-%d-%H:%M:%S"))
                      + " make returned {} code".format(retCode) + '\n')


if __name__ == "__main__":
    runTests()
