# GPS API for redpesk

## Pre-Requisites
This binding is using GPSD in order to get positioning information from a physical GPS device.

In order to build and run the binding, you will need several packages, gpsd itself, developments libraries and gpsd-clients.

### Ubuntu/Debian
```bash
sudo apt install gpsd libgps-dev gpsd-clients liburcu-dev
```

### Fedora
```bash
sudo dnf install gpsd gpsd-clients gpsd-devel gpsd-libs userspace-rcu-devel
```

For Fedora 32 & 33, gpsd-devel is no longer available, you can follow [this guide](https://gpsd.gitlab.io/gpsd/installation.html#_special_notes_for_fedora_derivatives) to build it yourself.

## Build & run the binding

You can build the binding in the same way than any other binding :
```bash
mkdir build
cd build
cmake ..
make
```

Then, you can run the binding with the command suggested at the end of the previous make command.

## Verbs and subscription

The API exposed by the binding is : ```gps```

The binding has been made for clients to subscribe to it, so it has only three verbs :

| Verb          | Description                                       |
|---------------|---------------------------------------------------|
| gps_data      | Get last data that came from GPSD                 |
| subscribe     | Subscribe to gps data with a specific condition   |
| unsubscribe   | Unsubscribe to gps data with a specific condition |

### gps_data

```bash
gps gps_data
```

### subscribe

- available data :
    - gps_data

- available condition & values :
    - frequency (hz)
        * 1, 10, 20, 50, 100
    - movement (m)
        * 1, 10, 100, 300, 500, 1000
    - max_speed (km/h)
        * 20, 30, 50, 90, 110, 130

- examples :

Get data a 10Hz
```bash
gps subscribe {"data" : "gps_data", "condition" : "frequency", "value" : 10}
```

Get event if there is a movement of at least 1 meter since last event
```bash
gps subscribe {"data" : "gps_data", "condition" : "movement", "value" : 1}
```

Get an event each time the speed exceeds 20 km/h
```bash
gps subscribe {"data" : "gps_data", "condition" : "max_speed", "value" : 20}
```

### unsubscribe

Exactly the same as the subscribe verb

### JSON Answer format

The content of the answer is rawly coming from the libgps, you can find a lot of information about them directly in this library.
Here is the Json content :

| Key                   | Type		| Description                                           |
|-----------------------|-----------|-------------------------------------------------------|
| visible satellites    | Int       | Number of visible satellites							|
| used satellites       | Int       | Number of satellites in used                          |
| mode                  | Int       | Mode of fix (0 to 3) 									|
| latitude              | Double    | Latitude in degrees									|
| longitude             | Double    | Longitude in degrees									|
| speed                 | Double    | Speed over ground, meters/sec 						|
| altitude              | Double    | Altitude in meters 									|
| climb                 | Double    | Vertical speed, meters/sec 							|
| heading (true north)  | Double    | Course made good (relative to true north) 			|
| timestamp             | Double    | Standard timestamp 									|

Each value from "Latitude" is also accompanied by its error value expressed in the same unit as this one. (ex : latitude error).

## Environment variables

| Name              | Description                      |
|-------------------|:---------------------------------|
| RPGPS\_HOST       | hostname to connect to           |
| RPGPS\_SERVICE    | service to connect to (tcp port) |


## Testing the binding

To test the binding without any physical device, you can use the gpsfake tool that is included in the the "gpsd-clients" package.

When launched with a NMEA log, gpsfake will be seen as a regular gpsd instance by the binding.

A very little log has been provided with the binding sources and can be found in the "log" directory, it can be used to be sure of the way of using logs but it is not reflecting real life values.

You may need to kill all other gpsd processes before running gpsfake :
### Ubuntu
```bash
sudo killall gpsd
```
### Fedora
```bash
sudo systemctl stop gpsd
sudo systemctl stop gpsd.socket
```

Then you can launch the log playing with :
```bash
gpsfake -S {file.nmea}
```

You should now be able to see data with gps client like :
```bash
cgps -s
```
or
```bash
xgps
```

## Use the tests suite of the binding

```bash
mkdir build
cd build
cmake -DBUILD_TEST_WGT=TRUE ..
make
make widget
./package-test/var/run-test.sh
```

If you want to launch tests manually using the afm-test suite, you should run a working gpsd instance before running them.


