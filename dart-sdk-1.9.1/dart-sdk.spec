Name: dart-sdk
Version: 1.9.1
Release: 1
Summary: Dart is an open-source, scalable programming language, with robust libraries and runtimes, for building web, server, and mobile apps.
License: MIT
Group: Development/Libraries
Vendor: dartlang.org
Url: https://www.dartlang.org/
Source: %name-%version.tar.gz
Prefix: %_prefix
BuildRoot: %{_tmppath}/%name-%version-root

%description
Dart is an open-source, scalable programming language, with robust libraries and runtimes, for building web, server, and mobile apps.

%prep
%setup -q

%install
mkdir -p $RPM_BUILD_ROOT/usr/lib
cp -r dart $RPM_BUILD_ROOT/usr/lib

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
/usr/lib/dart/*

%post
chmod +x /usr/lib/dart/bin/*
ln -snf /usr/lib/dart/bin/dart /bin/dart
ln -snf /usr/lib/dart/bin/pub /bin/pub
ln -snf /usr/lib/dart/bin/dart2js /bin/dart2js

