import os
import glob
import re
import signal
import time
from configparser import NoOptionError
from installer.procutils import execute, execute_lines
from installer.packaging import installPackages
from libdrive.libdrive import DriveManager

filesystem_list = []
mount_list = []

partition_map = {}
boot_drives = []

mkfs_map = {}
mkfs_map[ 'ext2' ] = '/sbin/mkfs.ext2 -F -text2 {block_device}'
mkfs_map[ 'ext3' ] = '/sbin/mkfs.ext2 -F -text3 {block_device}'
mkfs_map[ 'ext4' ] = '/sbin/mkfs.ext2 -F -text4 {block_device}'
mkfs_map[ 'xfs' ] = '/sbin/mkfs.xfs -f {block_device}'
mkfs_map[ 'vfat' ] = '/sbin/mkfs.vfat {block_device}'
mkfs_map[ 'swap' ] = '/sbin/mkswap {block_device}'

FS_WITH_TRIM = ( 'ext4', 'xfs', 'btrfs' )

# NOTE, bios grub partitions will be auto added to any drives holding /boot or / if there is no /boot
# swap = swap_size / total swap partitions
partition_recipes = {}
partition_recipes[ 'single' ] = [
                                  { 'target': 0, 'mount_point': '/boot', 'size': 'boot' },
                                  { 'target': 0, 'mount_point': '/', 'size': '- swap' },
                                  { 'target': 0, 'type': 'swap', 'size': 'end' }
                                ]

partition_recipes[ 'mirror' ] = [
                                  { 'target': 0, 'type': 'md', 'group': 0, 'size': 'boot' },
                                  { 'target': 1, 'type': 'md', 'group': 0, 'size': 'boot' },
                                  { 'target': 0, 'type': 'md', 'group': 1, 'size': '- swap' },
                                  { 'target': 1, 'type': 'md', 'group': 1, 'size': '- swap' },
                                  { 'target': 0, 'type': 'swap', 'size': 'end' },
                                  { 'target': 1, 'type': 'swap', 'size': 'end' },
                                  { 'md': 0, 'level': 1, 'mount_point': '/boot' },
                                  { 'md': 1, 'level': 1, 'mount_point': '/' }
                                ]

partition_recipes[ 'raid10' ] = [
                                  { 'target': 0, 'type': 'md', 'group': 0, 'size': 'boot' },
                                  { 'target': 1, 'type': 'md', 'group': 0, 'size': 'boot' },
                                  { 'target': 2, 'type': 'swap', 'size': 'swap' },
                                  { 'target': 3, 'type': 'swap', 'size': 'swap' },
                                  { 'target': 0, 'type': 'md', 'group': 1, 'size': 'end' },
                                  { 'target': 1, 'type': 'md', 'group': 1, 'size': 'end' },
                                  { 'target': 2, 'type': 'md', 'group': 1, 'size': 'end' },
                                  { 'target': 3, 'type': 'md', 'group': 1, 'size': 'end' },
                                  { 'md': 0, 'level': 1, 'mount_point': '/boot' },
                                  { 'md': 1, 'level': 10, 'mount_point': '/' },
                                  { 'ref_size': 'boot', 'lambda': lambda sizes: sizes[ 'swap' ] if sizes[ 'swap' ] > sizes[ 'boot' ] else sizes[ 'boot' ] },
                                  { 'ref_size': 'swap', 'lambda': lambda sizes: ( sizes[ 'swap' ] - sizes[ 'boot_rsvd' ] ) if sizes[ 'swap' ] > sizes[ 'boot' ] else ( sizes[ 'boot' ] - sizes[ 'boot_rsvd' ] ) }
                                ]

