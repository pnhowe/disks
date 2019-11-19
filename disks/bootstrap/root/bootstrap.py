#!/usr/bin/env python3
import sys
import time
import glob
import os
import lib
from contractor.client import getClient
from libdrive.libdrive import DriveManager
from libhardware.libhardware import dmiInfo, pciInfo


contractor = getClient()
lib.contractor = contractor

dm = DriveManager()

foundation_type = 'Manual'
if os.path.exists( '/dev/ipmi0' ):
  foundation_type = 'IPMI'
elif os.path.exists( '/dev/mei0' ):
  foundation_type = 'AMT'  # NOTE: Intel has pulled the AMT SDK, and I can't find any other tools to get the AMT's MAC and set the ip locally, for new we skip this... keep an eye on https://software.intel.com/en-us/amt-sdk

if foundation_type == 'IPMI':
  lib.ipmicommand( 'chassis identify force', True )

foundation_locator = None

print( 'Getting LLDP Information...' )
lldp = lib.getLLDP()
primary_iface = open( '/tmp/dhcp-interface', 'r' ).read().strip()

print( 'Getting Hardware Information...' )
hardware = {}
hardware[ 'dmi' ] = dmiInfo()
hardware[ 'pci' ] = pciInfo()

hardware[ 'total_ram' ] = lib.getRAMAmmount()
hardware[ 'total_cpu_count' ] = lib.cpuLogicalCount()
hardware[ 'total_cpu_sockets' ] = lib.cpuPhysicalCount()

while not foundation_locator:
  print( 'Looking up....' )
  lookup = contractor.lookup( { 'hardware': hardware, 'lldp': lldp, 'ip_address': lib.getIpAddress( primary_iface ) } )

  if lookup[ 'matched_by'] is None:
    print( 'Waiting 30 seconds....' )
    time.sleep( 30 )

  else:
    foundation_locator = lookup[ 'locator' ]

print( '** Hello World! I am Foundation "{0}", nice to meet you! **'.format( foundation_locator ) )
print( 'Lookedup by "{0}"'.format( lookup[ 'matched_by'] ) )
contractor.postMessage( 'Foundation Lokked up as "{0}"'.format( foundation_locator ) )

config = contractor.getConfig( foundation_locator=foundation_locator )
config[ 'ipmi_lan_channel' ] = config.get( 'ipmi_lan_channel', 1 )
lib.config = config

print( 'Getting Network Information...' )
network = {}

for item in glob.glob( '/sys/class/net/eth*' ):
  network[ item.split( '/' )[ -1 ] ] = { 'mac': open( os.path.join( item, 'address' ), 'r' ).read().strip() }

if foundation_type == 'IPMI':
  network[ 'ipmi' ] = { 'mac': lib.getIPMIMAC() }

for iface in lldp:
  if iface not in network:
    continue

  network[ iface ][ 'lldp' ] = lldp[ iface ]

if primary_iface not in network:
  network[ primary_iface ] = {}

network[ primary_iface ][ 'primary' ] = True

print( 'Getting Disk Information...' )
disks = []
for drive in dm.drive_list:
  try:
    item = drive.reporting_info
  except Exception as e:
    print( 'Warning: Error getting info from "{0}"({1}), skipped...'.format( drive, e ) )
    continue

  item[ 'capacity' ] = drive.capacity
  item[ 'devpath' ] = drive.devpath
  item[ 'pcipath' ] = drive.pcipath
  item[ 'isSSD' ] = drive.isSSD
  item[ 'isVirtualDisk' ] = drive.isVirtualDisk
  disks.append( item )
  drive.setFault( False )

# print 'Getting SCSI/Disk Enclosure Information...'
# nothing yet

print( 'Reporting Hardware info to contractor...' )
error = contractor.setIdMap( foundation_locator, { 'hardware': hardware, 'network': network, 'disks': disks } )
if error != 'Good':
  contractor.postMessage( 'Hardware Error: "{0}"'.format( error ) )
  sys.exit( 20 )

contractor.postMessage( 'Hardware Profile Verified' )

iface_list = []

