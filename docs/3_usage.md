# GPS binding usage

## Run

### GPSD initialization

The GPS binding is working as a GPSD client, so we need to launch this one first of all.

#### Using physical device

When a GPS is plugged in through serial or usb port, GPSD will automatically detect it and start it's daemon.

Tested with GlobalSat BU-353-S4 USB GPS device.

For further information, see official [GPSD website](https://gpsd.gitlab.io/gpsd/index.html):

#### Using an NMEA log

You may need to kill all other GPSD processes before running a nmea log :

##### Ubuntu

```bash
sudo killall gpsd
```

##### Fedora

```bash
sudo service gpsd stop
sudo service gpsd.socket stop
```

Then you can launch the log playing with :

```bash
gpsfake -S {file.nmea}
```

### Verify GPSD functioning

To check if GPSD is now providing expected data, you can use any GPSD client such as :

```bash
cgps -c
```

or

```bash
xgps
```

If GPSD is providing data as expected, you should see these data in the gui client you just opened.

### Start GPS binding

#### Manually built binding

In your build directory:

```bash
afb-binder --rootdir=./package --binding=./package/lib/libgps-binding.so --port=1234 --tracereq=common -vvv
```

#### Installed binding

If you installed the binding either through package or through make install command:

```bash
export GPS_BINDING_DIR=/var/local/lib/afm/applications/gps-binding
afb-binder \
--rootdir=$GPS_BINDING_DIR \
--binding=$GPS_BINDING_DIR/lib/libgps-binding.so \
--port=1234 \
--tracereq=common \
-vvv
```

### Call for GPS binding API

Now that the binding is running and attached to GPSD, you can call for any verb provided by the `gps` described in the [GPS API description](./4_api_description.html) part.

#### Binding UI

<!--TODO-->

#### Command line (afb-client)

```bash
afb-client --human 'ws://localhost:1234/api?token='
```

## Test

### Redpesk

To run test suite on Redpesk powered target, you should installed provided package:

```bash
dnf install gps-binding-redtest
```

Then launch the run-redtest script:

```bash
/usr/lib/gps-binding-redtest/redtest/run-redtest
```

### Native

#### Install test suite

In order to test the binding on host, you should install the following packages:

Install package `afb-test` like you did for the `gps-binding` package in [Installation](./2_installation.html)

#### Build test version of the binding

```bash
cd build
cmake -DBUILD_TEST_WGT=TRUE ..
make
make widget
```

#### Start automated test suite

Launch test script that will start test suite and coverage report generation.
The script is managing :

- gpsd instances cleaning
- nmea log replay through gpsfake
- binding test suite launch

```bash
./package-test/var/run-test.sh
```
