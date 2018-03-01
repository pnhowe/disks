#these to get over loaded by dpkg-build package, these are for compiles for other things, like the pxe-profiles
LDFLAGS =
CFLAGS = -O3 -W -Wall -W

# so far only in precise and trusty have had h2xml, and a
# good version of gccxml.  xenial has gccxml from castxml which h2xml
# has a hard time passing `-I .` to and xml2py has a hard time parsing
# the result from... for now use a cached copy of the *_h.py file, later
# we need to find a way to generate the *_h.py
HAS_H2XML = 0
ifeq ($(wildcard /usr/bin/h2xml),/usr/bin/h2xml)
ifeq ($(shell gccxml | grep compatibility -c),0)
HAS_H2XML = 1
endif
endif

LIB_VERSION = $(shell grep "^__VERSION__" libdrive.py | cut -f2 | sed s/\[^0-9\.]//g)
PROG_VERSION = $(shell echo "$(LIB_VERSION)" | sed s/"\.[0-9]$$"//)
LIB_CURRENT_VERSION = $(shell echo "$(PROG_VERSION)" | sed s/"\.[0-9]$$"//)

LDLIBS = -lm
PROGS = wipedrive muncher selftest driveinfo drivepower diskcopy smartinfo enclosureinfo
SHARED_LIBS = libdrive.so.$(LIB_VERSION) libenclosure.so.$(LIB_VERSION)
BASE_OBJS = device.o disk.o enclosure.o cdb.o ata.o scsi.o ide.o sgio.o sat.o lsi.o

override LDFLAGS +=
override CFLAGS += -DPROG_VERSION=\"$(PROG_VERSION)\" -fno-builtin-memset

#TODO: possibly touch a file to tell if the current build is normal or ESX, if not clean and build yours
#make sure gcc-multilib libc6-dev-i386 is installed for 32 bit
#update Makefile for libhardware as needed

ifeq ($(shell uname -m),x86_64)
override CFLAGS += -fPIC
endif

DEBUG ?= 1
ifeq (DEBUG, 1)
  override CFLAGS += -g -DDEBUG
endif

PROG_OBJS = $(foreach prog,$(PROGS),$(prog).o)
LIB_OBJS = $(foreach lib,$(SHARED_LIBS),$(lib).o)
ALL_OBJS = $(BASE_OBJS) $(PROG_OBJS) $(LIB_OBJS)

all: $(PROGS) $(SHARED_LIBS) libdrive_h.py libenclosure_h.py

$(ALL_OBJS): device.h disk.h enclosure.h cdb.h ata.h scsi.h ide.h sgio.h sat.h lsi.h libdrive.py

esx-lib: override CFLAGS += -m32
esx-lib: override LDFLAGS += -m32
esx-lib: $(SHARED_LIBS)

libdrive_h.py: disk.h
ifeq ($(HAS_H2XML), 1)
	h2xml -I . disk.h -o disk.xml
	xml2py disk.xml -s drive_info -s smart_attribs > libdrive_h.py
	$(RM) disk.xml
else
	cp libdrive_h.cache libdrive_h.py
endif

libenclosure_h.py: enclosure.h
ifeq ($(HAS_H2XML), 1)
	h2xml -I . enclosure.h -o enclosure.xml
	xml2py enclosure.xml -s enclosure_info -s subenc_descriptors > libenclosure_h.py
	$(RM) enclosure.xml
else
	cp libenclosure_h.cache libenclosure_h.py
endif

# make sure to update the dependancies in plato-disks/Makefile if more/less pxe-images are affected
install-disks: all
	mkdir -p ../plato-disks/dtest/initrd/lib
	mkdir -p ../plato-disks/dtest/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/dtest/initrd/sbin
	mkdir -p ../plato-disks/dtest/initrd/bin
	cp libdrive.so.$(LIB_VERSION) ../plato-disks/dtest/initrd/lib
	cp libenclosure.so.$(LIB_VERSION) ../plato-disks/dtest/initrd/lib
	cp libdrive.py ../plato-disks/dtest/initrd/usr/lib/python2.7/platoclient
	cp libenclosure.py ../plato-disks/dtest/initrd/usr/lib/python2.7/platoclient
	cp libdrive_h.py ../plato-disks/dtest/initrd/usr/lib/python2.7/platoclient
	cp libenclosure_h.py ../plato-disks/dtest/initrd/usr/lib/python2.7/platoclient
	cp wipedrive ../plato-disks/dtest/initrd/sbin
	cp muncher ../plato-disks/dtest/initrd/sbin
	cp selftest ../plato-disks/dtest/initrd/sbin
	cp diskcopy ../plato-disks/dtest/initrd/sbin
	cp driveinfo ../plato-disks/dtest/initrd/bin
	cp enclosureinfo ../plato-disks/dtest/initrd/bin
	cp smartinfo ../plato-disks/dtest/initrd/bin
	cp drivepower ../plato-disks/dtest/initrd/bin
	cp drivelist ../plato-disks/dtest/initrd/bin
	cp enclosurelist ../plato-disks/dtest/initrd/bin

	mkdir -p ../plato-disks/diskcopy/initrd/lib
	mkdir -p ../plato-disks/diskcopy/initrd/sbin
	mkdir -p ../plato-disks/diskcopy/initrd/bin
	cp libdrive.so.$(LIB_VERSION) ../plato-disks/diskcopy/initrd/lib
	cp libdrive.py ../plato-disks/diskcopy/initrd
	cp libdrive_h.py ../plato-disks/diskcopy/initrd
	cp wipedrive ../plato-disks/diskcopy/initrd/sbin
	cp selftest ../plato-disks/diskcopy/initrd/sbin
	cp diskcopy ../plato-disks/diskcopy/initrd/sbin
	cp driveinfo ../plato-disks/diskcopy/initrd/bin
	cp smartinfo ../plato-disks/diskcopy/initrd/bin

	mkdir -p ../plato-disks/linux-installer/initrd/lib
	mkdir -p ../plato-disks/linux-installer/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/linux-installer/initrd/sbin
	mkdir -p ../plato-disks/linux-installer/initrd/bin
	cp libdrive.so.$(LIB_VERSION) ../plato-disks/linux-installer/initrd/lib
	cp libdrive.py ../plato-disks/linux-installer/initrd/usr/lib/python2.7/platoclient
	cp libdrive_h.py ../plato-disks/linux-installer/initrd/usr/lib/python2.7/platoclient
	cp drivelist ../plato-disks/linux-installer/initrd/bin
	cp wipedrive ../plato-disks/linux-installer/initrd/sbin

	mkdir -p ../plato-disks/utility/initrd/lib
	mkdir -p ../plato-disks/utility/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/utility/initrd/sbin
	mkdir -p ../plato-disks/utility/initrd/bin
	cp libdrive.so.$(LIB_VERSION) ../plato-disks/utility/initrd/lib
	cp libenclosure.so.$(LIB_VERSION) ../plato-disks/utility/initrd/lib
	cp libdrive.py ../plato-disks/utility/initrd/usr/lib/python2.7/platoclient
	cp libenclosure.py ../plato-disks/utility/initrd/usr/lib/python2.7/platoclient
	cp libdrive_h.py ../plato-disks/utility/initrd/usr/lib/python2.7/platoclient
	cp libenclosure_h.py ../plato-disks/utility/initrd/usr/lib/python2.7/platoclient
	cp wipedrive ../plato-disks/utility/initrd/sbin
	cp selftest ../plato-disks/utility/initrd/sbin
	cp diskcopy ../plato-disks/utility/initrd/sbin
	cp driveinfo ../plato-disks/utility/initrd/bin
	cp enclosureinfo ../plato-disks/utility/initrd/bin
	cp smartinfo ../plato-disks/utility/initrd/bin
	cp drivelist ../plato-disks/utility/initrd/bin
	cp enclosurelist ../plato-disks/utility/initrd/bin

	mkdir -p ../plato-disks/bootstrap/initrd/lib
	mkdir -p ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/bootstrap/initrd/bin
	cp libdrive.so.$(LIB_VERSION) ../plato-disks/bootstrap/initrd/lib
	cp libenclosure.so.$(LIB_VERSION) ../plato-disks/bootstrap/initrd/lib
	cp libdrive.py ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	cp libenclosure.py ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	cp libdrive_h.py ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	cp libenclosure_h.py ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	cp driveinfo ../plato-disks/bootstrap/initrd/bin
	cp enclosureinfo ../plato-disks/bootstrap/initrd/bin
	cp drivelist ../plato-disks/bootstrap/initrd/bin
	cp enclosurelist ../plato-disks/bootstrap/initrd/bin

	mkdir -p ../plato-disks/firmware-update/initrd/lib
	mkdir -p ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/firmware-update/initrd/sbin
	mkdir -p ../plato-disks/firmware-update/initrd/bin
	cp libdrive.so.$(LIB_VERSION) ../plato-disks/firmware-update/initrd/lib
	cp libenclosure.so.$(LIB_VERSION) ../plato-disks/firmware-update/initrd/lib
	cp libdrive.py ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	cp libenclosure.py ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	cp libdrive_h.py ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	cp libenclosure_h.py ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	cp driveinfo ../plato-disks/firmware-update/initrd/sbin
	cp enclosureinfo ../plato-disks/firmware-update/initrd/sbin
	cp drivelist ../plato-disks/firmware-update/initrd/bin
	cp enclosurelist ../plato-disks/firmware-update/initrd/bin

	mkdir -p ../plato-disks/provision-check/initrd/lib
	mkdir -p ../plato-disks/provision-check/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/provision-check/initrd/sbin
	mkdir -p ../plato-disks/provision-check/initrd/bin
	cp libdrive.so.$(LIB_VERSION) ../plato-disks/provision-check/initrd/lib
	cp libenclosure.so.$(LIB_VERSION) ../plato-disks/provision-check/initrd/lib
	cp libdrive.py ../plato-disks/provision-check/initrd/usr/lib/python2.7/platoclient
	cp libenclosure.py ../plato-disks/provision-check/initrd/usr/lib/python2.7/platoclient
	cp libdrive_h.py ../plato-disks/provision-check/initrd/usr/lib/python2.7/platoclient
	cp libenclosure_h.py ../plato-disks/provision-check/initrd/usr/lib/python2.7/platoclient
	cp driveinfo ../plato-disks/provision-check/initrd/sbin
	cp enclosureinfo ../plato-disks/provision-check/initrd/sbin
	cp drivelist ../plato-disks/provision-check/initrd/bin
	cp enclosurelist ../plato-disks/provision-check/initrd/bin

	mkdir -p ../plato-disks/dwipe/initrd/sbin
	mkdir -p ../plato-disks/dwipe/initrd/bin
	cp wipedrive ../plato-disks/dwipe/initrd/sbin
	cp driveinfo ../plato-disks/dwipe/initrd/bin
	cp drivelist ../plato-disks/dwipe/initrd/bin

install-client-pkg: all
	mkdir -p $(DESTDIR)usr/lib
	mkdir -p $(DESTDIR)usr/sbin
	install -m 644 libdrive.so.$(LIB_VERSION) $(DESTDIR)usr/lib
	install -m 644 libenclosure.so.$(LIB_VERSION) $(DESTDIR)usr/lib
	install -m 755 selftest $(DESTDIR)usr/sbin
	install -m 755 driveinfo $(DESTDIR)usr/sbin
	install -m 755 enclosureinfo $(DESTDIR)usr/sbin
	install -m 755 smartinfo $(DESTDIR)usr/sbin
	install -m 755 diskcopy $(DESTDIR)usr/sbin
	install -m 755 wipedrive $(DESTDIR)usr/sbin
	install -m 755 drivelist $(DESTDIR)usr/sbin
	install -m 755 enclosurelist $(DESTDIR)usr/sbin

# have to compile twice b/c the lib is 32-bit (esx's python is 32bit)
install-esx:
	$(MAKE) clean
	$(MAKE) esx-lib
	cp libdrive.so.$(LIB_VERSION) ../plato-esx/lib/libdrive.so.$(LIB_CURRENT_VERSION)
	$(MAKE) clean
	$(MAKE) driveinfo smartinfo libdrive_h.py
	cp libdrive.py ../plato-esx/lib/python2.6/site-packages/platoclient
	cp libdrive_h.py ../plato-esx/lib/python2.6/site-packages/platoclient
	cp driveinfo ../plato-esx/sbin
	cp smartinfo ../plato-esx/sbin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.so.$(LIB_VERSION): %.o $(BASE_OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(patsubst %.so.$(LIB_VERSION),%.so.$(LIB_CURRENT_VERSION),$@) -o $@ $< $(BASE_OBJS) $(LDLIBS)

%: %.o $(BASE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(BASE_OBJS) $(LDLIBS)

clean:
	$(RM) libdrive_h.py
	$(RM) libenclosure_h.py
	$(RM) *.o
	$(RM) *.pyc
	$(RM) $(SHARED_LIBS)
	$(RM) $(PROGS)
	$(RM) disk.xml
	$(RM) enclosure.xml

full-clean: clean

test-requires:

test:

lint-requires:
ifeq (trusty, $(DISTRO_NAME))
	echo linter
endif

lint:
ifeq (trusty, $(DISTRO_NAME))
	linter -i libdrive_h.py -i libenclosure_h.py
endif

.PHONY: all clean esx-lib install-disks install-client-pkg install-esx test lint test-requires lint-requires
