from afb_test import AFBTestCase, configure_afb_binding_tests, run_afb_binding_tests
"""
Usage :
You might need to terminate gpsd's process before running tests by using the command :
    On FEDORA
sudo systemctl stop gpsd 
    On UBUNTU
sudo 
To run the file 'tests.py' us the command
python tests.py --path ../build
"""

import libafb
import os
import subprocess
import signal
import time
import unittest
from math import radians, sin, cos, asin, sqrt


bindings = {"gps": f"gps-binding.so"}

def setUpModule():
    configure_afb_binding_tests(bindings=bindings)
    

class TestVerbGps(AFBTestCase):

    "Test gps-data verb"
    def test_data_success(self):
        time.sleep(1.0) # add a sleep time to wait for the gpsd to start
             
        r = libafb.callsync(self.binder, "gps", "gps-data", {})
        assert r.status == 0 # 0 = binding worked properly

        r = libafb.callsync(self.binder, "gps", "gps-data", {})
        dicto = r.args[0]
        
        assert 'visible satellites' in dicto
        assert 'used satellites' in dicto
        assert 'mode' in dicto
        assert 'latitude' in dicto
        assert 'longitude' in dicto
        assert 'speed' in dicto
        assert 'altitude'in dicto
        # assert 'altitude error' in dicto
        assert 'climb' in dicto
        # assert 'heading (true north)' in dicto
        assert 'timestamp' in dicto
        assert 'timestamp error' in dicto

        assert type(dicto['visible satellites']) == int
        assert type(dicto['used satellites']) == int
        assert type(dicto['mode']) == int
        assert type(dicto['latitude']) == float
        assert type(dicto['longitude']) == float
        assert type(dicto['speed']) == float
        assert type(dicto['altitude']) == float
        # assert type(dicto['altitude error']) == float
        assert type(dicto['climb']) == float
        assert type(dicto['climb error']) == float
        # assert type(dicto['heading (true north)']) == float
        assert type(dicto['timestamp']) == float
        assert type(dicto['timestamp error']) == float

    
    def test_data_fail(self):
        time.sleep(1.0) # add a sleep time to wait for the gpsd to start

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "noData", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "noData", "condition" : "frequency", "value" : 1})       
        

    "Test subscribe verb"
    def test_subscribe_unsubscribe(self):
        time.sleep(1.0) # add a sleep time to wait for the gpsd to start

        freqList = [1, 10, 20, 50, 100]

        for freq in freqList:
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : freq})
            assert r.status == 0 #0 = binding worked properly
            
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "frequency", "value" : freq})
            assert r.status == 0      

        movList = [1, 10, 100, 300, 500, 1000]


        for m in range(len(movList)):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "movement", "value" : movList[m]})
            assert r.status == 0
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "movement", "value" : movList[m]})
            assert r.status == 0

        speedList = [20, 30, 50, 90, 110, 130]


        for s in range(len(speedList)):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : speedList[s]})
            assert r.status == 0
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : speedList[s]})
            assert r.status == 0

        #testing double subscription 
        r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        assert r.status == 0
        r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})


    def test_subscribe_fail(self):
        time.sleep(1.0) # add a sleep time to wait for the gpsd to start

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 0.5})

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "movement", "value" : 0.5})

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : 0.5})

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : "hello"})

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "noCond", "value" : 1})

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"dataaa" : "gps_data", "condition" : "frequency", "value" : 1})


    def test_unsubscribe_fail(self):
        time.sleep(1.0) # add a sleep time to wait for the gpsd to start

        #testing double unsubscription 
        libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})

        #testing wrong condition 
        libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "noCond", "value" : 1})

        #testing not matching requests
        libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 100})

        libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "movement", "value" : 10})
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "movement", "value" : 100})

        libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : 20})
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : 110})

        libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "movement", "value" : 1})


    "Test info verb"
    def test_info_success(self):
        time.sleep(1.0) # add a sleep time to wait for the gpsd to start

        r = libafb.callsync(self.binder, "gps", "info")
        assert r.status == 0


class TestEventGps(AFBTestCase):

    def test_event_success(self):
        time.sleep(1.0) # add a sleep time to wait for the gpsd to start
        count = 0
        def evt_freq(binder, evt_name, userdata, data):
            nonlocal count
            count += 1

        e = libafb.evthandler(self.binder, {"uid": "gps", "pattern": "gps/*", "callback": evt_freq})
        r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 10})
        time.sleep(5.0)
        print("count = ", count)
        assert 40 <= count <= 55
        libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 10})
        libafb.evtdelete(self.binder, "gps/*")

        ###
        #   Calculate the great circle distance in kilometers 
        #   between two points on the earth (specified in decimal degrees)
        #   BEWARE : in this computation the Earth is considered a perfect sphere  
        ###
        def haversine(lon1, lat1, lon2, lat2):
            lon1, lat1, lon2, lat2 = map(radians, [lon1, lat1, lon2, lat2])
            dlon = lon2 - lon1
            dlat = lat2 - lat1
            a = sin(dlat / 2) ** 2 + cos(lat1) * cos(lat2) * sin(dlon / 2) ** 2
            return 2 * 6371 * asin(sqrt(a))

        distance = 0
        lo1 = 0
        lo2 = 0
        la1 = 0
        la2 = 0
        def evt_move(binder, evt_name, userdata, data):
            nonlocal distance
            nonlocal lo1
            nonlocal lo2
            nonlocal la1 
            nonlocal la2
            lo2 = data["longitude"]
            la2 = data["latitude"]
            distance = haversine(lo1, la1, lo2, la1)
        
        e = libafb.evthandler(self.binder, {"uid": "gps", "pattern": "gps/*", "callback": evt_move})
        # with the speed defined in the simulation you'll have one callback every 3 seconds
        r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "movement", "value" : 100})
        time.sleep(10.0)
        if(la1 != 0 & lo1 != 0): assert 90 <= d <= 110
        libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "movement", "value" : 100})
        libafb.evtdelete(self.binder, "gps/*")


        target = 20
        speed = 0
        def evt_speed(binder, evt_name, userdata, data):
            nonlocal speed
            dicto = data
            speed = dicto["speed"]

        e = libafb.evthandler(self.binder, {"uid": "gps", "pattern": "gps/*", "callback": evt_speed})
        r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : target})
        time.sleep(5.0)
        assert speed >= target * 0.90
        libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : target})



if __name__ == "__main__":
    run_afb_binding_tests(bindings)
