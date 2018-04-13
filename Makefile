DEPS = $(shell ls deps)
DISKS = $(shell ls disks)
TEMPLATES = $(shell ls templates)
DEP_DOWNLOADS = $(foreach dep,$(DEPS),build.deps/$(dep).download)
DEP_BUILDS = $(foreach dep,$(DEPS),build.deps/$(dep).build)
IMAGE_ROOT = $(foreach disk,$(DISKS),build.images/$(disk).root)

# for thoes following along...
#  for each disk
#    wildcard /disks/<disk>.pxe
#    move to /images/pxe
#    replace images/pxe/_default.pxe with images/pxe/<disk>
#    replace images/pxe/<other>.pxe with imagex/pxe/<disk>-<other>
# that should give us a images/pxe/<disk> for every _default and a <disk>_<other> for every other .pxe file
PXES = $(foreach disk,$(DISKS),$(patsubst images/pxe/%.pxe,images/pxe/$(disk)_%,$(patsubst images/pxe/_default.pxe,images/pxe/$(disk),$(patsubst disks/$(disk)/%,images/pxe/%,$(wildcard disks/$(disk)/*.pxe)))))
PXE_FILES = $(foreach disk,$(DISKS),images/pxe/$(disk).vmlinuz images/pxe/$(disk).initrd)

# see PXES
IMGS = $(foreach disk,$(DISKS),$(patsubst images/img/%.img,images/img/$(disk)_%,$(patsubst images/img/_default.img,images/img/$(disk),$(patsubst disks/$(disk)/%,images/img/%,$(wildcard disks/$(disk)/*.img)))))

# see IMGS
ISOS = $(foreach disk,$(DISKS),$(patsubst images/iso/%.iso,images/iso/$(disk)_%,$(patsubst images/iso/_default.iso,images/iso/$(disk),$(patsubst disks/$(disk)/%,images/iso/%,$(wildcard disks/$(disk)/*.iso)))))


PWD = $(shell pwd)

all: $(IMAGE_ROOT) all-pxe

# included source
build-src:
	$(MAKE) -C src all
	touch build-src

clean-src:
	$(MAKE) -C src clean
	$(RM) build-src


# external dependancy targets
build.deps:
	mkdir build.deps

downloads:
	mkdir downloads

build.deps/download: build.deps downloads $(DEP_DOWNLOADS)
	touch $@

build.deps/build: build.deps/download $(DEP_BUILDS)
	touch $@

build.deps/%.download : FILE = $(shell grep -m 1 '#FILE:' $< | sed s/'#FILE: '// )
build.deps/%.download: deps/%
	if test "$$( sha1sum downloads/$(FILE) | cut -d ' ' -f 1 )" != "$(shell grep -m 1 '#HASH:' $< | sed s/'#HASH: '// )";              \
	then                                                                                                                               \
	  wget $(shell grep -m 1 '#SOURCE:' $< | sed s/'#SOURCE: '// ) -O downloads/$(FILE) --progress=bar:force:noscroll --show-progress; \
	fi
	if test "$$( sha1sum downloads/$(FILE) | cut -d ' ' -f 1 )" != "$(shell grep -m 1 '#HASH:' $< | sed s/'#HASH: '// )";             \
	then                                                                                                                              \
	  echo "Hash Missmatch";                                                                                                          \
	  false;                                                                                                                          \
	fi
	touch $@

build.deps/%.build: deps/% build.deps/%.download
	mkdir -p build.deps/$*
	scripts/build_dep $* downloads/$(shell grep -m 1 '#FILE:' $< | sed s/'#FILE: '// )
	touch $@

clean-downloads:
	$(RM) -r downloads

clean-deps:
	$(RM) -r build.deps

# global image targets

images:
	mkdir images
	mkdir images/pxe
	mkdir images/img
	mkdir images/iso

clean-images:
	$(RM) -r build.images
	$(RM) -r images

# pxe targets

all-pxe: $(PXE_FILES) $(PXES)

pxe-targets:
	@echo "Aviable PXE Targets: $(PXES)"

# build_root paramaters: $1 - target root fs dir, $2 - source dir, $3 - dep build dir
build.images/%.root: build.deps/build build-src
	mkdir -p build.images/$*
	cp -a rootfs/* build.images/$*
	cp -a disks/$*/root/* build.images/$*
	./disks/$*/build_root
	touch $@

images/pxe/%.initrd: build.images/%.root images
	cd build.images/$* && find ./ | grep -v ^./boot | cpio -H newc -o | gzip -9 > $(PWD)/$@

images/pxe/%.vmlinuz: build.images/%.root images
	cp -f build.images/$*/boot/vmlinuz $@

# the ugly sed mess...
#    <disk>_<other> -> disks/<images>/<other>.pxe
#    <disk> (ie no `_`) -> disks/<images>/_default.pxe
images/pxe/% : FILE = $(shell echo "$*" | sed -e s/'\(.*\)_\(.*\)'/'disks\/\1\/\2.pxe'/ -e t -e s/'\(.*\)'/'disks\/\1\/_default.pxe'/)
images/pxe/%: $(FILE) $(PXE_FILES)
	cp -f $(FILE) $@

# img targets

img-targets:
	@echo "Aviable Disk Image Targets: $(IMGS)"

# for the sed see images/pxe/%
images/img/% : FILE = $(shell echo "$*" | sed -e s/'\(.*\)_\(.*\)'/'disks\/\1\/\2.img'/ -e t -e s/'\(.*\)'/'disks\/\1\/_default.img'/)
images/img/%: $(PXE_FILES)
	if [ -f templates/$* ];                                                                                                             \
	then                                                                                                                                \
	  mkdir -p build.images/templates/$*/ ;                                                                                             \
	  DISK=$$( scripts/build_template $* build.images/templates/$*/config build.images/templates/$*/boot.config build.images/templates/$*/boot.menu 3>&1 1>/dev/null 2>/dev/null);    \
	  sudo scripts/makeimg $@ images/pxe/$$DISK.vmlinuz images/pxe/$$DISK.initrd build.images/templates/$*/boot.config build.images/templates/$*/config build.images/templates/$*/boot.menu; \
	elif [ -f $(FILE).config_file ] && [ -f $(FILE).boot_menu ];                                                                        \
	then                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE) $(FILE).config_file $(FILE).boot_menu;                 \
	elif [ -f $(FILE).config_file ];                                                                                                    \
	then                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE) $(FILE).config_file;                                   \
	elif [ -f $(FILE).boot_menu ];                                                                                                      \
	then                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE) "" $(FILE).boot_menu;                                  \
	else                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE);                                                       \
	fi

# iso targets

iso-targets:
	@echo "Aviable Disk Image Targets: $(ISOS)"

# clean up

localclean: clean-deps clean-images clean-src

distclean: clean-deps clean-images clean-src clean-downloads

.PHONY: all all-pxe all-imgs clean-src clean-downloads clean-deps clean-images localclean distclean pxe-targets
