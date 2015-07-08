# norootforbuild
%define mod_name dbi
%define rb_ver 1.8

Name: ruby-dbi
Version: 0.1.1
Release: 4.2%{?release_prefix}

License: BSD
Group: Development/Languages/Ruby

BuildRoot: %{_tmppath}/%{name}-%{version}-build
BuildRequires: ruby ruby-devel

URL: http://rubyforge.org/projects/ruby-dbi
Source: http://rubyforge.org/frs/download.php/12368/%{name}-%{version}.tar.gz


Summary: Ruby/DBI develops a database independent interface for accessing databases - similar to Perl's DBI

%description
Ruby/DBI develops a database independent interface for accessing databases - similar to Perl's DBI 

%prep
%setup -n %{name}-%{version}


%build
ruby setup.rb config \
    --rb-dir=%{_libdir}/ruby/%{rb_ver} \
    --without="dbd_sybase,dbd_sqlite" \
    --with="dbi,dbd_proxy,dbd_mysql,dbd_pg,dbd_frontbase,dbd_db2,dbd_oracle,dbd_odbc,dbd_ado,dbd_msql,dbd_interbase,dbd_sqlrelay"
ruby setup.rb setup

%install
DESTDIR="%{buildroot}" ruby setup.rb install

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/proxyserver.rb
%{_bindir}/sqlsh.rb
%{_libdir}/ruby/%{rb_ver}/DBD/
%{_libdir}/ruby/%{rb_ver}/dbi/
%{_libdir}/ruby/%{rb_ver}/dbi.rb
%doc README LICENSE ChangeLog doc/*
%doc examples/ test/

%changelog
* Mon Mar 22 2010 Douglas Hubler <douglas@hubler.us>
- Remove noarch because ruby expects _libdir is different locations on 32 
  and 64 bit systems: /usr/lib and /usr/lib64. NOTE: It is unclear if ruby on 
  64 bit machines would load from /usr/lib/ruby, if it could then you could 
  swap each reference to _libdir variable with fixed value /usr/lib

* Tue Nov 15 2006 Damian Krzeminski <damian@pingtel.com> - dbi-2
- Install in ruby lib directory and not in site_ruby directory.

* Tue Nov 14 2006 Douglas Hubler <dhubler@penguin.pingtel.com> - dbi-1
- Initial build.

