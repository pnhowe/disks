VERSION := 0.6

# other arches: arm arm64
ARCH = x86_64

DEPS = $(foreach item,$(sort $(shell ls deps)),$(lastword $(subst _, ,$(item))))
DISKS = $(shell ls disks)
TEMPLATES = $(shell ls templates)
DEP_DOWNLOADS = $(foreach dep,$(DEPS),build.deps/$(dep).download)
DEP_BUILDS = $(foreach dep,$(DEPS),build.deps/$(dep)-$(ARCH).build)
IMAGE_ROOT = $(foreach disk,$(DISKS),build.images/$(disk)-$(ARCH).root)

# now that we are embracing contractor fully, we don't need the /disks/<disk>*.pxe sutff, that is all in the contractor resource, just get a list of the disks, and that is our list of PXEs
PXES = $(foreach disk,$(DISKS),images/pxe-$(ARCH)/$(disk))
PXE_FILES = $(foreach disk,$(DISKS),images/pxe-$(ARCH)/$(disk).vmlinuz images/pxe-$(ARCH)/$(disk).initrd)

# for img and iso targets, create a .boot for both, and a .img or .iso for .img and .iso respectively

# for thoes following along...
#  for each disk
#    wildcard /disks/<disk>/*.boot /disks/<disk>/*.img
#    move to /images/img-$(ARCH)
#    replace images/img-$(ARCH)/_default.boot with images/img-$(ARCH)/<disk>.img
#    replace images/img-$(ARCH)/<other>.boot with imagex/img-$(ARCH)/<disk>-<other>.img
# same thing for the .iso
IMGS = $(foreach item,$(foreach disk,$(DISKS),$(patsubst images/img-$(ARCH)/%.img,images/img-$(ARCH)/$(disk)_%,$(patsubst images/img-$(ARCH)/%.boot,images/img-$(ARCH)/$(disk)_%,$(patsubst images/img-$(ARCH)/_default.boot,images/img-$(ARCH)/$(disk),$(patsubst disks/$(disk)/%,images/img-$(ARCH)/%,$(wildcard disks/$(disk)/*.boot) $(wildcard disks/$(disk)/*.img)))))), $(item).img)
ISOS = $(foreach item,$(foreach disk,$(DISKS),$(patsubst images/iso-$(ARCH)/%.iso,images/iso-$(ARCH)/$(disk)_%,$(patsubst images/iso-$(ARCH)/%.boot,images/iso-$(ARCH)/$(disk)_%,$(patsubst images/iso-$(ARCH)/_default.boot,images/iso-$(ARCH)/$(disk),$(patsubst disks/$(disk)/%,images/iso-$(ARCH)/%,$(wildcard disks/$(disk)/*.boot) $(wildcard disks/$(disk)/*.iso)))))), $(item).iso)

PWD = $(shell pwd)

JOBS := $(or $(shell cat /proc/$$PPID/cmdline | sed -n 's/.*\(-j\|--jobs=\)[^0-9]\?\([0-9]\+\).*/\2/p'),1)

all: build.images/build-$(ARCH)

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

build.deps/build-$(ARCH): build.deps/download $(DEP_BUILDS)
	touch $@

build.deps/%.download : FILE = $(shell grep -m 1 '#FILE:' $< | sed s/'#FILE: '// )
build.deps/%.download: deps/*_%
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

# to make sure dependancies are in order
# this will make a target for each root, depending on one before it
DEPS_1 = $(filter-out $(firstword $(DEP_BUILDS)), $(DEP_BUILDS))
DEPS_2 = $(filter-out $(lastword $(DEP_BUILDS)), $(DEP_BUILDS))
$(foreach pair, $(join $(DEPS_1),$(addprefix :,$(DEPS_2))),$(eval $(pair)))

build.deps/libs-$(ARCH):
	mkdir -p build.deps/libs-$(ARCH)

build.deps/%-$(ARCH).build: deps/*_% build.deps/libs-$(ARCH) build.deps/%.download
	mkdir -p build.deps/$*-$(ARCH)
	scripts/build_dep $* build.deps/$*-$(ARCH) $(abspath build.deps/libs-$(ARCH)) downloads/$(shell grep -m 1 '#FILE:' $< | sed s/'#FILE: '// ) $(ARCH) "-j$(JOBS)"
	touch $@

clean-downloads:
	$(RM) -r downloads

clean-deps:
	$(RM) -r build.deps

# global image targets

clean-images:
	$(RM) -r build.images
	$(RM) -r images-$(ARCH)

.PHONY:: clean-downloads clean-deps clean-images

# disk file systems

all-disks: $(IMAGE_ROOT)

# we can't build more than one root at a time, the python installer (mabey others) do work in the build.deps dir during install
# this will make a target for each root, depending on one before it
IMAGE_ROOTS_1 = $(filter-out $(firstword $(IMAGE_ROOT)), $(IMAGE_ROOT))
IMAGE_ROOTS_2 = $(filter-out $(lastword $(IMAGE_ROOT)), $(IMAGE_ROOT))
$(foreach pair, $(join $(IMAGE_ROOTS_2),$(addprefix :,$(IMAGE_ROOTS_1))),$(eval $(pair)))

.SECONDEXPANSION:
build.images/%-$(ARCH).root: build.deps/build-$(ARCH) build-src.touch disks/%/info $$(shell find disks/$$*/root -type f)
	scripts/build_disk $* $(abspath build.images/$*-$(ARCH)) $(ARCH)
	touch $@

build.images/build-$(ARCH): $(IMAGE_ROOT)
	touch $@

# pxe targets

all-pxe: $(PXE_FILES)

pxe-targets:
	@echo "Aviable PXE Targets: $(PXES)"

images/pxe-$(ARCH):
	mkdir -p images/pxe-$(ARCH)

images/pxe-$(ARCH)/%.initrd: images/pxe-$(ARCH) build.images/%-$(ARCH).root
	cd build.images/$*-$(ARCH) && find ./ | grep -v boot/vmlinuz | cpio --owner=+0:+0 -H newc -o | gzip -9 > $(PWD)/$@

images/pxe-$(ARCH)/%.vmlinuz: images/pxe-$(ARCH) build.images/%-$(ARCH)/boot/vmlinuz
	cp -f build.images/$*-$(ARCH)/boot/vmlinuz $@

images/pxe-$(ARCH)/%: images/pxe-$(ARCH)/%.initrd images/pxe-$(ARCH)/%.vmlinuz

.PHONY:: $(PXES)

# img targets

img-targets:
	@echo "Aviable Disk Image Targets: $(IMGS)"

images/img-$(ARCH):
	mkdir -p images/img-$(ARCH)

# the ugly sed mess...
#    <disk>_<other> -> disks/<images>/<other>.img
#    <disk> (ie no `_`) -> disks/<images>/_default.img
images/img-$(ARCH)/%.img : FILE = $(shell echo "$*" | sed -e s/'\(.*\)_\(.*\)'/'disks\/\1\/\2.img'/ -e t -e s/'\(.*\)'/'disks\/\1\/_default.boot'/)
SECONDEXPANSION:
images/img-$(ARCH)/%.img: images/img-$(ARCH) $$(shell scripts/img_iso_deps $(ARCH) $$*)
	if [ -f templates/$* ];                                                                                                             \
	then                                                                                                                                \
	  mkdir -p build.images/templates/$*/extras ;                                                                                       \
		DISK=$$( grep -m 1 '#DISK:' templates/$* | sed s/'#DISK: '// );                                                                   \
	  scripts/build_template $* build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/config.boot build.images/templates/$*/extras && \
	  sudo scripts/makeimg $@ images/pxe-$(ARCH)/$$DISK.vmlinuz images/pxe-$(ARCH)/$$DISK.initrd build.images/templates/$*/config.boot build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/extras; \
	elif [ -f $(FILE).config_file ];                                                                                                    \
	then                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE) $(FILE).config_file;                                   \
	else                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE);                                                       \
	fi

# iso targets

iso-targets:
	@echo "Aviable ISO Targets: $(ISOS)"

images/iso-$(ARCH):
	mkdir -p images/iso-$(ARCH)

# the ugly sed mess...
#    <disk>_<other> -> disks/<images>/<other>.iso    # TODO: add the <disk>_<other> -> disks/<images>/<other>.boot case, probably easer to sub shell it like img_iso_deps
#    <disk> (ie no `_`) -> disks/<images>/_default.boot
images/iso-$(ARCH)/%.iso : FILE = $(shell echo "$*" | sed -e s/'\(.*\)_\(.*\)'/'disks\/\1\/\2.iso'/ -e t -e s/'\(.*\)'/'disks\/\1\/_default.boot'/)
.SECONDEXPANSION:
images/iso-$(ARCH)/%.iso: images/iso-$(ARCH) $$(shell scripts/img_iso_deps $(ARCH) $$*)
	if [ -f templates/$* ];                                                                                                             \
	then                                                                                                                                \
	  mkdir -p build.images/templates/$*/extras ;                                                                                       \
		DISK=$$( grep -m 1 '#DISK:' templates/$* | sed s/'#DISK: '// );                                                                   \
		scripts/build_template $* build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/config.boot build.images/templates/$*/extras && \
	  scripts/makeiso $@ images/pxe-$(ARCH)/$$DISK.vmlinuz images/pxe-$(ARCH)/$$DISK.initrd build.images/templates/$*/config.boot build.images/templates/$*/config-init build.images/templates/$*/config.json build.images/templates/$*/extras; \
	elif [ -f $(FILE).config_file ];                                                                                                    \
	then                                                                                                                                \
	  scripts/makeiso $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE) $(FILE).config_file;                                        \
	else                                                                                                                                \
	  scripts/makeiso $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE);                                                            \
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
	echo respkg fakeroot bc gperf python3-dev python3-setuptools pkg-config gettext python3-pip bison flex gawk
	# echo still need?  libelf-dev  libassuan-dev libksba-dev libnpth0-dev
ifeq ($(ARCH),"x86_64")
	echo build-essential uuid-dev libblkid-dev libudev-dev libgpg-error-dev liblzma-dev zlib1g-dev libxml2-dev libdevmapper-dev libssl-dev libreadline-dev libsqlite3-dev libbz2-dev libgcrypt-dev
endif
ifeq ($(ARCH),"arm")
	echo crossbuild-essential-armhf uuid-dev libblkid-dev libudev-dev libgpg-error-dev liblzma-dev zlib1g-dev libxml2-dev libdevmapper-dev libssl-dev libreadline-dev libsqlite3-dev libbz2-dev libgcrypt-dev
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