partition_recipes[ 'raid6' ] = [
                                 { 'target': 0, 'type': 'md', 'group': 0, 'size': 'boot' },
                                 { 'target': 1, 'type': 'md', 'group': 0, 'size': 'boot' },
                                 { 'target': 2, 'type': 'swap', 'size': 'swap' },
                                 { 'target': 3, 'type': 'swap', 'size': 'swap' },
                                 { 'target': 0, 'type': 'md', 'group': 1, 'size': 'end' },
                                 { 'target': 1, 'type': 'md', 'group': 1, 'size': 'end' },
                                 { 'target': 2, 'type': 'md', 'group': 1, 'size': 'end' },
                                 { 'target': 3, 'type': 'md', 'group': 1, 'size': 'end' },
                                 { 'md': 0, 'level': 1, 'mount_point': '/boot' },
                                 { 'md': 1, 'level': 6, 'mount_point': '/' },
                                 { 'ref_size': 'boot', 'lambda': lambda sizes: sizes[ 'swap' ] if sizes[ 'swap' ] > sizes[ 'boot' ] else sizes[ 'boot' ] },
                                 { 'ref_size': 'swap', 'lambda': lambda sizes: ( sizes[ 'swap' ] - sizes[ 'boot_rsvd' ] ) if sizes[ 'swap' ] > sizes[ 'boot' ] else ( sizes[ 'boot' ] - sizes[ 'boot_rsvd' ] ) }
                                ]

partition_recipes[ 'single-lvm' ] = [
                                      { 'target': 0, 'mount_point': '/boot', 'size': 'boot' },
                                      { 'target': 0, 'type': 'pv', 'group': 0, 'size': '- swap' },
                                      { 'target': 0, 'type': 'swap', 'size': 'end' },
                                      { 'vg': 0, 'name': 'VG0' },
                                      { 'lv': 0, 'name': 'root', 'mount_point': '/', 'size': '100%' }
                                    ]

partition_recipes[ 'dual-lvm' ] = [
                                    { 'target': 0, 'type': 'md', 'group': 0, 'size': 'boot' },
                                    { 'target': 1, 'type': 'md', 'group': 0, 'size': 'boot' },
                                    { 'target': 0, 'type': 'pv', 'group': 0, 'size': '- swap' },
                                    { 'target': 0, 'type': 'swap', 'size': 'end' },
                                    { 'target': 1, 'type': 'pv', 'group': 0, 'size': '- swap' },
                                    { 'target': 1, 'type': 'swap', 'size': 'end' },
                                    { 'md': 0, 'level': 1, 'mount_point': '/boot' },
                                    { 'vg': 0, 'name': 'VG0' },
                                    { 'lv': 0, 'name': 'root', 'mount_point': '/', 'size': '95%' }  # TODO: get the caculs right to get this to 100%
                                  ]


class FileSystemException( Exception ):
  pass


class MissingDrives( Exception ):
  pass


def _parted( block_device, cmd ):
  execute( '/sbin/parted -s {0} -- {1}'.format( block_device, cmd ) )


partition_counter = {}
partition_pos = {}


def _addPartition( block_device, part_type, size ):  # size in M
  global partition_counter

  size = int( size )  # just incase something gives us a float
  print( 'Creating Partition on "{0}" of type "{1}" size "{2}"...'.format( block_device, part_type, size ) )

  try:
    start = partition_pos[ block_device ]
    if start == 0:
      raise Exception( 'Block Device allready full' )
  except KeyError:
    partition_counter[ block_device ] = 0
    start = 0

  if part_type != 'blank':
    partition_counter[ block_device ] += 1

  if size == 0:
    offsets = '{0}m -0'.format( start )
    partition_pos[ block_device ] = 0

  elif size < 0:
    offsets = '{0}m {1}m'.format( start, size )
    partition_pos[ block_device ] = size

  else:
    offsets = '{0}m {1}m'.format( start, ( start + size ) )
    partition_pos[ block_device ] = start + size

  if part_type == 'blank':  # we are only incramenting the position counters
    return

  dev_name = '{0}{1}'.format( block_device, partition_counter[ block_device ] )
  if part_type == 'bios_grub':
    _parted( block_device, 'mkpart primary fat32 {0}'.format( offsets ) )
    _parted( block_device, 'set 1 bios_grub on' )

  elif part_type == 'fs':
    _parted( block_device, 'mkpart primary ext2 {0}'.format( offsets ) )

  elif part_type == 'swap':
    _parted( block_device, 'mkpart primary linux-swap {0}'.format( offsets ) )

  else:
    raise Exception( 'Unknown partition type' )

  return dev_name


