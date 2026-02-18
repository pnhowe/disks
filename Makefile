VERSION := 0.9.0

# other arches: arm64
ARCH = x86_64

DEPS_BUILD_FS = build/$(ARCH)
DEPS_BUILD_DIR = $(DEPS_BUILD_FS)/build
DEPS = $(foreach item,$(sort $(shell ls deps)),$(lastword $(subst _, ,$(item))))
DISKS = $(shell ls disks | grep -v Makefile)
TEMPLATES = $(shell ls templates)
DEP_DOWNLOADS = $(foreach dep,$(DEPS),build/$(dep).download)
DEP_BUILDS = $(foreach dep,$(DEPS),$(DEPS_BUILD_DIR)/$(dep).build)
IMAGE_ROOT = $(foreach disk,$(DISKS),build.images/$(disk)-$(ARCH).root)

PXES = $(foreach disk,$(DISKS),images/pxe-$(ARCH)/$(disk))
#PXE_FILES = $(foreach disk,$(DISKS),images/pxe-$(ARCH)/$(disk).vmlinuz images/pxe-$(ARCH)/$(disk).initrd)

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

all: $(IMAGE_ROOT)

version:
	echo $(VERSION)

# included source
src.build: $(shell find src -type f)
	$(MAKE) -C src all
	touch $@

clean-src:
	$(MAKE) -C src clean
	$(RM) src.build

.PHONY:: all version clean-src

# external dependancy source downloading
downloads:
	mkdir downloads

