#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

function stop_gpsd {
        sudo systemctl stop gpsd.service >/dev/null
        sudo systemctl stop gpsd.socket >/dev/null
        sudo pkill -9 gpsfake >/dev/null
        sudo pkill -9 gpsd >/dev/null
}

echo "--- Stopping remaining gpsd & gpsfake instances ---"
stop_gpsd

echo "--- Launching gpsfake instance ---"
gpsfake -q -S $DIR/lorient.nmea &

#gpsfake need few seconds before having enough data to be reliable
sleep 3

LD_LIBRARY_PATH=${SCRIPT_DIR}/../build python ${SCRIPT_DIR}/tests.py -vvv --tap

echo "--- Killing created gpsd & gpsfake instances ---"
stop_gpsd