def _addRAID( id, member_list, md_type, meta_version ):
  dev_name = '/dev/md{0}'.format( id )

  print( 'Creating RAID "{0}" of type "{1}" members {2}...'.format( dev_name, md_type, member_list ) )

  if md_type in ( 1, 10 ) and len( member_list ) % 2:
    raise Exception( 'RAID {0} requires even number of members'.format( md_type ) )

  if md_type in ( 5, 6 ) and len( member_list ) < 3:
    raise Exception( 'RAID {0} requires a minimum of 3 members'.format( md_type ) )

  execute( '/sbin/mdadm {0} --create --run --force --metadata={1} --level={2} --raid-devices={3} {4}'.format( dev_name, meta_version, md_type, len( member_list ), ' '.join( member_list ) ) )

  return dev_name


def _targetsWithMount( recipe, mount_point ):
  md_device = None
  target_device = None
  for item in [ i for i in recipe if 'mount_point' in i ]:
    if item[ 'mount_point' ] == mount_point:
      try:
        md_device = int( item[ 'md' ] )
      except KeyError:
        pass

      try:
        target_device = int( item[ 'target' ] )
      except KeyError:
        pass

  if md_device is None and target_device is None:  # target_device/md_device can be 0
    return []

  if md_device is not None and target_device is not None:
    raise Exception( 'Confused as to where "{0}" goes'.format( mount_point ) )

  if md_device is not None:
    wrk = []
    for item in [ i for i in recipe if 'target' in i ]:
      try:
        if int( item[ 'group' ] ) == md_device:
          wrk.append( int( item[ 'target' ] ) )
      except KeyError:
        pass

    return wrk
  else:
    return [ target_device ]


def _getTargetDrives( target_drives ):
  dm = DriveManager()  # must re-enumerate the drives everything we look for targets, incase the kernel is still enumerating/detecting
  block_list = list( dm.drive_map.keys() )
  block_list.sort( key=lambda x: ( len( x ), x ) )

  drive_map = {}
  for port in dm.drive_map:
    drive_map[ port ] = dm.drive_map[ port ].drive

  if not target_drives:
    return ( block_list, drive_map )

  result = []

  tmp = dm.port_map
  port_map = dict( [ ( i, tmp[i].block_path ) for i in tmp if tmp[i] ] )
  tmp = dm.devpath_map
  devpath_map = dict( [ ( i, tmp[i].block_path ) for i in tmp if tmp[i] ] )

  for drive in target_drives:
    block = None
    if drive in block_list:
      block = drive

    if not block:
      try:
        block = port_map[ drive ]
      except KeyError:
        pass

    if not block:
      try:
        block = devpath_map[ drive ]
      except KeyError:
        pass

    if not block:
      tmp = [ port_map[i] for i in port_map if re.search( drive, i ) ]
      try:
        block = tmp[0]
      except IndexError:
        pass

    if not block:
      tmp = [ devpath_map[i] for i in devpath_map if re.search( drive, i ) ]
      try:
        block = tmp[0]
      except IndexError:
        pass

    if not block:
      raise MissingDrives( drive )
    else:
      result.append( block )

  return ( result, drive_map )


