SUBDIRS = ttytee libdrive libhardware libconfig nscd

all:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir all || exit $$?; done

install:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir install || exit $$?; done

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean || exit $$?; done

.PHONY: all install clean
