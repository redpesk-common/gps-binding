Name: gps-binding
Version: 2.0.0
Release: 1%{?dist}
License:  APL2.0
Summary: Gps service set to be used in the redpesk
URL: https://git.ovh.iot/redpesk/redpesk-common/gps-binding
Source: %{name}-%{version}.tar.gz

%global _afmappdir %{_prefix}/redpesk

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  afb-cmake-modules
BuildRequires:  pkgconfig(json-c)
BuildRequires:  pkgconfig(afb-binding)
BuildRequires:  pkgconfig(afb-libhelpers)
BuildRequires:  pkgconfig(afb-libcontroller)
BuildRequires:  pkgconfig(libsystemd) >= 222
BuildRequires:  pkgconfig(afb-helpers4)
BuildRequires:  pkgconfig(libgps)
BuildRequires:  pkgconfig(liburcu)


%if 0%{?suse_version}
BuildRequires:  libdb-4_8-devel
%else
BuildRequires:  libdb-devel
%endif

Requires:       afb-binder

%description
This binding provide a gps service


%package redtest
Summary: redtest package
Requires: %{name} = %{version}-%{release}
%description redtest 


%prep
%autosetup -p 1

%build
%cmake -DAFM_APP_DIR=%{_afmappdir} .
%cmake_build

%install
%cmake_install

%files
%defattr(-,root,root)
%dir %{_afmappdir}/%{name}
%{_afmappdir}


%files redtest
%{_libexecdir}/redtest/%{name}/run-redtest
%{_libexecdir}/redtest/%{name}/tests.py
%{_libexecdir}/redtest/%{name}/lorient.nmea

%%changelog



