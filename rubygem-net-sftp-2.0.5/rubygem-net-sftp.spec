%global gemdir %(ruby -rubygems -e 'puts Gem::dir' 2>/dev/null)
%global gemname net-sftp
%global geminstdir %{gemdir}/gems/%{gemname}-%{version}
%global rubyabi 1.8

Summary: A pure Ruby implementation of the SFTP client protocol
Name: rubygem-%{gemname}
Version: 2.0.5
Release: 2%{?dist}
Group: Development/Languages
License: MIT or LGPLv2
URL: http://net-ssh.rubyforge.org/sftp
Source0: http://rubygems.org/downloads/rubygem-%{gemname}-%{version}.tar.gz
Requires: ruby(abi) = %{rubyabi}
Requires: rubygems
Requires: rubygem(net-ssh)
BuildRequires: rubygems
BuildArch: noarch
Provides: rubygem(%{gemname}) = %{version}

%description
A pure Ruby implementation of the SFTP client protocol

%package        doc
Summary:        Documentation for %{name}
Group:          Documentation
Requires:       %{name} = %{version}-%{release}

%description	doc
This package contains documentation for %{name}.

%prep
%setup

%build

# NOTE: There are some tests, but they require 'echoe' and a lot of patching.
# I've contacted upstream to fix this.
%check

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{gemdir}
gem install --local --install-dir %{buildroot}%{gemdir} \
            --force --rdoc ./%{gemname}-%{version}.gem

%clean
rm -rf %{buildroot}

%files
%defattr(-, root, root, -)
%dir %{geminstdir}
%{geminstdir}/lib
%doc %{geminstdir}/CHANGELOG.rdoc
%doc %{geminstdir}/Manifest
%doc %{geminstdir}/README.rdoc
%{gemdir}/cache/%{gemname}-%{version}.gem
%{gemdir}/specifications/%{gemname}-%{version}.gemspec

%files doc
%defattr(-, root, root, -)
%{geminstdir}/test
%{geminstdir}/Rakefile
%{geminstdir}/net-sftp.gemspec
%{gemdir}/doc/%{gemname}-%{version}
# License: LGPL version 2.1
%{geminstdir}/setup.rb

%changelog
* Thu Oct 14 2010 Michal Fojtik <mfojtik@redhat.com> - 2.0.5-2
- Fixed license
- Fixes source0 URL

* Wed Oct 13 2010 Michal Fojtik <mfojtik@redhat.com> - 2.0.5-1
- Initial package
