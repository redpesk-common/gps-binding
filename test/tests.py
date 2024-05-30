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


bindings = {"gps": f"libgps-binding.so"}

def setUpModule():
    configure_afb_binding_tests(bindings=bindings)
    
        

class TestVerbGps(AFBTestCase):

    "Test gps-data verb"
    def test_data_success(self):
             
        time.sleep(3.0)
        r = libafb.callsync(self.binder, "gps", "gps-data", {})
        print("STATUS = ", r.status)
        assert r.status == 0 # 0 = binding worked properly

        r = libafb.callsync(self.binder, "gps", "gps-data", {})
        d = r.args[0]
        print(type(r.args), type(d))
        #print(d)
        
        self.assertIn('visible satellites', d)
        self.assertIn('used satellites', d)
        self.assertIn('mode', d)
        self.assertIn('latitude', d)
        self.assertIn('longitude', d)
        self.assertIn('speed', d)
        self.assertIn('altitude', d)
        self.assertIn('altitude error', d)
        self.assertIn('climb', d)
        self.assertIn('climb error', d)
        self.assertIn('heading (true north)', d)
        self.assertIn('timestamp', d)
        self.assertIn('timestamp error', d)

        self.assertTrue(type(d['visible satellites']) == int)
        self.assertTrue(type(d['used satellites']) == int)
        self.assertTrue(type(d['mode']) == int)   
        self.assertTrue(type(d['latitude']) == float)
        self.assertTrue(type(d['longitude']) == float)
        self.assertTrue(type(d['speed']) == float)
        self.assertTrue(type(d['altitude']) == float)
        self.assertTrue(type(d['altitude error']) == float)
        self.assertTrue(type(d['climb']) == float)
        self.assertTrue(type(d['climb error']) == float)
        self.assertTrue(type(d['heading (true north)']) == float)
        self.assertTrue(type(d['timestamp']) == float)
        self.assertTrue(type(d['timestamp error']) == float)

        print("DATA SUCCESS DONE")


    "Test subscribe verb"
    def test_subscribe_success(self):

        freqList = [1, 10, 20, 50, 100]
        for f in range(len(freqList)):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : freqList[f]})
            assert r.status == 0 #0 = binding worked properly

        movList = [1, 10, 100, 300, 500, 1000]
        for m in range(len(movList)):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "movement", "value" : movList[m]})
            assert r.status == 0 #0 = binding worked properly

        speedList = [20, 30, 50, 90, 110, 130]
        for s in range(len(speedList)):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : speedList[s]})
            assert r.status == 0 #0 = binding worked properly

        #testing double subscription 
        r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 1})
        assert r.status == 0 #0 = binding worked properly

        print("SUBSCRIBE SUCCESS DONE")
        

    def test_subscribe_fail(self):

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : 0.5})
        
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "movement", "value" : 0.5})
        
        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : 0.5})

        with self.assertRaises(RuntimeError):
            r = libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "noCond", "value" : 1})

        print("SUBSCRIBE FAILS DONE")


    "Test unsubscribe verb"
    def test_unsubscribe_success(self):

        freqList = [1, 10, 20, 50, 100]
        for f in range(len(freqList)):
            libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "frequency", "value" : freqList[f]})
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "frequency", "value" : freqList[f]})
            assert r.status == 0 #0 = binding worked properly

        movList = [1, 10, 100, 300, 500, 1000]
        for m in range(len(movList)):
            libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "movement", "value" : movList[m]})
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "movement", "value" : movList[m]})
            assert r.status == 0 #0 = binding worked properly

        speedList = [20, 30, 50, 90, 110, 130]
        for s in range(len(speedList)):
            libafb.callsync(self.binder, "gps", "subscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : speedList[s]})
            r = libafb.callsync(self.binder, "gps", "unsubscribe", {"data" : "gps_data", "condition" : "max_speed", "value" : speedList[s]})
            assert r.status == 0 #0 = binding worked properly

        print("UNSUBSCRIBE SUCCESS DONE")
    

    def test_unsubscribe_fail(self):

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

        print("UNSUBSCRIBE FAILS DONE")


    "Test info verb"
    def test_info_success(self):

        r = libafb.callsync(self.binder, "gps", "info")
        assert r.status == 0 #0 = binding worked properly
        
        print("TEST INFO DONE")



class TestEventGps(AFBTestCase):
    ""

if __name__ == "__main__":
    run_afb_binding_tests(bindings)
