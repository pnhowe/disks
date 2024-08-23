SUBDIRS = ttytee nscd libconfig libdrive libhardware contractor

all:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir all || exit $$?; done

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean || exit $$?; done

test-requires:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir test-requires || exit $$?; done

test:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir test || exit $$?; done

lint:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir lint || exit $$?; done

.PHONY: all clean test-requires test lint
