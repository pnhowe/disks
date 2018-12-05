DISTRO := $(shell lsb_release -si | tr A-Z a-z)
DISTRO_MAJOR_VERSION := $(shell lsb_release -sr | cut -d. -f1)
DISTRO_NAME := $(shell lsb_release -sc | tr A-Z a-z)

all:
	./setup.py build

install:
	mkdir -p $(DESTDIR)/usr/sbin
	mkdir -p $(DESTDIR)/var/lib/config-curator/templates/config-curator/
	install -m 755 sbin/config-curator $(DESTDIR)/usr/sbin
	install -m 644 templates/config-curator/* $(DESTDIR)/var/lib/config-curator/templates/config-curator/

ifeq (ubuntu, $(DISTRO))
	./setup.py install --root $(DESTDIR) --install-purelib=/usr/lib/python3/dist-packages/ --prefix=/usr --no-compile -O0
else
	./setup.py install --root $(DESTDIR) --prefix=/usr --no-compile -O0
endif

ifeq (centos, $(DISTRO))
ifeq (6, $(DISTRO_MAJOR_VERSION))
	# These two files are not compatible with the python3 version in centos 6
	$(RM) $(DESTDIR)/usr/lib/python3.4/site-packages/libconfig/jinja2/asyncfilters.py
	$(RM) $(DESTDIR)/usr/lib/python3.4/site-packages/libconfig/jinja2/asyncsupport.py
endif

ifeq (7, $(DISTRO_MAJOR_VERSION))
	# These two files are not compatible with the python3 version in centos 6
	$(RM) $(DESTDIR)/usr/lib/python3.4/site-packages/libconfig/jinja2/asyncfilters.py
	$(RM) $(DESTDIR)/usr/lib/python3.4/site-packages/libconfig/jinja2/asyncsupport.py
endif
endif

ifeq (xenial, $(DISTRO_NAME))
	# These two files are not compatible with the python3 version in xenial
	$(RM) $(DESTDIR)/usr/lib/python3/dist-packages/libconfig/jinja2/asyncfilters.py
	$(RM) $(DESTDIR)/usr/lib/python3/dist-packages/libconfig/jinja2/asyncsupport.py
endif
ifeq (trusty, $(DISTRO_NAME))
	# These two files are not compatible with the python3 version in trusty
	$(RM) $(DESTDIR)/usr/lib/python3/dist-packages/libconfig/jinja2/asyncfilters.py
	$(RM) $(DESTDIR)/usr/lib/python3/dist-packages/libconfig/jinja2/asyncsupport.py
endif

clean:
	./setup.py clean
	$(RM) -fr build
	$(RM) -f dpkg
	$(RM) -f rpm
ifeq (ubuntu, $(DISTRO))
	dh_clean || true
endif

dist-clean: clean
	$(RM) -fr debian
	$(RM) -fr rpmbuild
	$(RM) -f dpkg-setup
	$(RM) -f rpm-setup

.PHONY:: all install clean dist-clean

test-requires:
	echo flake8

test:

lint:
	flake8 --ignore=E501,E201,E202,E111,E126,E114,E402,W605 --statistics .

.PHONY:: test-requires test lint

dpkg-distros:
	echo ubuntu-trusty ubuntu-xenial ubuntu-bionic

dpkg-requires:
	echo dpkg-dev debhelper python3-dev python3-setuptools

dpkg-setup:
	./debian-setup
	touch dpkg-setup

dpkg:
	dpkg-buildpackage -b -us -uc
	touch dpkg

.PHONY:: dpkg-distros dpkg-requires dpkg-file

rpm-distros:
	echo centos-6 centos-7

rpm-requires:
	echo rpm-build python34-setuptools

rpm-setup:
	./rpmbuild-setup
	touch rpm-setup

rpm:
	rpmbuild -v -bb rpmbuild/config.spec
	touch rpm

.PHONY:: rpm-distros rpm-requires rpm-file
