Making Templates
================

The template consists of a few deinitiaions, a list of quiestions, some post question processing, and then building of the config file that will be stored in `/config_file`.

It is also possible to tweek the boot config file, if you do tweek the boot config file, make sure to pass `local_config` as a boot paramater, otherwise the init scripts will not read `/config_file`.

Definitions
-----------

You must define the following variables

DISK="<disk name>"

This defines what disk image to apply to, ie: `linux-installer`

Questions
---------

QUESTION_LIST=( "MIRROR_SERVER" "HOSTNAME" "SWAP_SIZE" "USE_PROXY" "PROXY" "IS_VM" "RAID_TYPE"  )
DESC_LIST=( "Mirror Server" "Hostname" "Swap Size" "Use HTTP Proxy" "HTTP Proxy (ignored if use proxy is n)" "Is VM" "RAID Type (ignored if VM)" )
TYPE_LIST=( "textbox", "textbox", "textbox", "yesno", "textbox", "yesno", "menu:single_Single Disk_mirror_Mirror Two Disks_raid10_All disks into RAID 10_raid6_All disks into RAID 6")
DEFAULT_LIST=( "us.archive.ubuntu.com" "xenial" "2048" "y" "http://proxy:3128/" "n" "single"  )

Environment Vars

BDT_

Post Question Processing
------------------------

post_process



Output
------

CONFIG='DISTRO="ubuntu"



EXTLINUX='PROMPT 1


EXTMENU='PROMPT 1
