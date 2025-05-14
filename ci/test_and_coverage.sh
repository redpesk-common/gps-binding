# Install or reinstall dependencies
for p in afb-binding-devel afb-libpython afb-test-py afb-cmake-modules libdb-devel gpsd gpsd-clients gpsd-devel gpsd-libs userspace-rcu-devel  ; do
    sudo rpm -q $p && sudo dnf update -y $p || sudo dnf install -y $p 
done

# Install missing tools if needed
for p in g++ lcov; do
    command -v $p 2>/dev/null 2>&1 || sudo dnf install -y $p
done

CMAKE_COVERAGE_OPTIONS="-DCMAKE_C_FLAGS=--coverage -DCMAKE_CXX_FLAGS=--coverage"

function stop_gpsd {
        sudo systemctl stop gpsd.service >/dev/null
        sudo systemctl stop gpsd.socket >/dev/null
        pkill -9 gpsfake >/dev/null
        pkill -9 gpsd >/dev/null
}

echo "--- Stopping remaining gpsd & gpsfake instances ---"
stop_gpsd

rm -rf build
mkdir -p build
cd build
cmake ..
make

gpsfake -S ../test/lorient.nmea &

LD_LIBRARY_PATH=. python  ../test/tests.py -vvv

echo "--- Killing created gpsd & gpsfake instances ---"
stop_gpsd