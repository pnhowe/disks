Bootable Disks
==============


Building
--------

WARNING: do not run `make` as root, there is a possibility of some of these targets removing key parts of your system.  Even better run in a chroot or
some other way to protect your local filesystem.  The only targets that require root is to make the disk and iso images, make will `sudo` the required
sub-commands.  It is recomended to do the `img` and `iso` commands in a VM/Container where any the block device maniplutaion can't mess up your filesystem.

curently only tested for Ubuntu 18.04

Install required packages::

  sudo apt install build-essential libelf-dev bc zlib1g-dev libssl-dev gperf libreadline-dev libsqlite3-dev libbz2-dev liblzma-dev uuid-dev libdevmapper-dev libgcrypt-dev libgpg-error-dev libassuan-dev libksba-dev libnpth0-dev python3-dev python3-setuptools pkg-config libblkid-dev gettext

External Binaries
~~~~~~~~~~~~~~~~~

There are a few binary files that can't be downloaded automatically that are needed for various things.  Each binary what
is is for and how to get it follow.  If you don't need it just skip it.  Make sure they all end up in external.  The external
directory is not cleared during `make clan` nor `make dist-clean`

SUM
...

Supermicro Update Manager, this is required for configuring the BIOS on Supermicro boards, so far this is supported X9, X10, X11
based boards.  You can get it at https://www.supermicro.com/SwDownload/UserInfo.aspx?sw=0&cat=SUM .  It does require you
to give the site some basic info.  After that you will get a page that lists a few versions of SUM.  Click "Accept EULA"
for the Linux version.  That will turn the link to "Download File per EULA acceptance", click the link again and it will
download a .tar.gz file.  Put that .tar.gz file in the `external` directory.

SysCfg
------

Save and Restore System Configuration Utility, this is required for configuring the BIOS on some Intel White box servers.
The list of supported servers and the link to download is at https://downloadcenter.intel.com/download/29452/Save-and-Restore-System-Configuration-Utility-SYSCFG-
There is a link to download a *AllOS.zip file.  Click Download, Accept the terms, and put the .zip in the `external`
directory.

AFULUX
------

AMI's BIOS configuration utility....


PXE
~~~

Build the PXE files::

  make

This will download the external dependancies and build the initrd and kernels for the PXE images

Build one PXE::

  make images/pxe/utility

replace 'utility' with the desired disk

For a list of avilable PXE targets::

  make pxe-targets

Disk images
~~~~~~~~~~~

To build disk images you will need to install::

  sudo apt install syslinux-common extlinux

Build a Disk Image::

  make images/img/utility

replace 'utility' with the desired disk

For a list of avilable Disk Images targets::

  make img-targets

ISO images
~~~~~~~~~~

To build disk images you will need to install::

  sudo apt install genisoimage syslinux-common isolinux

Build a Disk Image::

  make images/iso/utility

replace 'utility' with the desired disk

For a list of avilable Disk Images targets::

  make iso-targets

Disks
-----

 - bios-cfg -
 - bootstrap -
 - disk-test -
 - disk-wipe -
 - firmware -
 - hardware-test -
 - linux-installer -
 - provision-check -
 - set-rtc -
 - storage-config -
 - utility -

Templates
---------

Templates apply pre-built configuration to disks and output as a img or an iso.  Depending on the template it might interactivly ask questions, or pull answers from environment values.

NOTE: `dialog` is required, please install it before using any templates::

  sudo apt install dialog

for a list of aviabile Templates::

  make templates

for more about templates and how to make them see template.rst

Notes
-----

The output of the executing script is sent to `console`(see `console` boot option) as well as saved to `/tmp/output`, be warned not to `cat /tmp/output.log` while on the primary output,
you may end up in a loop.  A second shell is started on tty2, and the output of the syslog is sent to tty3.

Boot Options
------------

- console: The TTY to use, ie: `ttyS0`, the kernel will use this during boot, then the init scripts will use this as well.  If left blank it will output to `tty1`, ie: the monitor.  It May be specified multiple times ie: `console=ttyS0 console=tty1` to use both to the first serial port and the monitor, NOTE: both the monitor and serial ports will be fully interactive.
- debug: A shell is executed after network configuration and before the task is executed
- net_debug: A shell is executed before network configuration, NOTE: when exiting this shell, the powerdown command is executed, and the task is NOT executed.
- interface: Interface to use - if not it will assume `eth0`, if set to `any` it will try all the interfaces, and will take the first one that is sucessfull at pinging the `contractor_ip`
- interface_mac: MAC Address of the interface to use - if specified, it overrites the `interface` with the interface that matches the this MAC
- ip_address: Ip Address to use, if not specified it will dhcp
- netmask: If `ip_address` is specified, this netmask will be used
- gateway: If `ip_address` is specified, this gateway will be used
- dns_server: If `ip_address` is specified, this name name server will be used
- dns_search_zone: If `ip_address` is specified, this will specify the dns search zone
- hostname: Specifies the hostname to be set, and used when DHCPing.  If not specified it will be set to `mac<MAC ADDRESS>`, where `MAC ADDRESS` is the mac address of `interface` being used, if `interface` is `any` it will pick a random interface to use for the MAC Address
- ip_prompt: Will prompt interactivly for the ip information, during boot
- no_ip: All network configuration will be skipped, this also implies `no_config`
- contractor_ip: If set, it will add an entry to the /etc/hosts file for `contractor_host` to point to `contractor_ip`, useful if DNS is problematic
- contractor_proxy: The Proxy to use for making the request to `contractor_host`, if specified, the ping check to see if the interface is good will ping this host instead of `contractor_host`
- http_proxy: HTTP Proxy for everything except the request to retrieve the `config_url`
- no_config: Do not try to get a config, for some disks such as `utility` a config is not needed.  This also disables the ping test
- local_config: Config is retreived from `/media/config_file` or `/config_file` in the local filesystem, instead of via HTTP from `contractor_host`/config/config/
- media_uuid: Mounts the filesystem with the UUID of `media_uuid` to `/media`, usefull for booting from a pre-made image with resources stored on an extra filesystem
- media_label: Same as `media_uuid` excpet the filesystem is mounted by label
