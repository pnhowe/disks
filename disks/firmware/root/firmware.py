#!/usr/bin/env python3

import sys
from urllib import request
import tempfile
import os
import traceback

from contractor.client import getClient
from firmware.manager import Manager
from firmware.planner import findStep


chunk_size = 4096

manager = Manager()
handler_list = manager.getHandlers()

contractor = getClient( '/etc/job.config' )
contractor.login()

config = contractor.getConfig()
resource_location = config.get( 'firmware_resource_location', None )
proxy = config.get( 'firmware_proxy', None )

if not resource_location:
  print( 'firmware_resource_location not defined.' )
  sys.exit( 1 )

firmware = config.get( 'firmware', None )
if not firmware:
  print( 'firmware not defined.' )
  sys.exit( 1 )

firmware_targets = config.get( 'firmware_targets', None )
if not firmware_targets:
  print( 'firmware_targets not defined.' )
  sys.exit( 1 )

print( f'Using "{resource_location}" for firmware resource via proxy "{proxy}"' )

if proxy:
  print( 'Using Proxy "{0}" for getting resources'.format( proxy ) )
  opener = request.build_opener( request.ProxyHandler( { 'http': proxy, 'https': proxy } ) )
else:
  opener = request.build_opener( request.ProxyHandler( {} ) )

filename_cache = {}

# try:
#   try:
#     count = int( platopxe.readEnv( 'firmware-update_boot_count' ) )
#   except ( TypeError, ValueError ):
#     count = 0

#   print( f'Boot Count: {count}' )

#   if count > 5:
#     print 'Rebooted more than 5 times, something is wrong.'
#     sys.exit( 1 )

#   platopxe.writeEnv( 'firmware-update_boot_count', count + 1 )
# except PlatoPxeNoJobException:
#   print 'WARNING: No job to store boot count... We may reboot forever without knowing.'

dirty = False

contractor.postMessage( 'Setting up handlers...' )
for ( name, handler ) in handler_list:
  print( f'Setting Up Handler "{name}"...' )
  handler.setup()

contractor.postMessage( 'Checking for Updates....' )
for ( name, handler ) in handler_list:
  try:
    version_map = firmware_targets[ name ]
  except KeyError:
    print( f'No Firmware Target for Handler "{name}", skipping...' )
    continue

  try:
    firmware_map = firmware[ name ]
  except KeyError:
    print( f'Error: No Firmware for Handler, however there is a version for it "{name}".' )
    sys.exit( 1 )

  print( f'Checking Handler "{name}"...' )
  try:
    target_list = handler.getTargets()
  except Exception as e:
    print( f'Error getting Target list: {e}' )
    print( traceback.format_exc() )
    sys.exit( 1 )

  for ( target, model, version ) in target_list:
    print( f'Checking for update for target "{target}"...' )
    print( f'  model "{model}" version "{version}"...' )
    try:
      target_version = version_map[ model ]
    except KeyError:
      print( f'Model {model} not found in version_map' )
      continue

    print( f'    Want "{target_version}"...' )
    if version == target_version:
      print( '    All Good Skipping...' )
      continue

    if not dirty:  # dirty is set after the first update, we will barrow that to know if it is the first update
      handler.beforeUpdates()

    try:
      model_map = firmware_map[ model ]
    except KeyError:
      print( f'Error getting Model "{model}" from firmware_map' )
      sys.exit( 1 )

    filename = findStep( version, target_version, model_map )
    if not filename:
      print( 'Unable to find next step' )
      sys.exit( 1 )

    if filename not in filename_cache:
      url = f'{resource_location}{filename}'
      try:
        resp = opener.open( url )
      except request.HTTPError as e:
        print( f'error retreiving "{url}": {e}' )
        sys.exit( 1 )

      ( fd, local_filename ) = tempfile.mkstemp()
      buff = resp.read( chunk_size )
      while buff:
        os.write( fd, buff )
        buff = resp.read( chunk_size )

      resp.close()
      os.close( fd )
      print( f'Downloaded "{filename}" to "{local_filename}"' )
      filename_cache[ filename ] = local_filename

    contractor.postMessage( f'Updating "{target}" with "{filename}"' )
    if not handler.updateTarget( target, filename_cache[ filename ] ):
      print( 'Firmware update Failed' )
      sys.exit( 1 )

    dirty = True

  # TODO: add a flag to the handler to determin if a reboot is really needed
  # TODO: add a value to indicate how many processes can be done at the same time per handler
  if dirty:  # don't want to try to update harddrive firmware if the HBA/RAID card's firmware hasn't been rebooted
    handler.afterUpdates()  # only running if something was updated, aka dirty
    break

if dirty:
  contractor.postMessage( 'Firmware Changed, rebooting.' )
  sys.exit( 20 )

print( 'Done!' )
sys.exit( 0 )
