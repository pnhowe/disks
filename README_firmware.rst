Firmware
========

By default only the harddrive handler is included, run "buildHandlers" in the disk/firmware folder to generate a export with the other desired handlers

The configuration comes from a map under the name 'firmware'.  This map is keyed by the name of the handler.  Each handler will have a map of the filename -> from-to verion mapping plus a desired target version
the from version can be a regex, and must match the curent version (the ^$ is implied, so you do not have to include it)

the update works by retreiving the curent version of the target hardware, if it does not match the target, it will build a list of filenames from the curent version to the target version.


example.....

[ firmware.harddrive ]
[ firmware.mellanox.MT_0000000436 ]
'fw-ConnectX7-rel-28_46_3048-MCX75310AAS-NEA_Ax-UEFI-14.39.14-FlexBoot-3.8.100.signed.bin' = { from = '.*', to = '28.46.3048' }
[ firmware_targets.mellanox ]
MT_0000000436 = '22.41.1000'
MT_0000000838 = '28.46.3048'


Set "firmware_resource_location" as the host and root path to where the firmware files are stored.  Also set "firmware_proxy" if a proxy is required to get to them.
set the value "firmware_handler_resources" to a list of paths relitive to "firmware_resource_location" to download handlers in .tar.gz


tty4 has the console output from the handler commands

Handler Priorities
------------------
20s - BIOS
40s - BMC/IPMI
60s - HBA/RAID
80s - NIC/Infiniband
100s - Harddrives/SSD/NVME
120s - GPUs/CoProcessors