.PRECIOUS:: build/%.download
build/%.download : SOURCE = $(shell grep -m 1 '#SOURCE:' $< | sed s/'#SOURCE: '//)
build/%.download : HASH = $(shell grep -m 1 '#HASH:' $< | sed s/'#HASH: '//)
build/%.download: deps/*_% downloads
	@if test "$$( sha1sum downloads/$(shell basename $(SOURCE)) 2> /dev/null | cut -d ' ' -f 1 )" != "$(HASH)";  \
	then                                                                                                         \
	  wget $(SOURCE) -O downloads/$(shell basename $(SOURCE)) --progress=bar:force:noscroll --show-progress;     \
	fi
	@if test "$$( sha1sum downloads/$(shell basename $(SOURCE)) | cut -d ' ' -f 1 )" != "$(HASH)";               \
	then                                                                                                         \
	  echo "Hash Missmatch";                                                                                     \
	  false;                                                                                                     \
	fi
	mkdir -p build
	touch $@

clean-downloads:
	$(RM) -r downloads

.PHONY:: clean-downloads

# external dependancy building

build/host.build:
	mkdir -p build/host
# https://bugs.launchpad.net/ubuntu/+source/mawk/+bug/2052392
# --exclude=libgcrypt20,libassuan0,libgpg-error0,libksba-dev --no-resolve-deps
	fakechroot fakeroot debootstrap --variant=fakechroot noble build/host
	fakechroot fakeroot chroot build/host sed 's/ main/ main universe multiverse/' -i /etc/apt/sources.list
	fakechroot fakeroot chroot build/host apt update
# do not install package list: libgcrypt-dev libgpg-error-dev libassuan-dev libksba-dev
	fakechroot fakeroot chroot build/host apt -y install build-essential less bison flex bc gawk python3 pkg-config uuid-dev libblkid-dev libudev-dev liblzma-dev zlib1g-dev libxml2-dev libreadline-dev libsqlite3-dev libbz2-dev libelf-dev libksba-dev libnpth0-dev gperf rsync autoconf automake libtool curl libsmartcols-dev libaio-dev libinih-dev liburcu-dev liblz4-dev libffi-dev unzip mandoc libpopt-dev
	touch $@

$(DEPS_BUILD_FS)/.mount: build/host.build
	[ -d $(DEPS_BUILD_FS) ] || mkdir $(DEPS_BUILD_FS)
	[ -d $(DEPS_BUILD_FS).upper ] || mkdir $(DEPS_BUILD_FS).upper
	[ -d $(DEPS_BUILD_FS).work ] || mkdir $(DEPS_BUILD_FS).work
	mountpoint -q $(DEPS_BUILD_FS) || sudo mount -t overlay overlay -olowerdir=build/host,upperdir=$(DEPS_BUILD_FS).upper,workdir=$(DEPS_BUILD_FS).work $(DEPS_BUILD_FS)
	touch $@

$(DEPS_BUILD_FS).setup: $(DEPS_BUILD_FS)/.mount build/host.build scripts/setup_build_root scripts/setup_build_root_chroot scripts/build_dep_chroot
	scripts/setup_build_root $(abspath $(DEPS_BUILD_FS))
	touch $@

$(DEPS_BUILD_FS).build: $(DEP_BUILDS)
	touch $@

# to make sure dependancies are in order
# this will make a target for each root, depending on one before it
DEPS_1 = $(filter-out $(firstword $(DEP_BUILDS)), $(DEP_BUILDS))
DEPS_2 = $(filter-out $(lastword $(DEP_BUILDS)), $(DEP_BUILDS))
$(foreach pair, $(join $(DEPS_1),$(addprefix :,$(DEPS_2))),$(eval $(pair)))

$(DEPS_BUILD_DIR)/%.build: deps/*_% $(DEPS_BUILD_FS).setup build/%.download
	mkdir -p $(DEPS_BUILD_DIR)/$*
	cp deps/*_$* $(DEPS_BUILD_DIR)/$*.dep
	scripts/build_dep $(abspath $(DEPS_BUILD_DIR)/$*.dep) $(abspath $(DEPS_BUILD_DIR)/$*) $(abspath $(DEPS_BUILD_FS)) $(abspath downloads/$(shell basename "`grep -m 1 '#SOURCE:' $< | sed s/'#SOURCE: '//`")) $(ARCH) "-j$(JOBS)"
	touch $@

clean-deps:
	[ -d $(DEPS_BUILD_FS) ] && mountpoint -q $(DEPS_BUILD_FS) && sudo umount $(DEPS_BUILD_FS) || true
	sudo $(RM) -r $(DEPS_BUILD_FS).work
	$(RM) -r build

.PHONY:: clean-deps

# build utility targets
build-shell: $(DEPS_BUILD_FS)/.mount
	fakechroot fakeroot chroot $(abspath $(DEPS_BUILD_FS)) /bin/bash -l || true

.PHONY:: build-shell

# disk file systems

# we can't build more than one root at a time, the python installer (mabey others) do work in the $(DEPS_BUILD_FS) dir during install
# this will make a target for each root, depending on one before it
# this does have a side affect that building one will force some of the others to also build
IMAGE_ROOTS_1 = $(filter-out $(firstword $(IMAGE_ROOT)), $(IMAGE_ROOT))
IMAGE_ROOTS_2 = $(filter-out $(lastword $(IMAGE_ROOT)), $(IMAGE_ROOT))
$(foreach pair, $(join $(IMAGE_ROOTS_2),$(addprefix :,$(IMAGE_ROOTS_1))),$(eval $(pair)))

.SECONDEXPANSION:
build.images/%-$(ARCH).root: $(DEPS_BUILD_FS).build src.build scripts/build_disk disks/%/info $$(shell find disks/$$*/root -type f)
	scripts/build_disk $* $(abspath build.images/$*-$(ARCH)) $(abspath $(DEPS_BUILD_FS)) $(abspath $(DEPS_BUILD_DIR)) $(abspath src) $(ARCH)
	touch $@

clean-images:
	$(RM) -r build.images
	$(RM) -r images

.PHONY:: clean-images

# pxe targets

all-pxe: all $(PXES)

pxe-targets:
	@echo "Aviable PXE Targets: $(PXES)"

images/pxe-$(ARCH):
	mkdir -p images/pxe-$(ARCH)

.PRECIOUS:: images/pxe-$(ARCH)/%.initrd
images/pxe-$(ARCH)/%.initrd: images/pxe-$(ARCH) build.images/%-$(ARCH).root
	cd build.images/$*-$(ARCH) && find ./ | grep -v boot/vmlinuz | cpio --owner=+0:+0 -H newc -o | gzip -9 > $(PWD)/$@

# techinically depends on "build.images/%-$(ARCH)/boot/vmlinuz" but that is a part of "build.images/%-$(ARCH).root"
.PRECIOUS:: images/pxe-$(ARCH)/%.vmlinuz
images/pxe-$(ARCH)/%.vmlinuz: images/pxe-$(ARCH) build.images/%-$(ARCH).root
	cp -f build.images/$*-$(ARCH)/boot/vmlinuz $@

images/pxe-$(ARCH)/%: images/pxe-$(ARCH)/%.initrd images/pxe-$(ARCH)/%.vmlinuz
	touch $@

# img targets

img-targets:
	@echo "Aviable Disk Image Targets: $(IMGS)"

images/img-$(ARCH):
	mkdir -p images/img-$(ARCH)

# the ugly sed mess...
#    <disk>_<other> -> disks/<images>/<other>.img
#    <disk> (ie no `_`) -> disks/<images>/_default.img
images/img-$(ARCH)/%.img : FILE = $(shell echo "$*" | sed -e s/'\(.*\)_\(.*\)'/'disks\/\1\/\2.img'/ -e t -e s/'\(.*\)'/'disks\/\1\/_default.boot'/)
.SECONDEXPANSION:
images/img-$(ARCH)/%.img: images/img-$(ARCH) $$(shell scripts/img_iso_deps $(ARCH) $$*)
	if [ -f templates/$* ];                                                                                                             \
	then                                                                                                                                \
	  mkdir -p build.images/templates/$*/extras ;                                                                                       \
		DISK=$$( grep -m 1 '#DISK:' templates/$* | sed s/'#DISK: '// );                                                                   \
	  scripts/build_template $* build.images/templates/$*/config.json build.images/templates/$*/config.boot build.images/templates/$*/extras && \
	  sudo scripts/makeimg $@ images/pxe-$(ARCH)/$$DISK.vmlinuz images/pxe-$(ARCH)/$$DISK.initrd build.images/templates/$*/config.boot build.images/templates/$*/config.json build.images/templates/$*/extras; \
	elif [ -f $(FILE).config_file ];                                                                                                    \
	then                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE) $(FILE).config_file;                   \
	else                                                                                                                                \
	  sudo scripts/makeimg $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE);                                       \
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
		scripts/build_template $* build.images/templates/$*/config.json build.images/templates/$*/config.boot build.images/templates/$*/extras && \
	  scripts/makeiso $@ images/pxe-$(ARCH)/$$DISK.vmlinuz images/pxe-$(ARCH)/$$DISK.initrd build.images/templates/$*/config.boot build.images/templates/$*/config.json build.images/templates/$*/extras; \
	elif [ -f $(FILE).config_file ];                                                                                                    \
	then                                                                                                                                \
	  scripts/makeiso $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE) $(FILE).config_file;                        \
	else                                                                                                                                \
	  scripts/makeiso $@ images/pxe-$(ARCH)/$*.vmlinuz images/pxe-$(ARCH)/$*.initrd $(FILE);                                            \
	fi

