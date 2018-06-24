SUBDIRS = ttytee nscd libconfig libdrive libhardware platoclient controller

all:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir all || exit $$?; done

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean || exit $$?; done

.PHONY: all clean
