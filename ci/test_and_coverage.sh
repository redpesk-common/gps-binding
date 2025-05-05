SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Install or reinstall dependencies
for p in afb-binding-devel afb-libpython afb-test-py afb-cmake-modules libdb-devel gpsd; do
    sudo rpm -q $p && sudo dnf update -y $p || sudo dnf install -y $p 
done

# Install missing tools if needed
for p in g++ lcov; do
    command -v $p 2>/dev/null 2>&1 || sudo dnf install -y $p
done

CMAKE_COVERAGE_OPTIONS="-DCMAKE_C_FLAGS=--coverage -DCMAKE_CXX_FLAGS=--coverage"

rm -rf build
mkdir -p build
cd build
cmake ..
make
# sudo systemctl stop gpsd
# sudo systemctl stop gpsd.socket

gpsfake -S ../test/lorient.nmea &

LD_LIBRARY_PATH=. python  ../test/tests.py -vvv