# templates

templates:
	@echo "Aviable Templates: $(TEMPLATES)"

# clean up

clean: clean-deps clean-images clean-src respkg-clean pkg-clean resource-clean
	cd disks/firmware && ./clean

dist-clean: clean-deps clean-images clean-src clean-downloads respkg-clean pkg-dist-clean resource-clean

.PHONY:: all all-pxe all-imgs clean clean-src clean-downloads clean-deps clean-images dist-clean pxe-targets templates images/%

contractor/linux-installer-profiles.touch: $(shell find disks/linux-installer/profiles -type f -print)
	mkdir -p  contractor/linux-installer-profiles/var/www/static/disks
	for DISTRO in trusty xenial bionic focal jammy noble; do tar -h -czf contractor/linux-installer-profiles/var/www/static/disks/ubuntu-$$DISTRO-profile.tar.gz -C disks/linux-installer/profiles/ubuntu/$$DISTRO . ; done
	for DISTRO in buster; do tar -h -czf contractor/linux-installer-profiles/var/www/static/disks/debian-$$DISTRO-profile.tar.gz -C disks/linux-installer/profiles/debian/$$DISTRO . ; done
	for DISTRO in 6 7; do tar -h -czf contractor/linux-installer-profiles/var/www/static/disks/centos-$$DISTRO-profile.tar.gz -C disks/linux-installer/profiles/centos/$$DISTRO . ; done
	tar -h -czf contractor/linux-installer-profiles/var/www/static/disks/cloudinit-noble-profile.tar.gz -C disks/linux-installer/profiles/cloudinit/noble .
	touch contractor/linux-installer-profiles.touch

