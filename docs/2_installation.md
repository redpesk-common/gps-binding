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

#### Ubuntu 20.04 and 18.04

Firstly, add the redpesk "sdk" repository in the list of your packages repositories.

```bash
# Add the repository in your list
$ echo "deb https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/$DISTRO/ ./" | sudo tee -a /etc/apt/sources.list
# Add the repository key
$ curl -L https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/$DISTRO/Release.key | sudo apt-key add -
```

Then, update the list of packages and simply install the `gps-binding` package.

```bash
# Update the list of available packages
$ sudo apt update
# Installation of gps-binding
$ sudo apt-get install gps-binding
```

#### Fedora 31, 32 and 33

Firstly, add the redpesk "sdk" repository in the list of your packages repositories.

```bash
$ cat << EOF > /etc/yum.repos.d/redpesk-sdk.repo
[redpesk-sdk]
name=redpesk-sdk
baseurl=https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/$DISTRO
enabled=1
repo_gpgcheck=0
type=rpm
gpgcheck=0
skip_if_unavailable=True
EOF
```

Then, simply install the `gps-binding` package.

```bash
dnf install gps-binding
```

#### OpenSUSE Leap 15.1 and 15.2

Firstly, add the redpesk "sdk" repository in the list of your packages repositories.

```bash
$ OPENSUSE_VERSION=15.2 # Set the right OpenSUSE version
# Add the repository in your list
$ sudo zypper ar https://download.redpesk.bzh/redpesk-lts/{{ site.redpesk-os.latest }}/sdk/$DISTRO/ redpesk-sdk
# Refresh your repositories
$ sudo zypper ref
```

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
