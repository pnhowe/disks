#!/usr/bin/env python3

import sys
import time
import glob
import os
import hashlib
import lib
import random
from contractor.client import getClient
from libdrive.libdrive import DriveManager
from libhardware.libhardware import dmiInfo, pciInfo
from platforms import getPlatform

contractor = getClient()

print( 'Getting Hardware Information...' )
hardware = {}
hardware[ 'dmi' ] = dmiInfo()
hardware[ 'pci' ] = pciInfo()
hardware[ 'total_ram' ] = lib.getRAMAmmount()
hardware[ 'total_cpu_count' ] = lib.cpuLogicalCount()
hardware[ 'total_cpu_sockets' ] = lib.cpuPhysicalCount()

dm = DriveManager()

print( 'Getting Platform Handler...' )
platform = getPlatform( hardware )
platform.startIdentify()

print( 'Found "{0}" Platform.'.format( platform.type ) )

primary_iface = open( '/tmp/dhcp-interface', 'r' ).read().strip()

foundation_locator = None

print( 'Generating unique Id...' )
hasher = hashlib.sha256()

for item in hardware[ 'dmi' ][ 'Processor Information' ]:
  hasher.update( item[ 'ID' ].encode() )

for item in hardware[ 'dmi' ][ 'System Info' ]:
  hasher.update( item[ 'Product Name' ].encode() )
  hasher.update( item[ 'Version' ].encode() )
  hasher.update( item[ 'Serial Number' ].encode() )
  hasher.update( item[ 'UUID' ].encode() )

for item in hardware[ 'dmi' ][ 'Base Board Information' ]:
  hasher.update( item[ 'Product Name' ].encode() )
  hasher.update( item[ 'Serial Number' ].encode() )
  hasher.update( item[ 'Asset Tag' ].encode() )

for item in hardware[ 'dmi' ][ 'Chassis Information' ]:
  hasher.update( item[ 'Version' ].encode() )
  hasher.update( item[ 'Serial Number' ].encode() )
  hasher.update( item[ 'Asset Tag' ].encode() )

identifier = hasher.hexdigest()

print( 'My Identifier is "{0}"'.format( identifier ) )
bootstrap = lib.Bootstrap( identifier, contractor )   # TODO: start a try block and post any exception to the Cartographer

lib._setMessage = bootstrap.setMessage
platform._setMessage = bootstrap.setMessage

bootstrap.setMessage( 'Getting LLDP Information...' )
print( 'Getting LLDP Information...' )
lldp = lib.getLLDP()

while not foundation_locator:
  print( 'Looking up....' )
  lookup = bootstrap.lookup( { 'hardware': hardware, 'lldp': lldp, 'ip_address': lib.getIpAddress( primary_iface ) } )

  if lookup[ 'matched_by' ] is None:
    bootstrap.setMessage( 'no match' )
    print( 'Waiting....' )
    time.sleep( 10 + random.randint( 0, 41 ) )

  else:
    foundation_locator = lookup[ 'locator' ]

print( '** Hello World! I am Foundation "{0}", nice to meet you! **'.format( foundation_locator ) )
print( 'Lookedup by "{0}"'.format( lookup[ 'matched_by'] ) )
bootstrap.setMessage( 'Looked up as "{0}"'.format( foundation_locator ) )

config = contractor.getConfig( foundation_locator=foundation_locator )
config[ 'ipmi_lan_channel' ] = config.get( 'ipmi_lan_channel', 1 )

bootstrap.setMessage( 'Geting Hardware Information...' )
print( 'Getting Network Information...' )
network = {}

for item in glob.glob( '/sys/class/net/eth*' ):
  network[ item.split( '/' )[ -1 ] ] = { 'mac': open( os.path.join( item, 'address' ), 'r' ).read().strip() }

platform.updateNetwork( config, network )

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

bootstrap.setMessage( 'Reporting Hardware info...' )
print( 'Reporting Hardware info to contractor...' )
error = bootstrap.setIdMap( foundation_locator, { 'hardware': hardware, 'network': network, 'disks': disks } )
if error is not None:
  bootstrap.setMessage( 'Hardware Error: "{0}"'.format( error ) )
  bootstrap.logout()
  sys.exit( 20 )

bootstrap.setMessage( 'Hardware Profile Verified' )

iface_list = []

bootstrap.setMessage( 'Configuring {0}...'.format( platform.type ) )

changed = platform.setup( config )

changed != platform.setAuth( config )

changed != platform.setIp( config )

if changed:
  print( 'Preping Platform...' )
  bootstrap.setMessage( 'Preping Platform...' )
  platform.prep()


if config.get( 'bootstrap_wipe_mbr', False ):
  bootstrap.setMessage( 'Clearing MBRs...' )
  for drive in dm.drive_list:
    if drive.block_path is not None:
      tmp = open( drive.block_path, 'r+b' )
      tmp.write( bytes( [ 0, 0, 0, 0, 0 ] ) )
      tmp.close()

# if config.get( 'bios_config', False ) and config.get( 'bios_config_location', False ):
#   bootstrap.setMessage( 'Loading Initial BIOS Config' )
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
#       bootstrap.setMessage( 'Error getting bios config' )
#       bootstrap.logout()
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
#       bootstrap.setMessage( 'Motherboard "{0}" not supported'.format( baseboard ) )
#       bootstrap.logout()
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
#     bootstrap.setMessage( 'BIOS Config Failed' )
#     bootstrap.logout()
#     sys.exit( 1 )

bootstrap.setMessage( 'Cleaning up...' )

platform.stopIdentify()

bootstrap.done()
bootstrap.logout()
print( 'All Done' )
sys.exit( 0 )
