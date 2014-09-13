Name:		divine
Version:	0
Release:	1%{?dist}
Summary:	Multi-core model checking system for proving specifications

Group:		Applications/Engineering
License:	BSD and GPLv2+ and LGPLv2
URL:		http://divine.fi.muni.cz/
Source0:	http://divine.fi.muni.cz/divine-%{version}.tar.gz

BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires:	cmake

BuildRequires: perl cmake llvm-devel libxml2-devel boost-devel bison
Requires:      clang

%description
DiVinE is a tool for LTL model checking and reachability analysis of discrete
distributed systems. The tool is able to efficiently exploit the aggregate
computing power of multiple network-interconnected multi-cored workstations in
order to deal with extremely large verification tasks. As such it allows to
analyse systems whose size is far beyond the size of systems that can be
handled with regular sequential tools.

%prep
%setup -q
%setup -q -T -D

%build

export CXXFLAGS="%{optflags}" CFLAGS="%{optflags}"

# "configure" is from cmake, not GNU, so there is no libdir option to invoke.
# Therefore, ignore the rpmlint warning from this line:
./configure -DCMAKE_INSTALL_PREFIX=/usr $CMAKE_FLAGS -DDIVINE_BINUTILS=ON

# Use "make -k" so we can get all the errors at once; this is particularly
# helpful with koji builds. Use VERBOSE=1 so we can check options invoked.
make -k %{?_smp_mflags} VERBOSE=1

%check
make check

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/{bin,share}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/*
/usr/include/*
/usr/lib/*
/usr/share/*
%doc README COPYING AUTHORS HACKING NEWS
%doc examples/

%changelog
* Fri Jun 20 2014 Vladimir Still <xstill at fi.muni.cz> 3.1pre-1
- Updated .spec: dependencies, better support for building in NIX VM

* Wed Apr 25 2012 Petr Rockai <me at mornfall.net> 2.5.1-1
- Reboot. New upstream version.

* Sat Mar 21 2009 David A. Wheeler <dwheeler at, dwheeler.com> 1.3-7
- Installs more binaries.  As a result, has more dependencies.
- Switched to released version of nips
- Added optionally-invoked test

* Wed Mar  4 2009 David A. Wheeler <dwheeler at, dwheeler.com> 1.3-6
- Recompile .jar files

* Wed Mar  4 2009 David A. Wheeler <dwheeler at, dwheeler.com> 1.3-5
- gcc flags - exported before ./configure, removed -fomit-frame-pointer
- Added VERBOSE=1 as a make argument
- examples now packaged as documentation
- Let rpmlint whine.
- examples/ packaged as documentation.
- No longer use "--help" as a test; there are better tests anyway.
- Fixed sed to remove -gstabs+

* Wed Mar  4 2009 David A. Wheeler <dwheeler at, dwheeler.com> 1.3-4
- Fix tests (in "check") to verify that we get correct results.

* Mon Mar  2 2009 David A. Wheeler <dwheeler at, dwheeler.com> 1.3-3
- Patch so that it compiles with gcc 4.4 (Fedora 11)

* Mon Feb 23 2009 David A. Wheeler <dwheeler at, dwheeler.com> 1.3-2
- Doesn't build on ppc64 - fixed spec file so it doesn't try.

* Fri Feb 20 2009 David A. Wheeler <dwheeler at, dwheeler.com> 1.3-1
- Initial packaging

