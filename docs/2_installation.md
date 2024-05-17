# GPS Binding installation guide

## redpesk target

Gps binding is available in standard redpesk repositories.

```bash
dnf install gps-binding
```

## Native

### From repository

Firstly, ["Verify Your Build Host"]({% chapter_link host-configuration-doc.setup-your-build-host %}). Indeed, your host needs to have a supported distribution.
Then, you can use the following command-line to get the `gps-binding` binding and all its dependencies. Please use the right paragraph, according to you distribution.

#### Ubuntu

Firstly, add the redpesk "sdk" repository in the list of your packages repositories.

[Installation RedpPesk Factory]({% chapter_link rp-cli-doc.installation %})

Then, update the list of packages and simply install the `gps-binding` package.

```bash
# Update the list of available packages
$ sudo apt update
# Installation of gps-binding
$ sudo apt-get install gps-binding
```

#### Fedora

Firstly, add the redpesk "sdk" repository in the list of your packages repositories.

[Installation RedpPesk Factory]({% chapter_link rp-cli-doc.installation %})

Then, simply install the `gps-binding` package.

```bash
dnf install gps-binding
```

#### OpenSUSE Leap

Firstly, add the redpesk "sdk" repository in the list of your packages repositories.

[Installation RedpPesk Factory]({% chapter_link rp-cli-doc.installation %})

Then, simply install the `gps-binding` package.

```bash
sudo zypper in gps-binding
```

### From sources

We advise you to use the [local builder]({% chapter_link local-builder-doc.installation %}) for building the binding sources. The local builder comes with everything setup to build redpesk projects.

#### Dependencies

- gcc
- make
- cmake
- afb-cmake-modules
- json-c
- afb-binding
- libmicrohttpd
- afb-libhelpers
- gpsd
- gpsd-devel
- gpsd-clients
- Userspace RCU library

Fedora/OpenSUSE/redpesk:

```bash
sudo dnf install gcc make cmake afb-cmake-modules json-c-devel afb-binding-devel libmicrohttpd afb-libhelpers-devel gpsd gpsd-devel gpsd-clients gpsd-libs userspace-rcu-devel
```

Ubuntu:

```bash
sudo apt install gcc make cmake afb-cmake-modules-bin libsystemd-dev libjson-c-dev afb-binding-dev libmicrohttpd12 afb-libhelpers-dev gpsd libgps-dev gpsd-clients liburcu-dev
```

#### Build & Install

```bash
git clone https://github.com/redpesk-common/gps-binding
cd gps-binding
mkdir build
cd build
cmake ..
make
make install
```