if foundation_type == 'IPMI' and 'ipmi_ip_address' in config:  # TODO: when the interface on the ipmi foundation get's figured out, this will change
  contractor.postMessage( 'Configuring IPMI' )

  tmp = config[ 'ipmi_ip_address' ].split( '.' )  # TODO: when the interface on the ipmi foundation get's figured out, this will change
  tmp[3] = '1'
  address = { 'address': config[ 'ipmi_ip_address' ], 'gateway': '.'.join( tmp ), 'netmask': '255.255.255.0', 'vlan': 0, 'tagged': False }

  # remove the other users first
  lib.ipmicommand( 'user disable 5' )
  # # lib.ipmicommand( 'user set name 5 {0}_'.format( ipmi_username ) )  # some ipmi's don't like you to set the username to the same as it is allready....Intel!!!
  # lib.ipmicommand( 'user set name 5 {0}'.format( ipmi_username ) )
  # lib.ipmicommand( 'user set password 5 {0}'.format( ipmi_password ) )  # make sure username and password match /plato/plato-pod/plato/lib/IPMI.py, SOL.py
  # lib.ipmicommand( 'user enable 5' )
  # lib.ipmicommand( 'user priv 5 4 {0}'.format( config[ 'ipmi_lan_channel' ] ) )  # 4 = ADMINISTRATOR

  lib.ipmicommand( 'sol set force-encryption true {0}'.format( config[ 'ipmi_lan_channel' ] ), True )  # these two stopped working on some new SM boxes, not sure why.
  lib.ipmicommand( 'sol set force-authentication true {0}'.format( config[ 'ipmi_lan_channel' ] ), True )
  lib.ipmicommand( 'sol set enabled true {0}'.format( config[ 'ipmi_lan_channel' ] ) )
  lib.ipmicommand( 'sol set privilege-level user {0}'.format( config[ 'ipmi_lan_channel' ] ), True )  # dosen't work on some SM boxes?
  lib.ipmicommand( 'sol payload enable {0} 5'.format( config[ 'ipmi_lan_channel' ] ) )

  lib.ipmicommand( 'lan set {0} arp generate off'.format( config[ 'ipmi_lan_channel' ] ), True )  # disable gratious arp, dosen't work on some Intel boxes?
  lib.ipmicommand( 'lan set {0} ipsrc static'.format( config[ 'ipmi_lan_channel' ] ) )
  lib.ipmicommand( 'lan set {0} ipaddr {1}'.format( config[ 'ipmi_lan_channel' ], address[ 'address' ] ) )
  lib.ipmicommand( 'lan set {0} netmask {1}' .format( config[ 'ipmi_lan_channel' ], address[ 'netmask' ] ) )
  if not address.get( 'gateway', None ):
    address['gateway'] = '0.0.0.0'
  lib.ipmicommand( 'lan set {0} defgw ipaddr {1}'.format( config[ 'ipmi_lan_channel' ], address[ 'gateway' ] ) )  # use the address 0.0.0.0 dosen't allways work for disabeling defgw

  try:
    if address[ 'vlan' ] and address[ 'tagged' ]:
      lib.ipmicommand( 'lan set {0} vlan id {1}'.format( config[ 'ipmi_lan_channel' ], address[ 'vlan' ] ) )
  except KeyError:
    pass

  contractor.postMessage( 'Letting IPMI settle' )
  print( 'Letting IPMI config settle...' )
  time.sleep( 30 )  # make sure the bmc has saved everything

  print( 'Resetting BMC...' )  # so the intel boards's BMCs like to go out to lunch and never come back, wich messes up power off and bios config in the subtask, have to do something here later
  lib.ipmicommand( 'bmc reset cold' )  # reset the bmc, make sure it comes up the way we need it
  print( 'Letting BMC come back up...' )
  time.sleep( 60 )  # let the bmc restart

  # bmc info can hang for ever use something like http://stackoverflow.com/questions/1191374/subprocess-with-timeout
  # to kill it and restart and try again.
  lib.ipmicommand( 'bmc info' )  # should hang untill it comes back, need a timeout for this
  lib.ipmicommand( 'bmc info' )

  lib.ipmicommand( 'sel clear', True )  # clear the eventlog, clean slate, everyone deserves it

if config.get( 'bootstrap_wipe_mbr', False ):
  contractor.postMessage( 'Clearing MBRs...' )
  for drive in dm.drive_list:
    if drive.block_path is not None:
      tmp = open( drive.block_path, 'r+b' )
      tmp.write( bytes( [ 0, 0, 0, 0, 0 ] ) )
      tmp.close()

# if config.get( 'bios_config', False ) and config.get( 'bios_config_location', False ):
#   controller.postMessage( 'Loading Initial BIOS Config' )
#   if isinstance( config[ 'bios_config' ], basestring ):
#     config_file_list = [ config[ 'bios_config' ] ]
#   else:
#     config_file_list = list( config[ 'bios_config' ] )
#
#   local_config_file = '/tmp/biosconfig'
#
#   success = False
#
#   for config_file in config_file_list:
#     url = '%s%s' % ( config[ 'bios_config_location' ], config_file )
#     print 'Retreiving BIOS config "%s" ...' % url  # disabeling proxy for now, need a http_server proxy
#     proc = Popen( [ '/bin/wget', '-Yoff', '-q', '-O', local_config_file, url ] )
#     if proc.wait() != 0:
#       controller.postMessage( 'Error getting bios config' )
#       sys.exit( 1 )
#
#     cmd = None
#
#     bios_password = config.get( 'bios_password', '' )
#
#     if baseboard in ( 'S2600JF', ):
#       cmd = [ './syscfg', '13', bios_password, local_config_file ]
#
#     elif baseboard in ( 'X9DRE-TF+/X9DR7-TF+', 'X9DRW' ):
#       cmd = [ './sum', '1_3', bios_password, local_config_file ]
#
#     elif baseboard in ( 'X10DRU-i+', ):
#       cmd = [ './sum', '1_5', bios_password, local_config_file ]
#
#     if cmd is None:
#       print 'Motherboard "%s" not supported' % baseboard
#       sys.exit( 1 )
#
#     proc = Popen( cmd )
#     if proc.wait() == 0:
#       success = True
#       break
#
#     print 'trying next file...'
#
#   if not success:
#     controller.postMessage( 'BIOS Config Failed' )
#     sys.exit( 1 )

contractor.setLocated( foundation_locator )

contractor.postMessage( 'Cleaning up' )

if foundation_type == 'IPMI':
  lib.ipmicommand( 'chassis identify 0', True )

print( 'All Done' )
sys.exit( 0 )
