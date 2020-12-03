###########################################################################
# Copyright 2015 - 2020 IoT.bzh
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################
Name:    gps-binding
#Hexsha: 3997d85c023e7ad9068f615c1c3e302a45acc541
#Hexsha: 3997d85c023e7ad9068f615c1c3e302a45acc541
Version: 0.0.0+20200701+0+g3997d85c
Release: 9%{?dist}
License: GPLv3
Summary: gps api for redpesk
URL:     http://git.ovh.iot/redpesk-common/gps-binding
Source0: %{name}-%{version}.tar.gz

BuildRequires: afm-rpm-macros
BuildRequires: cmake
BuildRequires: gcc gcc-c++
BuildRequires: afb-cmake-modules
BuildRequires: pkgconfig(json-c)
BuildRequires: pkgconfig(libsystemd) >= 222
BuildRequires: pkgconfig(afb-binding)
BuildRequires: pkgconfig(libmicrohttpd) >= 0.9.55
BuildRequires: pkgconfig(afb-helpers)
BuildRequires: gpsd gpsd-clients gpsd-devel gpsd-libs
BuildRequires: userspace-rcu-devel
Requires: gpsd gpsd-clients gpsd-devel gpsd-libs

%description
The gps api is using gpsd to provide GNSS data.

# main package: default install widget in /var/local/lib/afm/applications/%%{name}
%afm_package
# test package: default install widget in /var/local/lib/afm/applications/%%{name}-test
%afm_package_test
%afm_package_redtest

%prep
%autosetup -p 1

%build
%afm_configure_cmake
%afm_build_cmake

%install
%afm_makeinstall

%check

%clean

%changelog
