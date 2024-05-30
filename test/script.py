import os
import subprocess
import signal
import time

curpath = "/iotBZH.nmea"
abpath = os.path.abspath(os.path.dirname(__file__))
testpath = abpath+curpath
print(testpath)

p = subprocess.Popen(["gpsfake", "-S", testpath])
print(p.pid)

time.sleep(10.0)
os.killpg(os.getpgid(p.pid), signal.SIGKILL)