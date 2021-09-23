all:

clean:

install:
	mkdir -p $(DESTDIR)/lib/python3.6/contractor
	cp -a contractor/* $(DESTDIR)/lib/python3.6/contractor
	# cp postMessage $(DESTDIR)/bin
	# cp signalAlert $(DESTDIR)/bin
	# cp signalComplete $(DESTDIR)/bin
	cp setEnv $(DESTDIR)/bin

test-requires:
	echo flake8

test:

lint:
	flake8 --ignore=E501,E201,E202,E111,E126,E114,E402 --statistics .

.PHONY: all clean install test-requires test lint
