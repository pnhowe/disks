#these to get over loaded by dpkg-build pacage, these are for compiles for other things, like the pxe-profiles
LDFLAGS =
CFLAGS = -O3 -W -Wall -W

#see libdrive's Makefile for explanation for what is going on here
HAS_H2XML = 0
ifeq ($(wildcard /usr/bin/h2xml),/usr/bin/h2xml)
ifeq ($(shell gccxml | grep compatibility -c),0)
HAS_H2XML = 1
endif
endif

LIB_VERSION = $(shell grep "^__VERSION__" libhardware.py | cut -f2 | sed s/\[^0-9\.]//g)
PROG_VERSION = $(shell echo "$(LIB_VERSION)" | sed s/"\.[0-9]$$"//)
LIB_CURRENT_VERSION = $(shell echo "$(PROG_VERSION)" | sed s/"\.[0-9]$$"//)

LDLIBS = -lm
PROGS = hardwareinfo
SHARED_LIBS = libhardware.so.$(LIB_VERSION)
BASE_OBJS = dmi.o dmi_decode.o pci.o

override LDFLAGS +=
override CFLAGS += -DPROG_VERSION=\"$(PROG_VERSION)\"

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

all: $(PROGS) $(SHARED_LIBS) libhardware_h.py

$(ALL_OBJS): dmi.h dmi_decode.h pci.h libhardware.py

esx-lib: override CFLAGS += -m32
esx-lib: override LDFLAGS += -m32
esx-lib: $(SHARED_LIBS)

libhardware_h.py: dmi.h pci.h
ifeq ($(HAS_H2XML), 1)
	h2xml -I . dmi.h pci.h -o libhardware.xml
	xml2py libhardware.xml -s dmi_entry -s pci_entry > libhardware_h.py
	$(RM) libhardware.xml
else
	cp libhardware_h.cache libhardware_h.py
endif

# make sure to update the dependancies in plato-disks/Makefile if more/less pxe-images are affected
install-disks: all
	mkdir -p ../plato-disks/bootstrap/initrd/lib
	mkdir -p ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/bootstrap/initrd/bin
	cp libhardware.so.$(LIB_VERSION) ../plato-disks/bootstrap/initrd/lib
	cp libhardware.py ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	cp libhardware_h.py ../plato-disks/bootstrap/initrd/usr/lib/python2.7/platoclient
	cp hardwareinfo ../plato-disks/bootstrap/initrd/bin

	mkdir -p ../plato-disks/firmware-update/initrd/lib
	mkdir -p ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/firmware-update/initrd/bin
	cp libhardware.so.$(LIB_VERSION) ../plato-disks/firmware-update/initrd/lib
	cp libhardware.py ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	cp libhardware_h.py ../plato-disks/firmware-update/initrd/usr/lib/python2.7/platoclient
	cp hardwareinfo ../plato-disks/firmware-update/initrd/bin

	mkdir -p ../plato-disks/utility/initrd/bin
	cp hardwareinfo ../plato-disks/utility/initrd/bin
	cp libhardware.so.$(LIB_VERSION) ../plato-disks/utility/initrd/lib
	cp libhardware.py ../plato-disks/utility/initrd/usr/lib/python2.7/platoclient
	cp libhardware_h.py ../plato-disks/utility/initrd/usr/lib/python2.7/platoclient
	cp hardwareinfo ../plato-disks/utility/initrd/bin

	mkdir -p ../plato-disks/bios-cfg/initrd/lib
	mkdir -p ../plato-disks/bios-cfg/initrd/usr/lib/python2.7/platoclient
	mkdir -p ../plato-disks/bios-cfg/initrd/bin
	cp libhardware.so.$(LIB_VERSION) ../plato-disks/bios-cfg/initrd/lib
	cp libhardware.py ../plato-disks/bios-cfg/initrd/usr/lib/python2.7/platoclient
	cp libhardware_h.py ../plato-disks/bios-cfg/initrd/usr/lib/python2.7/platoclient
	cp hardwareinfo ../plato-disks/bios-cfg/initrd/bin

install-client-pkg: all
	mkdir -p $(DESTDIR)usr/lib
	mkdir -p $(DESTDIR)usr/sbin
	install -m 644 libhardware.so.$(LIB_VERSION) $(DESTDIR)usr/lib
	install -m 755 hardwareinfo $(DESTDIR)usr/sbin

# have to compile twice b/c the lib is 32-bit (esx's python is 32bit)
install-esx:
	$(MAKE) clean
	$(MAKE) esx-lib
	cp libhardware.so.$(LIB_VERSION) ../plato-esx/lib/libhardware.so.$(LIB_CURRENT_VERSION)
	$(MAKE) clean
	$(MAKE) hardwareinfo libhardware_h.py
	cp libhardware.py ../plato-esx/lib/python2.6/site-packages/platoclient
	cp libhardware_h.py ../plato-esx/lib/python2.6/site-packages/platoclient
	cp hardwareinfo ../plato-esx/sbin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.so.$(LIB_VERSION): %.o $(BASE_OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(patsubst %.so.$(LIB_VERSION),%.so.$(LIB_CURRENT_VERSION),$@) -o $@ $< $(BASE_OBJS) $(LDLIBS)

%: %.o $(BASE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(BASE_OBJS) $(LDLIBS)

clean:
	$(RM) libhardware_h.py
	$(RM) *.o
	$(RM) $(SHARED_LIBS)
	$(RM) $(PROGS)
	$(RM) libhardware.xml

full-clean: clean

test-requires:
	echo flake8

test:

lint:
	flake8 --exclude *_h.py --ignore=E501,E201,E202,E111,E126,E114,E402,W605 --statistics .

.PHONY: all clean esx-lib install-disks install-client-pkg install-esx test-requires test lint
