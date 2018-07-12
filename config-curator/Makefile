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

	./setup.py install --root $(DESTDIR) --install-purelib=/usr/lib/python3/dist-packages/ --prefix=/usr --no-compile -O0
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

dpkg:
	dpkg-buildpackage -b -us -uc
	touch dpkg

.PHONY: all install clean
