#these to get over loaded by dpkg-build package, these are for compiles for other things, like the pxe-profiles
LDFLAGS =
CFLAGS = -O3 -W -Wall -W


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

libdrive_h.py: libdrive.h disk.h libdrive_h.cache
	cp libdrive_h.cache libdrive_h.py

libenclosure_h.py: libenclosure.h enclosure.h libenclosure_h.cache
	cp libenclosure_h.cache libenclosure_h.py

install: all
	mkdir -p $(DESTDIR)/usr/lib
	mkdir -p $(DESTDIR)/usr/sbin
	install -m 644 libdrive.so.$(LIB_VERSION) $(DESTDIR)/usr/lib
	install -m 644 libenclosure.so.$(LIB_VERSION) $(DESTDIR)/usr/lib
	install -m 755 selftest $(DESTDIR)/usr/sbin
	install -m 755 driveinfo $(DESTDIR)/usr/sbin
	install -m 755 enclosureinfo $(DESTDIR)/usr/sbin
	install -m 755 smartinfo $(DESTDIR)/usr/sbin
	install -m 755 diskcopy $(DESTDIR)/usr/sbin
	install -m 755 wipedrive $(DESTDIR)/usr/sbin
	install -m 755 drivelist $(DESTDIR)/usr/sbin
	install -m 755 enclosurelist $(DESTDIR)/usr/sbin
	mkdir -p $(DESTDIR)/lib/python3.6/libdrive
	cp __init__.py $(DESTDIR)/lib/python3.6/libdrive
	cp libdrive.py $(DESTDIR)/lib/python3.6/libdrive
	cp libenclosure.py $(DESTDIR)/lib/python3.6/libdrive
	cp libdrive_h.py $(DESTDIR)/lib/python3.6/libdrive
	cp libenclosure_h.py $(DESTDIR)/lib/python3.6/libdrive



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

test:

test-requires:

test-requires:
	echo flake8

lint:
	flake8 --exclude *_h.py --ignore=E501,E201,E202,E111,E126,E114,E402,W605 --statistics .

.PHONY: all clean esx-lib install-disks install-client-pkg install-esx test lint test-requires
