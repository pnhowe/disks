SAA_FILE_NAME = saa_1.4.0_Linux_x86_64_20251022.tar.gz

all: build/saa

build:
	mkdir build

build/saa.tar.gz:
	wget https://www.supermicro.com/Bios/sw_download/1007/$(SAA_FILE_NAME) -O build/saa.tar.gz
	touch build/saa.tar.gz

build/saa: build/saa.tar.gz
	tar -xzf build/saa.tar.gz -C build --strip-components=1

install: build/saa
	mkdir -p $(DESTDIR)/bin
	cp build/saa $(DESTDIR)/bin/saa.real
	cp saa $(DESTDIR)/bin
	cp -a build/acpica_bin $(DESTDIR)/bin

clean:
	$(RM) -r build

.PHONY: all clean
