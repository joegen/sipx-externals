MODULES = \
	cfengine-3.3.8  \
	mongodb-2.6.7 \
        mongo-cxx-driver-legacy-0.0-26compat-2.6.7 \
        dart-sdk-1.9.1  \
        net-snmp-5.7.1 \
        ruby-postgres-0.7.1 \
	ruby-dbi-0.1.1 \
	rubygem-file-tail-1.0.4  \
	rubygem-net-sftp-2.0.5 \
        rubygem-net-ssh-2.0.23

all: rpm

submodule-init:
	git submodule update --init --recursive

oss_core-dist:
	cd oss_core; \
	git checkout release-2.0.1; \
	autoreconf -if; \
	rm -rf .oss_core; mkdir .oss_core ; cd .oss_core; \
	../configure --disable-dep-check --prefix=`pwd`/build; \
	make dist; \
	cp *.tar.gz ../../RPMBUILD/DIST

oss_core-rpm: oss_core-dist
	rpmbuild -ta --define "%_topdir `pwd`/RPMBUILD" `pwd`/RPMBUILD/DIST/oss_core*.tar.gz; \

submodule-dist: submodule-init oss_core-dist
	
rpm-dir:
	@rm -rf `pwd`/RPMBUILD; \
	mkdir -p `pwd`/RPMBUILD/{DIST,BUILD,SOURCES,RPMS,SRPMS,SPECS}

dist: rpm-dir submodule-dist
	for mod in ${MODULES}; do echo Preparing $${mod}.tar.gz; tar -czf `pwd`/RPMBUILD/DIST/$${mod}.tar.gz $${mod}; done 


rpm: submodule-init rpm-dir oss_core-rpm
	for mod in ${MODULES}; do \
		rm -rf `pwd`/RPMBUILD/BUILD/*; \
		rm -rf `pwd`/RPMBUILD/DIST/*; \
		tar -czf `pwd`/RPMBUILD/DIST/$${mod}.tar.gz $${mod}; \
		rpmbuild -ta --define "%_topdir `pwd`/RPMBUILD" `pwd`/RPMBUILD/DIST/$${mod}.tar.gz; \
	done

build-deps:
	@for mod in ${MODULES}; do grep '^BuildRequires' $${mod}/*.spec | awk '{print $$2}' | sed 's/,/ /g' | sed 's/ /\n/g' ; done | sort -u

docker-container:
	docker build -t="joegen/sipx-externals-builder" .; \
	docker push joegen/sipx-externals-builder


