#!/bin/bash

#Running this script as root may be a problem while launching afm-test command
if [ "$EUID" -eq 0 ]
then
        echo "Please do not run this script as root !"
        exit
fi


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

function stop_gpsd {
        sudo systemctl stop gpsd.service
        sudo systemctl stop gpsd.socket
        sudo pkill -9 gpsfake
        sudo pkill -9 gpsd
}

echo "--- Stopping remaining gpsd & gpsfake instances ---"
stop_gpsd

echo "--- Launching gpsfake instance ---"
gpsfake -q -S $DIR/test.nmea &

echo "--- Launching GPS binding tests ---"
afm-test package package-test/ -t 10 -c

echo "--- Killing created gpsd & gpsfake instances ---"
stop_gpsd