def partition( profile, config ):
  global filesystem_list, boot_drives, partition_map

  partition_type = profile.get( 'filesystem', 'partition_type' )
  if partition_type not in ( 'gpt', 'msdos' ):
    raise FileSystemException( 'Invalid partition_type "{0}"'.format( partition_type ) )

  fs_type = profile.get( 'filesystem', 'fs_type' )
  if fs_type not in mkfs_map.keys():
    raise FileSystemException( 'Invalid fs_type "{0}"'.format( fs_type ) )

  md_meta_version = profile.get( 'filesystem', 'md_meta_version' )
  try:
    if config[ 'md_meta_version' ]:
      md_meta_version = config[ 'md_meta_version' ]
  except KeyError:
    pass

  try:
    tmp_target_drives = config[ 'target_drives' ]
  except KeyError:
    tmp_target_drives = None

  target_drives = None

  if tmp_target_drives:
    print( 'Target Drives: "{0}"'.format( '", "'.join( tmp_target_drives ) ) )

  count = 0
  while True:
    try:
      ( target_drives, drive_map ) = _getTargetDrives( tmp_target_drives )
      break
    except MissingDrives as e:
      count += 1
      if count >= 10:
        raise e
      print( 'Waiting for target drive ({0}) to appear...'.format( e ) )
      time.sleep( 6 )

  print( 'Target Block Devices: "{0}"'.format( '", "'.join( target_drives ) ) )

  if partition_type == 'gpt':
    boot_rsvd_size = 5
  else:
    boot_rsvd_size = 1

  swap_size = 512
  try:
    if config[ 'swap_size' ]:
      swap_size = int( config[ 'swap_size' ] )
  except KeyError:
    pass

  boot_size = 512
  try:
    if config[ 'boot_size' ]:
      boot_size = int( config[ 'boot_size' ] )
  except KeyError:
    pass

  mounting_options = []
  try:
    if config[ 'mounting_options' ] and config[ 'mounting_options' ] != 'defaults':
      mounting_options = config[ 'mounting_options' ].split( ',' )
  except KeyError:
    pass

  scheme = 'single'
  try:
    if config[ 'partition_scheme' ]:
      scheme = config[ 'partition_scheme' ]
  except KeyError:
    pass

  recipe = None
  if scheme == 'custom':
    try:
      recipe = config[ 'partition_recipe' ]
    except KeyError:
      raise Exception( 'custom paritition_scheme specified, but recipe not found.' )

  elif scheme in partition_recipes:
    recipe = partition_recipes[ scheme ]

  if not recipe:
    raise Exception( 'Unknown partitioning scheme "{0}"'.format( scheme ) )

  targets = set( [ int( i[ 'target' ] ) for i in recipe if 'target' in i ] )
  if len( target_drives ) <= max( targets ):
    raise Exception( 'Not Enough Target Drives, need "{0}" found "{1}"'.format( max( targets ) + 1, len( target_drives ) ) )

  md_list = {}
  pv_list = {}

  try:
    swap_size = swap_size / sum( [ 1 for i in recipe if 'type' in i and i[ 'type' ] == 'swap' ] )
  except ZeroDivisionError:  # swap unused
    swap_size = 0

  # check drive sizes

  boot_drives = _targetsWithMount( recipe, '/boot' )
  if not boot_drives:
    boot_drives = _targetsWithMount( recipe, '/' )
  if not boot_drives:
    raise Exception( 'Unable to determine the boot_drives' )

  boot_drives = [ target_drives[i] for i in boot_drives ]

  for target in targets:
    _parted( target_drives[ target ], 'mklabel {0}'.format( partition_type ) )

  if partition_type == 'gpt':
    for drive in boot_drives:
      filesystem_list.append( { 'type': 'vfat', 'block_device': _addPartition( drive, 'bios_grub', boot_rsvd_size ) } )

  else:  # give room for MBR Boot sector
    for drive in boot_drives:
       _addPartition( drive, 'blank', boot_rsvd_size )

  # { 'ref_size': [ swap | boot ], 'lambda': < lambda takes a dict returns a number, sizes in the dict update as it goes > }
  for item in [ i for i in recipe if 'ref_size' in i ]:
    sizes = { 'swap': swap_size, 'boot': boot_size, 'boot_rsvd': boot_rsvd_size }
    if 'lambda' in item:
      tmp = item[ 'lambda' ]( sizes )

    if item[ 'ref_size' ] == 'boot':
      boot_size = tmp

    elif item[ 'ref_size' ] == 'swap':
      swap_size = tmp

  # { 'target': < drive # >, [ 'type': '<swap|fs type|md|pv|blank>' if not specified, fs will be used], [ 'group': <md group> (if type == 'md') ][ 'group': <volume group group> (if type == 'pv') ][ 'options': '<mounting options other than the default>' (for type == 'fs') | 'priority': <swap memer priority other then 0> (for type == 'swap') ] [ 'mount_point': '< mount point >' if type is a fs type, set to None to not mount], 'size': '<see if statement>' }
  for item in [ i for i in recipe if 'target' in i ]:
    target = int( item[ 'target' ] )
    target_drive = target_drives[ target ]
    size = item[ 'size' ]

    if size == 'end':
      size = 0

    elif size == 'boot':
      size = boot_size

    elif size == '- boot':
      size = -1 * boot_size

    elif size == 'swap':
      size = swap_size

    elif size == '- swap':
      size = -1 * swap_size

    elif isinstance( size, str ) and size[-1] == '%':
      size = float( size[0:-1] ) / 100.0
      if size == 1.0:
        size = 0  # avoid any potential math problems

      elif size <= 0.0 or size >= 1.0:  # yes inclusive, can't do a 0% nor a 100% sized disks
        raise Exception( 'Invalid size percentage "{0}"'.format( item[ 'size' ] ) )
        size = int( ( drive_map[ target_drive ].drive.capacity * 1024 ) * size )  # drive.capacity is in Gb

    else:
      size = int( size )  # in Mb

    try:
      part_type = item[ 'type' ]
    except KeyError:
      part_type = fs_type

    try:
      if item[ 'options' ] != 'defaults':
        options = item[ 'options' ].split( ',' )
    except KeyError:
      options = list( mounting_options )

    try:
      priority = int( item[ 'priority' ] )
    except KeyError:
      priority = 0

    if part_type == 'swap':
      block_device = _addPartition( target_drive, 'swap', size )

    elif part_type == 'blank':
      _addPartition( target_drive, 'blank', size )

    else:
      block_device = _addPartition( target_drive, 'fs', size )

    if part_type == 'md':
      try:
        md_list[ int( item[ 'group' ] ) ].append( ( block_device, drive_map[ target_drive ] ) )
      except KeyError:
        md_list[ int( item[ 'group' ] ) ] = [ ( block_device, drive_map[ target_drive ] ) ]

    elif part_type == 'pv':
      try:
        pv_list[ int( item[ 'group' ] ) ].append( ( block_device, drive_map[ target_drive ] ) )
      except KeyError:
        pv_list[ int( item[ 'group' ] ) ] = [ ( block_device, drive_map[ target_drive ] ) ]

    elif part_type == 'swap':
      filesystem_list.append( { 'type': 'swap', 'priority': priority, 'block_device': block_device } )

    elif part_type == 'blank':
      pass

    else:
      if drive_map[ target_drive ].supportsTrim and part_type in FS_WITH_TRIM:
        options.append( 'discard' )

      filesystem_list.append( { 'mount_point': item[ 'mount_point' ], 'type': part_type, 'options': options, 'block_device': block_device } )

  # { 'md': <group #>, [ 'type': '<fs type>' if not specified, default fs will be used], 'level': <raid level> [ 'options': '<mounting options other than the default>' ], [ 'meta_version': <md meta version if not default ], 'mount_point': '<mount point>' }
  for item in [ i for i in recipe if 'md' in i ]:
    try:
      part_type = item[ 'type' ]
    except KeyError:
      part_type = fs_type

    try:
      meta_version = item[ 'meta_version' ]
    except KeyError:
      meta_version = md_meta_version

    try:
      if item[ 'options' ] != 'defaults':
        options = item[ 'options' ].split( ',' )
    except KeyError:
      options = list( mounting_options )

    block_list = []
    supportsTrim = True
    for ( block_device, drive ) in md_list[ int( item[ 'md' ] ) ]:
      block_list.append( block_device )
      supportsTrim &= drive.supportsTrim

    block_device = _addRAID( int( item[ 'md' ] ), block_list, int( item[ 'level' ] ), meta_version )

    if part_type == 'swap':
        filesystem_list.append( { 'type': 'swap', 'priority': priority, 'block_device': block_device } )

    else:
      if supportsTrim and part_type in FS_WITH_TRIM:
        options.append( 'discard' )

      filesystem_list.append( { 'mount_point': item[ 'mount_point' ], 'type': part_type, 'options': options, 'block_device': block_device, 'members': block_list } )

  if pv_list:
    vg_names = {}
    vg_extents = {}
    vg_disks = {}

    for vg in pv_list:
      for ( block_device, drive ) in pv_list[ vg ]:
        print( 'Creating PV on "{0}"...'.format( block_device ) )
        execute( '/sbin/lvm pvcreate -ff -y {0}'.format( block_device ) )

    # { 'vg': < volumne group #>, 'name': <volume group name > },
    for item in [ i for i in recipe if 'vg' in i ]:
      vg = int( item[ 'vg' ] )
      vg_names[ vg ] = item[ 'name' ]
      vg_disks[ vg ] = []
      block_list = []
      for ( block_device, drive ) in pv_list[ vg ]:
        block_list.append( block_device )
        vg_disks[ vg ].append( drive )

      print( 'Creating LV "{0}" members {1}...'.format( vg_names[ vg ], block_list ) )
      execute( '/sbin/lvm vgcreate {0} {1}'.format( vg_names[ vg ], ' '.join( block_list ) ) )
      for line in execute_lines( '/sbin/lvm vgdisplay {0}'.format( vg_names[ vg ] ) ):
        result = re.match( '[ ]*Total PE[ ]*([0-9\.]*)', line )
        if result:
          vg_extents[ vg ] = int( result.group( 1 ) )
          break
      if vg not in vg_extents or not vg_extents[ vg ]:
        raise Exception( 'Unable to get Size/Extents of VG "{0}"'.format( vg_names[ vg ] ) )

    # { 'lv': < volume group #>, 'name': <lv name>, 'mount_point': <mount point>, 'size': < integer size>, [ 'type': '<fs type>' if not specified, default fs will be used][ 'options': '<mounting options other than the default>' ] }
    for item in [ i for i in recipe if 'lv' in i ]:
      vg = int( item[ 'lv' ] )
      size = item[ 'size' ]

      if size == 'boot':
        size = boot_size

      elif size == 'swap':
        size = swap_size

      if isinstance( size, str ) and size[-1] == '%':
        size = float( size[0:-1] ) / 100.0
        if size == 1.0:
          size = '--extents {0}'.format( vg_extents[ vg ] )  # avoid any potential math problems
        elif size <= 0.0 or size >= 1.0:
          raise Exception( 'Invalid size percentage "{0}"'.format( item[ 'size' ] ) )
        else:
          size = '--extents {0}'.format( int( vg_extents[ vg ] * size ) )
      else:
        size = '--size {0}M'.format( int( size ) )

      try:
        part_type = item[ 'type' ]
      except KeyError:
        part_type = fs_type

      try:
        if item[ 'options' ] != 'defaults':
          options = item[ 'options' ].split( ',' )
      except KeyError:
        options = list( mounting_options )

      block_device = '/dev/mapper/{0}-{1}'.format( vg_names[ vg ], item[ 'name' ] )

      print( 'Creating LV "{0}" size "{1}" in VG "{2}"...'.format( item[ 'name' ], size, vg_names[ vg ] ) )
      execute( '/sbin/lvm lvcreate --name "{0}" {1} {2}'.format( item[ 'name' ], size, vg_names[ vg ] ) )

      supportsTrim = True
      for drive in vg_disks[ vg ]:
        supportsTrim &= drive.supportsTrim

      if part_type == 'swap':
          filesystem_list.append( { 'type': 'swap', 'priority': priority, 'block_device': block_device } )

      else:
        if supportsTrim and part_type in FS_WITH_TRIM:
          options.append( 'discard' )

        filesystem_list.append( { 'mount_point': item[ 'mount_point' ], 'type': fs_type, 'options': options, 'block_device': block_device } )

  for fs in filesystem_list:
    try:
      if fs[ 'mount_point' ] is not None:
        partition_map[ fs[ 'mount_point' ] ] = fs[ 'block_device' ]
    except KeyError:
      pass

  if md_list:
    print( 'Giving RAIDs 3 min to sync....' )  # TODO: replace this with something better, mabey sync till / is 10% ?
    open( '/proc/sys/dev/raid/speed_limit_min', 'w' ).write( '500000000' )
    open( '/proc/sys/dev/raid/speed_limit_max', 'w' ).write( '500000000' )
    time.sleep( 180 )
    open( '/proc/sys/dev/raid/speed_limit_min', 'w' ).write( '2000' )

  print( 'Triggering udev...' )
  execute( '/sbin/udevadm trigger' )
  execute( '/sbin/udevadm settle' )


