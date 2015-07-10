#
# Docker container for building sipx-externals
#

FROM centos:centos6
MAINTAINER Joegen Baclor <joegen@ossapp.com>

#
# Install EPEL repository
#
RUN yum -y update; yum -y install epel-release; yum clean all; yum -y --disablerepo=epel update  ca-certificates
RUN sed -i "s/#baseurl/baseurl/" /etc/yum.repos.d/epel.repo; sed -i "s/mirrorlist/#mirrorlist/" /etc/yum.repos.d/epel.repo

#
# Install Dependency Package
#

RUN yum -y install \
	automake \
	bison \
	bzip2-devel \
	boost-devel \
	chrpath \
	createrepo \
	db4-devel \
	elfutils-devel \
	elfutils-libelf-devel \
	findutils \
	flex \
	gcc-c++ \
	git \
	gtest-devel \
	hiredis-devel \
	iproute \
	iptables \
	leveldb-devel \
	libacl-devel \
	libconfig-devel \
	libdnet-devel \
	libevent-devel \
	libmcrypt-devel \
	libpcap-devel \
	libselinux-devel \
	libsrtp-devel \
	libtool \
	libtool-ltdl-devel \
	lm_sensors-devel \
	m4 \
	mysql-devel \
	net-tools \
	openssl-devel \
	pcre-devel \
	perl \
	perl-devel \
	perl-TAP-Harness-Archive \
	perl-TAP-Harness-JUnit \
	perl-ExtUtils-Embed \
	poco-devel \
	postgresql-devel \
	python-devel \
	python-setuptools \
	rpm-build \
	rpm-devel \
	ruby \
	ruby-devel \
	rubygem-mocha \
	rubygem-rake \
	rubygems \
	scons \
	tar \
	tcp_wrappers-devel \
	tetex-dvips \
	texinfo-tex \
	tokyocabinet-devel \
	v8-devel \
	xmlrpc-c-devel; \
	yum clean all