respkg-blueprints:
	echo ubuntu-focal-large

respkg-requires:
	echo respkg fakeroot bc gperf python3-dev python3-setuptools pkg-config gettext python3-pip bison flex gawk netpbm caca-utils locales
ifeq ($(ARCH),x86_64)
	echo build-essential
endif
ifeq ($(ARCH),arm64)
	echo crossbuild-essential-arm64
endif

respkg: all-pxe contractor/linux-installer-profiles.touch
	mkdir -p contractor/resources/var/www/static/pxe/disks
	cp images/pxe-$(ARCH)/*.initrd contractor/resources/var/www/static/pxe/disks
	cp images/pxe-$(ARCH)/*.vmlinuz contractor/resources/var/www/static/pxe/disks
	cd contractor && fakeroot respkg -b ../disks-contractor_$(VERSION).respkg -n disks-contractor -e $(VERSION) -c "Disks for Contractor" -t load_resources.sh -d resources -s contractor-os-base
	cd contractor && fakeroot respkg -b ../disks-linux-installer-profiles_$(VERSION).respkg -n disks-linux-installer-profiles -e $(VERSION) -c "Disks Linux Installer Profiles" -t load_linux-installer-profiles.sh -d linux-installer-profiles
	touch respkg

respkg-file:
	echo $(shell ls *.respkg)

respkg-clean:
	$(RM) -r resources/var/www/disks
	$(RM) contractor/linux-installer-profiles.touch
	$(RM) -r contractor/linux-installer-profiles

.PHONY:: respkg-blueprints respkg-requires respkg respkg-file respkg-clean

resource-blueprints:
	# echo ubuntu-focal-base

resource-requires:
	echo build-essential python3-setuptools
	# AppImage

resource:
	# make an AppImage of the linux-installer, this will run on the host os that is close to the target os
	# this should take a profile and a path, and make a cpio of the installed OS
	# do the bootstrap, config, packag install and update
	# output a profile that installs the image finishes the config and installs the bootloader
	touch resource

resource-file:
	echo $(shell ls *.appimage)

resource-clean:


.PHONY:: resource-blueprints resource-requires resource resource-file resource-clean

# MCP targets

pkg-clean:
	for dir in config-curator; do $(MAKE) -C $$dir clean || exit $$?; done

pkg-dist-clean:
	for dir in config-curator; do $(MAKE) -C $$dir dist-clean || exit $$?; done

test-blueprints:
	echo ubuntu-focal-base

test-requires:
	for dir in disks src config-curator; do $(MAKE) -C $$dir test-requires || exit $$?; done

test:
	for dir in disks src config-curator; do $(MAKE) -C $$dir test || exit $$?; done

lint:
	for dir in disks src config-curator; do $(MAKE) -C $$dir lint || exit $$?; done

.PHONY:: test-blueprints lint test-requires test

dpkg-blueprints:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg-blueprints || exit $$?; done

dpkg-requires:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg-requires || exit $$?; done

dpkg-setup:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg-setup || exit $$?; done

dpkg:
	for dir in config-curator; do $(MAKE) -C $$dir dpkg || exit $$?; done

dpkg-file:
	echo $(shell ls config-curator*.deb)

.PHONY:: dpkg-blueprints dpkg-requires dpkg-file dpkg

rpm-blueprints:
	for dir in config-curator; do $(MAKE) -C $$dir rpm-blueprints || exit $$?; done

rpm-requires:
	for dir in config-curator; do $(MAKE) -C $$dir rpm-requires || exit $$?; done

rpm-setup:
	for dir in config-curator; do $(MAKE) -C $$dir rpm-setup || exit $$?; done

rpm:
	for dir in config-curator; do $(MAKE) -C $$dir rpm || exit $$?; done

rpm-file:
	echo $(shell ls config-curator/rpmbuild/RPMS/*/config-curator-*.rpm)

.PHONY:: rpm-blueprints rpm-requires rpm-file rpm