def mkfs():
  for fs in filesystem_list:
    if 'type' in fs:
      print( 'Making Filesystem "{0}" on "{1}"...'.format( fs[ 'type' ], fs[ 'block_device' ] ) )
      execute( mkfs_map[ fs[ 'type' ] ].format( **fs ) )
      for line in execute_lines( '/sbin/blkid -o export {0}'.format( fs[ 'block_device' ] ) ):
        ( key, value ) = line.split( '=' )
        if key.endswith( 'UUID' ):
          fs[ 'uuid' ] = value
          break


def _mount( mount_point, fstype, fs, options=[] ):
  global mount_list

  mount_point = mount_point.strip().rstrip( '/' )

  if not os.path.isdir( mount_point ):
    os.makedirs( mount_point )

  _do_mount( mount_point, fstype, fs, options )
  mount_list.append( ( mount_point, fstype, fs, options ) )


def _do_mount( mount_point, fstype, fs, options=[] ):
  print( 'Mounting "{0}"({1}) to "{2}", options "{3}"...'.format( fs, fstype, mount_point, options ) )

  if options:
    options = '-o {0}'.format( ','.join( options ) )
  else:
    options = ''

  if fstype:
    execute( 'mount {0} -t {1} {2} {3}'.format( options, fstype, fs, mount_point ) )
  else:
    execute( 'mount {0} {1} {2}'.format( options, fs, mount_point ) )


