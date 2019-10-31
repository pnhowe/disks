all:

clean:

install:
	mkdir -p $(DESTDIR)/libconfig
	cp __init__.py $(DESTDIR)/libconfig
	cp libconfig.py $(DESTDIR)/libconfig
	cp -a jinja2 $(DESTDIR)/libconfig
	cp -a markupsafe $(DESTDIR)/libconfig
	cp -a providers $(DESTDIR)/libconfig

test-requires:

test:

lint:

.PHONY: all clean install test-requires test lint
