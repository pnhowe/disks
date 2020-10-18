VERSION := 0.6

# other arches: arm arm64
ARCH = x86_64

DEPS = $(shell ls deps)
DISKS = $(shell ls disks)
TEMPLATES = $(shell ls templates)
DEP_DOWNLOADS = $(foreach dep,$(DEPS),build.deps/$(dep).download)
DEP_BUILDS = $(foreach dep,$(DEPS),build.deps/$(dep)-$(ARCH).build)
IMAGE_ROOT = $(foreach disk,$(DISKS),build.images/$(disk).root)

# now that we are embracing contractor fully, we don't need all the _default.pxe/*.pxe sutff, that is all in the contractor resource, just get a list of the disks, and that is our list of PXEs
PXES = $(foreach disk,$(DISKS),build.images/pxe/$(disk))
PXE_FILES = $(foreach disk,$(DISKS),images/pxe/$(disk).vmlinuz images/pxe/$(disk).initrd)

# for thoes following along...
#  for each disk
#    wildcard /disks/<disk>.pxe
#    move to /images/img
#    replace images/img/_default.boot with images/img/<disk>.img
#    replace images/img/<other>.boot with imagex/img/<disk>-<other>.img
# same thing for the .iso
IMGS = $(foreach item,$(foreach disk,$(DISKS),$(patsubst images/img/%.img,images/img/$(disk)_%,$(patsubst images/img/_default.boot,images/img/$(disk),$(patsubst disks/$(disk)/%,images/img/%,$(wildcard disks/$(disk)/*.img))))), $(item).img)
ISOS = $(foreach item,$(foreach disk,$(DISKS),$(patsubst images/iso/%.iso,images/iso/$(disk)_%,$(patsubst images/iso/_default.boot,images/iso/$(disk),$(patsubst disks/$(disk)/%,images/iso/%,$(wildcard disks/$(disk)/*.iso))))), $(item).iso)

PWD = $(shell pwd)

all: build.images/build

version:
	echo $(VERSION)

# included source
build-src.touch: $(shell find src -type f)
	$(MAKE) -C src all
	touch build-src.touch

clean-src:
	$(MAKE) -C src clean
	$(RM) build-src.touch

.PHONY:: all version clean-src

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
	@if test "$$( sha1sum downloads/$(FILE) 2> /dev/null | cut -d ' ' -f 1 )" != "$(shell grep -m 1 '#HASH:' $< | sed s/'#HASH: '// )";  \
	then                                                                                                                                 \
	  wget $(shell grep -m 1 '#SOURCE:' $< | sed s/'#SOURCE: '// ) -O downloads/$(FILE) --progress=bar:force:noscroll --show-progress;   \
	fi
	@if test "$$( sha1sum downloads/$(FILE) | cut -d ' ' -f 1 )" != "$(shell grep -m 1 '#HASH:' $< | sed s/'#HASH: '// )";     \
	then                                                                                                                       \
	  echo "Hash Missmatch";                                                                                                   \
	  false;                                                                                                                   \
	fi
	touch $@

build.deps/%-$(ARCH).build: deps/% build.deps/%.download
	mkdir -p build.deps/$*-$(ARCH)
	scripts/build_dep $* build.deps/$*-$(ARCH) downloads/$(shell grep -m 1 '#FILE:' $< | sed s/'#FILE: '// ) $(ARCH)
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

.PHONY:: clean-downloads clean-deps clean-images

# pxe targets

all-pxe: $(PXE_FILES)

pxe-targets:
	@echo "Aviable PXE Targets: $(PXES)"

.SECONDEXPANSION:
build.images/%.root: build.deps/build build-src.touch disks/%/build_root $$(shell find disks/$$*/root -type f)
	mkdir -p build.images/$*
	cp -a rootfs/* build.images/$*
	cp -a disks/$*/root/* build.images/$*
	./disks/$*/build_root
	touch $@

build.images/build: $(IMAGE_ROOT)
	touch $@

# we can't build more than one root at a time, the python installer (mabey others) do work in the build.deps dir during install
# this will make a target for each root, depending on one before it
ROOTS_1 = $(filter-out $(firstword $(IMAGE_ROOT)), $(IMAGE_ROOT))
ROOTS_2 = $(filter-out $(lastword $(IMAGE_ROOT)), $(IMAGE_ROOT))
$(foreach pair, $(join $(ROOTS_2),$(addprefix :,$(ROOTS_1))),$(eval $(pair)))

images/pxe:
	mkdir -p images/pxe

images/pxe/%.initrd: images/pxe build.images/%.root
	cd build.images/$* && find ./ | grep -v boot/vmlinuz | cpio --owner=+0:+0 -H newc -o | gzip -9 > $(PWD)/$@

images/pxe/%.vmlinuz: images/pxe build.images/%/boot/vmlinuz
	cp -f build.images/$*/boot/vmlinuz $@

images/pxe/%: images/pxe/%.initrd images/pxe/%.vmlinuz

.PHONY:: $(PXES)

# img targets

img-targets:
	@echo "Aviable Disk Image Targets: $(IMGS)"

images/img:
	mkdir -p images/img

# the ugly sed mess...
#    <disk>_<other> -> disks/<images>/<other>.img
#    <disk> (ie no `_`) -> disks/<images>/_default.img
images/img/%.img : FILE = $(shell echo "$*" | sed -e s/'\(.*\)_\(.*\)'/'disks\/\1\/\2.img'/ -e t -e s/'\(.*\)'/'disks\/\1\/_default.boot'/)
SECONDEXPANSION:
images/img/%.img: images/img $$(shell scripts/img_iso_deps $$*)
	if [ -f templates/$* ];                                                                                                             \
	then                                                                                                                                \
	  mkdir -p build.images/templates/$*/extras ;                                                                                       \
		DISK=$$( grep -m 1 '#DISK:' templates/$* | sed s/'#DISK: '// );                                                                   \
	  scripts/build_template $* build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/config.boot build.images/templates/$*/extras && \
	  sudo scripts/makeimg $@ images/pxe/$$DISK.vmlinuz images/pxe/$$DISK.initrd build.images/templates/$*/config.boot build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/extras; \
	elif [ -f $(FILE).config_file ];                                                                                                    \
	then                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE) $(FILE).config_file;                                   \
	else                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE);                                                       \
	fi

# iso targets

iso-targets:
	@echo "Aviable ISO Targets: $(ISOS)"

images/iso:
	mkdir -p images/iso

# the ugly sed mess...
#    <disk>_<other> -> disks/<images>/<other>.iso
#    <disk> (ie no `_`) -> disks/<images>/_default.iso
images/iso/%.iso : FILE = $(shell echo "$*" | sed -e s/'\(.*\)_\(.*\)'/'disks\/\1\/\2.img'/ -e t -e s/'\(.*\)'/'disks\/\1\/_default.boot'/)
.SECONDEXPANSION:
images/iso/%.iso: images/iso $$(shell scripts/img_iso_deps $$*)
	if [ -f templates/$* ];                                                                                                             \
	then                                                                                                                                \
	  mkdir -p build.images/templates/$*/extras ;                                                                                       \
		DISK=$$( grep -m 1 '#DISK:' templates/$* | sed s/'#DISK: '// );                                                                   \
		scripts/build_template $* build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/config.boot build.images/templates/$*/extras && \
	  scripts/makeiso $@ images/pxe/$$DISK.vmlinuz images/pxe/$$DISK.initrd build.images/templates/$*/config.boot build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/extras; \
	elif [ -f $(FILE).config_file ];                                                                                                    \
	then                                                                                                                                \
	  scripts/makeiso $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE) $(FILE).config_file;                                        \
	else                                                                                                                                \
	  scripts/makeiso $@ images/pxe/$*.vmlinuz images/pxe/$*.initrd $(FILE);                                                            \
	fi

# templates

templates:
	@echo "Aviable Templates: $(TEMPLATES)"

# clean up

clean: clean-deps clean-images clean-src respkg-clean pkg-clean resource-clean

dist-clean: clean-deps clean-images clean-src clean-downloads pkg-dist-clean

.PHONY:: all all-pxe all-imgs clean clean-src clean-downloads clean-deps clean-images dist-clean pxe-targets templates images/img/% images/iso/%

contractor/linux-installer-profiles.touch: $(shell find disks/linux-installer/profiles -type f -print)
	mkdir -p  contractor/linux-installer-profiles/var/www/static/disks
	for DISTRO in trusty xenial bionic focal; do tar -h -czf contractor/linux-installer-profiles/var/www/static/disks/ubuntu-$$DISTRO-profile.tar.gz -C disks/linux-installer/profiles/ubuntu/$$DISTRO . ; done
	for DISTRO in buster; do tar -h -czf contractor/linux-installer-profiles/var/www/static/disks/debian-$$DISTRO-profile.tar.gz -C disks/linux-installer/profiles/debian/$$DISTRO . ; done
	for DISTRO in 6 7; do tar -h -czf contractor/linux-installer-profiles/var/www/static/disks/centos-$$DISTRO-profile.tar.gz -C disks/linux-installer/profiles/centos/$$DISTRO . ; done
	touch contractor/linux-installer-profiles.touch

respkg-distros:
	echo ubuntu-bionic

#  sudo dpkg --add-architecture arm64 armhf
# deb http://ports.ubuntu.com/ubuntu-ports bionic main universe multiverse
# qemu-user-static
respkg-requires:
	echo respkg fake-root bc gperf python3-dev python3-setuptools pkg-config libblkid-dev gettext python3-pip bison flex
ifeq ($(ARCH),"x86_64")
libelf-dev libreadline-dev libsqlite3-dev libbz2-dev libgcrypt-dev libassuan-dev libksba-dev libnpth0-dev

	echo build-essential uuid-dev libblkid-dev libudev-dev libgpg-error-dev liblzma-dev zlib1g-dev libxml2-dev libdevmapper-dev libssl-dev
endif
ifeq ($(ARCH),"arm")
	echo crossbuild-essential-armhf uuid-dev libblkid-dev libudev-dev libgpg-error-dev liblzma-dev zlib1g-dev libxml2-dev libdevmapper-dev libssl-dev
endif
ifeq ($(ARCH),"arm64")
	echo crossbuild-essential-arm64
endif

respkg: all-pxe contractor/linux-installer-profiles.touch
	mkdir -p contractor/resources/var/www/static/pxe/disks
	cp images/pxe/*.initrd contractor/resources/var/www/static/pxe/disks
	cp images/pxe/*.vmlinuz contractor/resources/var/www/static/pxe/disks
	cd contractor && fakeroot respkg -b ../disks-contractor_$(VERSION).respkg -n disks-contractor -e $(VERSION) -c "Disks for Contractor" -t load_resources.sh -d resources -s contractor-os-base
	cd contractor && fakeroot respkg -b ../disks-linux-installer-profiles_$(VERSION).respkg -n disks-linux-installer-profiles -e $(VERSION) -c "Disks Linux Installer Profiles" -t load_linux-installer-profiles.sh -d linux-installer-profiles
	touch respkg

respkg-file:
	echo $(shell ls *.respkg)

respkg-clean:
	$(RM) linux-installer-profiles.touch
	$(RM) -fr resources/var/www/disks
	$(RM) -fr contractor/linux-installer-profiles

.PHONY:: respkg-distros respkg-requires respkg respkg-file respkg-clean

resource-distros:
	echo ubuntu-bionic

resource-requires:
	echo build-essential AppImage

resource:
	# make an AppImage of the linux-installer, this will run on the host os that is close to the target os
  # this should take a profile and a path, and make a cpio of the installed OS
	# do the bootstrap, config, packag install and update
	# output a profile that installs the image finishes the config and installs the bootloader
	touch resource

resource-file:
	echo $(shell ls *.appimage)

resource-clean:


.PHONY:: resource-distros resource-requires resource resource-file resource-clean

# MCP targets

pkg-clean:
	for dir in config-curator; do $(MAKE) -C $$dir clean || exit $$?; done

pkg-dist-clean:
	for dir in config-curator; do $(MAKE) -C $$dir dist-clean || exit $$?; done

test-distros:
	echo ubuntu-xenial

test-requires:
	for dir in src config-curator; do $(MAKE) -C $$dir test-requires || exit $$?; done

test:
	for dir in src config-curator; do $(MAKE) -C $$dir test || exit $$?; done

lint:
	for dir in src config-curator; do $(MAKE) -C $$dir lint || exit $$?; done

.PHONY:: test-distros lint test-requires test

dpkg-distros:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg-distros || exit $$?; done

dpkg-requires:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg-requires || exit $$?; done

dpkg-setup:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg-setup || exit $$?; done

dpkg:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg || exit $$?; done

dpkg-file:
	echo $(shell ls config-curator*.deb)

.PHONY:: dpkg-distros dpkg-requires dpkg-file dpkg

rpm-distros:
	for dir in config-curator; do $(MAKE) -C $$dir rpm-distros || exit $$?; done

rpm-requires:
	for dir in config-curator; do $(MAKE) -C $$dir rpm-requires || exit $$?; done

rpm-setup:
	for dir in config-curator; do $(MAKE) -C $$dir rpm-setup || exit $$?; done

rpm:
	for dir in config-curator; do $(MAKE) -C $$dir rpm || exit $$?; done

rpm-file:
	echo $(shell ls config-curator/rpmbuild/RPMS/*/config-curator-*.rpm)

.PHONY:: rpm-distros rpm-requires rpm-file rpm