def mount( mount_point, profile ):
  mount_points = {}

  for item in profile.items( 'filesystem' ):
    if item[0].startswith( 'extra_mount_' ):
      parts = [ i.strip() for i in item[1].split( ':' ) ]
      if len( parts ) == 3:
        mount_points[ parts[2] ] = tuple( parts[:2] )
      else:
        mount_points[ parts[2] ] = tuple( parts[:2] + [ parts[3].split( ',' ) ] )

  for fs in filesystem_list:
    if 'mount_point' in fs and fs[ 'mount_point' ] is not None:
      mount_points[ fs[ 'mount_point' ] ] = ( fs[ 'type' ], fs[ 'block_device' ], fs[ 'options' ] )

  mount_order = list( mount_points.keys() )
  mount_order.sort( key=lambda x: len( x )  )  # no need to do any fancy trees, we know that the string length of a dependant mount point is allways longer than it's parent

  for point in mount_order:
    _mount( '{0}{1}'.format( mount_point, point ), *mount_points[ point ] )

  if not os.path.isdir( os.path.join( mount_point, 'etc' ) ):
    os.makedirs( os.path.join( mount_point, 'etc' ) )

  execute( 'ln -s /proc/mounts {0}'.format( os.path.join( mount_point, 'etc', 'mtab' ) ) )  # TODO: write a mtab don't symlink it


