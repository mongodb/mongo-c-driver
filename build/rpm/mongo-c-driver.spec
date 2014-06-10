Name:           mongo-c-driver
Version:        0.96.3
Release:        1%{?dist}
Summary:        BSON library

License:        ASL 2.0
URL:            https://github.com/mongodb/mongo-c-driver
Source0:        https://github.com/mongodb/mongo-c-driver/releases/download/0.96.3/mongo-c-driver-0.96.3.tar.gz
BuildRequires:  automake
BuildRequires:  libbson-devel
BuildRequires:  cyrus-sasl-devel
BuildRequires:  openssl-devel
BuildRequires:  pkgconfig

%description
mongo-c-driver is a library for building high-performance
applications that communicate with the MongoDB NoSQL
database in the C language. It can also be used to write
fast client implementations in languages such as Python,
Ruby, or Perl.

%package        devel
Summary:        Development files for mongo-c-driver
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%prep
%setup -q -n %{name}-%{version}
automake 

%build
%configure --disable-static --disable-silent-rules --enable-debug-symbols --docdir=%{_pkgdocdir} --enable-debug --enable-man-pages --enable-ssl --enable-sasl --with-libbson=system --enable-optimizations
make %{?_smp_mflags}

%check
make local-check
make abicheck

%install
%make_install
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%doc COPYING README NEWS
%{_libdir}/*.so.*

%files devel
%dir %{_includedir}/libmongoc-1.0
%{_includedir}/libmongoc-1.0/*.h
%{_libdir}/libmongoc-1.0.so
%{_libdir}/pkgconfig/libmongoc-1.0.pc
%{_libdir}/pkgconfig/libmongoc-ssl-1.0.pc
%{_bindir}/mongoc-stat
%{_prefix}/share/man/man3/*

%changelog
* Thu Jun 10 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.3-1
- Enable automated builds of 0.96.3

* Thu Jun 05 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.2-1
- Release 0.96.2

* Fri May 30 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.96.0-1
- Release 0.96.0

* Tue May 06 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.94.3-1
- Initial package
