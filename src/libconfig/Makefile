all:

clean:

install:
	mkdir -p $(DESTDIR)/libconfig
	cp __init__.py $(DESTDIR)/libconfig
	cp libconfig.py $(DESTDIR)/libconfig
	cp -a jinja2 $(DESTDIR)/libconfig
	cp -a markupsafe $(DESTDIR)/libconfig

test-requires:
	echo flake8

test:

lint:
	flake8 --exclude=jinja2,markupsafe --ignore=E501,E201,E202,E111,E126,E114,E402 --statistics .

.PHONY: all clean install test-requires test lint