def remount():
  mounted = []
  for line in open( '/proc/mounts', 'r' ).readlines():
    ( _, point, _, _, _, _ ) = line.split( ' ' )
    mounted.append( point )

  for mount in mount_list:
    if mount[0] not in mounted:
      _do_mount( *mount )


def unmount( mount_point ):  # don't umount -a, other things after the installer still need /proc and such
  execute( 'rm {0}'.format( os.path.join( mount_point, 'etc', 'mtab' ) ) )
  execute( 'touch {0}'.format( os.path.join( mount_point, 'etc', 'mtab' ) ) )

  count = 0
  pid_list = [ int( i ) for i in execute_lines( 'sh -c "lsof | grep target | cut -f 1 | uniq"' ) ]
  while pid_list:
    if count < 3:
      print( 'Sending SIGTERM to pids: {0} ...'.format( pid_list ) )
      tmp = signal.SIGTERM

    else:
      print( 'Sending SIGKILL to pids: {0} ...'.format( pid_list ) )
      tmp = signal.SIGKILL

    for pid in pid_list:
      try:
        os.kill( pid, tmp )
      except OSError as e:
        if e.errno != 3:
          raise e

    time.sleep( 2 )
    pid_list = [ int( i ) for i in execute_lines( 'sh -c "lsof | grep target | cut -f 1 | uniq"' ) ]
    count += 1

  mount_list.sort( key=lambda x: len( x[0] ) )
  mount_list.reverse()
  for mount in mount_list:
    print( 'Unmounting "{0}"...'.format( mount[0] ) )
    execute( '/bin/umount {0}'.format( mount[0] ) )


