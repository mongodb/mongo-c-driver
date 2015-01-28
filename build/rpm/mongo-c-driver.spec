# norootforbuild

%define DriverName    mongo-c-driver
%define DriverVersion 1.1.1
%define BsonName      libbson
%define BsonVersion   1.1.1

Name:           %{DriverName}
Version:        %{DriverVersion}
Release:        1%{?dist}
Summary:        MongoDB C Driver
Group:          System Environment/Libraries

License:        ASL 2.0
URL:            https://github.com/mongodb/mongo-c-driver
Source0:        https://github.com/mongodb/mongo-c-driver/releases/download/%{DriverVersion}/mongo-c-driver-%{DriverVersion}.tar.gz
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  cyrus-sasl-devel
BuildRequires:  openssl-devel
BuildRequires:  pkgconfig

%description
mongo-c-driver is a library for building high-performance
applications that communicate with the MongoDB NoSQL
database in the C language. It can also be used to write
fast client implementations in languages such as Python,
Ruby, or Perl.


%package devel
Summary: Development files for mongo-c-driver
Requires: %{DriverName}%{?_isa} = %{DriverVersion}-%{release}
Group: Development/Libraries

%description devel
The %{DriverName}-devel package contains libraries and header files for
developing applications that use %{DriverName}.


%package -n %{BsonName}
Summary: A library for parsing and generating BSON documents.
Version: %{BsonVersion}
Group: System Environment/Libraries

%description -n %{BsonName}
Libbson is a library providing useful routines related to 
building, parsing, and iterating BSON documents. It is a 
useful base for those wanting to write high-performance 
C extensions to higher level languages such as Python, 
Ruby, or Perl.


%package -n %{BsonName}-devel
Summary: Development files for libbson
Requires: %{BsonName}%{?_isa} = %{BsonVersion}-%{release}
Version: %{BsonVersion}
Group: Development/Libraries

%description -n %{BsonName}-devel
The %{BsonName}-devel package contains libraries and header files for
developing applications that use %{BsonName}.


%prep
%setup -q -n %{DriverName}-%{DriverVersion}
automake

%build
%configure --disable-static --disable-silent-rules --enable-debug-symbols --enable-man-pages --enable-ssl --enable-sasl --with-libbson=bundled--enable-optimizations
make %{?_smp_mflags}

%check
make local-check
make abicheck

%install
%makeinstall
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%{_prefix}/share/doc/mongo-c-driver/*
%{_libdir}/libmongoc-1.0.so.*
%{_libdir}/libmongoc-priv.so*


%files -n %{BsonName}
%{_prefix}/share/doc/libbson/*
%{_libdir}/libbson-1.0.so.*


%files devel
%dir %{_includedir}/libmongoc-1.0
%{_includedir}/libmongoc-1.0/*.h
%{_includedir}/libmongoc-1.0/*.def
%{_includedir}/libmongoc-1.0/*.defs
%{_libdir}/libmongoc-1.0.so
%{_libdir}/pkgconfig/libmongoc-priv.pc
%{_libdir}/pkgconfig/libmongoc-1.0.pc
%{_libdir}/pkgconfig/libmongoc-ssl-1.0.pc
%{_bindir}/mongoc-stat
%{_prefix}/share/man/man3/mongoc*


%files -n %{BsonName}-devel
%dir %{_includedir}/libbson-1.0
%{_includedir}/libbson-1.0/*.h
%{_libdir}/libbson-1.0.so
%{_libdir}/pkgconfig/libbson-1.0.pc
%{_prefix}/share/man/man3/bson*


%changelog -n %{DriverName}
* Wed Jan 28 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.1-1
- Post-release bump for 1.1.0

* Wed Jan 28 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.0-1
- Release 1.1.0.

* Thu Nov 13 2014 Jason Carey <jason.carey@mongodb.com> - 1.1.0-rc0-1
- Release 1.1.0-rc0.

* Thu Oct 09 2014 Jason Carey <jason.carey@mongodb.com> - 1.0.3-1
- Post-release bump for 1.0.3.

* Thu Oct 09 2014 Jason Carey <jason.carey@mongodb.com> - 1.0.2-1
- Release 1.0.2.

* Tue Aug 26 2014 Christian Hergert <christian.hergert@mongodb.com> - 1.0.0-1
- Post-release bump for 1.0.1.

* Tue Aug 26 2014 Christian Hergert <christian.hergert@mongodb.com> - 1.0.0-1
- Release 1.0.0.

* Tue Aug 13 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.98.2-1
- Bump for 0.98.2.

* Tue Jul 17 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.98.1-1
- Bump for development release.

* Tue Jul 16 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.98.0-1
- Bump for 0.98.0.

* Tue Jun 20 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.5-1
- Bump for development releases.

* Tue Jun 20 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.4-1
- Release of 0.96.4

* Tue Jun 10 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.3-1
- Enable automated builds of 0.96.3

* Thu Jun 05 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.2-1
- Release 0.96.2

* Fri May 30 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.0-1
- Release 0.96.0

* Tue May 06 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.94.3-1
- Initial package


%changelog -n %{BsonName}
* Wed Jan 28 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.1-1
- Post-release bump for 1.1.0

* Wed Jan 28 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.0-1
- Release 1.1.0.

* Thu Nov 13 2014 Jason Carey <jason.carey@mongodb.com> - 1.1.0-rc0-1
- Release 1.1.0-rc0.

* Thu Oct 09 2014 Jason Carey <jason.carey@mongodb.com> - 1.0.3-1
- Post-release bump for 1.0.3.

* Thu Oct 09 2014 Jason Carey <jason.carey@mongodb.com> - 1.0.2-1
- Release 1.0.2.

* Tue Aug 26 2014 Christian Hergert <christian.hergert@mongodb.com> - 1.0.0-1
- Post-releases bump for 1.0.1.

* Tue Aug 26 2014 Christian Hergert <christian.hergert@mongodb.com> - 1.0.0-1
- Release 1.0.0.

* Tue Jul 16 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.98.0-1
- Bump for 0.98.0.

* Tue Jun 20 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.5-1
- Bump for development releases.

* Tue Jun 20 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.4-1
- Release of 0.8.4

* Tue Jun 10 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.3-1
- Enable automated builds of 0.8.3
