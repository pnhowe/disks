all:

test-requires:
	echo flake8

test:

lint:
	flake8 --exclude=disk-test,disk-wipe,firmware,hardware-test,storage-config,bios-cfg --ignore=E501,E201,E202,E111,E126,E114,E402 --statistics .

.PHONY: all test-requires test lint
