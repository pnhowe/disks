all: build/mstflint/flint/mstflint

build:
	mkdir build

build/mstflint: build
	cd build ; git clone https://github.com/Mellanox/mstflint.git

build/mstflint/flint/mstflint: build/mstflint
	cd build/mstflint && ./autogen.sh && ./configure --prefix=/ --disable-inband && make -j8

install: build/mstflint/flint/mstflint
	mkdir -p $(DESTDIR)/bin
	cp build/mstflint/flint/mstflint $(DESTDIR)/bin
	cp build/mstflint/mlxconfig/mstconfig $(DESTDIR)/bin
	mkdir -p $(DESTDIR)/lib
	cp /lib/x86_64-linux-gnu/libstdc++.so.6 $(DESTDIR)/lib
	cp /lib/x86_64-linux-gnu/libgcc_s.so.1 $(DESTDIR)/lib
#make install -C build/mstflint

clean:
	$(RM) -r build

.PHONY: all clean