def writefstab( mount_point, profile ):
  mount_points = {}
  for fs in filesystem_list:
    if 'mount_point' in fs and fs[ 'mount_point' ] is not None:
      mount_points[ fs[ 'mount_point' ] ] = fs

  mount_order = list( mount_points.keys() )
  mount_order.sort( key=lambda x: len( x ) )

  if not os.path.isdir( os.path.join( mount_point, 'etc' ) ):
    os.makedirs( os.path.join( mount_point, 'etc' ) )

  tmp = open( os.path.join( mount_point, 'etc', 'fstab' ), 'w' )
  for item in profile.items( 'fstab' ):
    if item[0].startswith( 'prefix_' ):
      tmp.write( item[1] )
      tmp.write( '\n' )

  for fs in filesystem_list:
    if fs[ 'type' ] == 'swap':
      tmp.write( 'UUID={0}\tnone\tswap\tsw,pri={1}\t0\t0\n'.format( fs[ 'uuid' ], fs[ 'priority' ] ) )

  for mount in mount_order:
    options = ','.join( mount_points[ mount ][ 'options' ] )
    if not options:
      options = 'defaults'

    tmp.write( 'UUID={0}\t{1}\t{2}\t{3}\t0\t{4}\n'.format( mount_points[ mount ][ 'uuid' ], mount_points[ mount ][ 'mount_point' ], mount_points[ mount ][ 'type' ], options, 1 if mount_points[ mount ][ 'mount_point' ] == '/' else 2 ) )

  tmp.close()


def fsConfigValues():
  tmp = {}
  tmp[ 'mount_points' ] = {}
  tmp[ 'boot_drives' ] = boot_drives
  for mount_point in partition_map:
    if mount_point[0] != '/':
      continue

    tmp[ 'mount_points' ][ mount_point ] = partition_map[ mount_point ]

  for fs in filesystem_list:
    try:
      if fs[ 'mount_point' ] == '/':
        tmp[ 'root_uuid' ] = fs[ 'uuid' ]
    except KeyError:
      pass

  return tmp


def _getFirstMDMember( mount_point ):
  for fs in filesystem_list:
    try:
      if fs[ 'mount_point' ] != mount_point:
        continue
    except KeyError:
      continue

    try:
      return fs[ 'members' ][0]
    except Exception:
      return fs[ 'block_device' ]


def grubConfigValues( mount_point ):
  tmp = {}

  if '/boot' in partition_map:
    boot_path = '/'
    tmp[ 'grub_root' ] = 'hd0,{0}'.format( int( _getFirstMDMember( '/boot' )[-1] ) - 1 )
  else:
    boot_path = '/boot/'
    tmp[ 'grub_root' ] = 'hd0,{0}'.format( int( _getFirstMDMember( '/' )[-1] ) - 1 )

  initrd_match = re.compile( '^initramfs-*' )
  kernel_match = re.compile( '^vmlinuz-*' )
  for filename in glob.glob( '{0}/boot/*'.format( mount_point ) ):
    parts = filename.split( '/' )
    if kernel_match.search( parts[-1] ):
      tmp[ 'kernel_path' ] = os.path.join( boot_path, parts[-1] )
    if initrd_match.search( parts[-1] ):
      tmp[ 'initrd_path' ] = os.path.join( boot_path, parts[-1] )

  return tmp


def installFilesystemUtils( profile ):
  fs_list = []
  mounts = open( '/proc/mounts', 'r' )
  for line in mounts.readlines():
    try:
      fs = line.split( ' ' )[ 2 ]
    except IndexError:
      continue

    if fs not in fs_list:
      fs_list.append( fs )

  mounts.close()

  for fs in fs_list:
    try:
      package = profile.get( 'packages', 'filesystem_{0}_package'.format( fs ) )
    except NoOptionError:
      continue

    installPackages( package )
