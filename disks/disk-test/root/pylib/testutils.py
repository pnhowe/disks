import time

import procutils

# these times are in (seconds)
POST_WAKE_DELAY = 120 # number of second to wait after waking drives before doing anything with them, 30 seconds was to short... now that we have a better wakup command do we still need this?

FDISK_CMD = '/sbin/sfdisk %(block_path)s'
MKFS_CMD = '/sbin/mke2fs -q -m 0 -t ext3 %(block_path)s1'
FSCK_CMD = '/sbin/fsck.ext3 -p -f %(block_path)s1'
MK_MOUNT_POINT_CMD = '/bin/mkdir /mnt/%(block_name)s'
MOUNT_CMD = '/bin/mount %(block_path)s1 /mnt/%(block_name)s'
UNMOUNT_CMD = '/bin/umount /mnt/%(block_name)s'
CLRPARTTABLE_CMD = '/sbin/clrparttable %(block_path)s'
SUSPEND_CMD = '/bin/drivepower -i %(libdrive_name)s'
#SUSPEND_CMD        = '/bin/drivepower -l %(libdrive_name)s'  # Voyager Drive DAEs do not support this, the disk disapears
#WAKE_CMD           = '/bin/drivepower -w %(libdrive_name)s'  # And also there are SCSI support issues
WAKE_CMD = '/bin/drivepower -a %(libdrive_name)s'
WIPE_CMD = '/sbin/wipedrive -k 61440 -z -i 120 %(libdrive_name)s' # 61440 = 4096 * 15, just smaller than 0xffff (LBA Count), the max trim size for ATA
SELFTEST_CMD = '/sbin/selftest -k -i 120 --long-or-short %(libdrive_name)s'
SHORTSELFTEST_CMD = '/sbin/selftest -k -i 120 --short %(libdrive_name)s'
CONVEYANCETEST_CMD = '/sbin/selftest -k -i 120 --conveyance-or-short %(libdrive_name)s'
THRASH_CMD = None
MUNCH_CMD = None
BONNIE_CMD = None

def setTestParms( destructive, muncher_dma, bonnie ):
  global THRASH_CMD, MUNCH_CMD, BONNIE_CMD

  if destructive:
    THRASH_CMD = '/sbin/thrash -v -c%(count)d %(block_path)s -w'
    MUNCH_CMD = '/sbin/muncher -b -a%(rand)d -p%(sweep)d -f%(full)d -c%(count)d -toff'
  else:
    THRASH_CMD = '/sbin/thrash -v -c%(count)d %(block_path)s'
    MUNCH_CMD = '/sbin/muncher     -a%(rand)d -p%(sweep)d -f%(full)d -c%(count)d -toff'

  if muncher_dma:
    MUNCH_CMD += ' -d'

  MUNCH_CMD += ' %(libdrive_name)s'

  BONNIE_CMD = '/sbin/bonnie -r 0 -d /mnt/%(block_name)s -u root ' + bonnie


def doThrash( manager, count, maxTime, desc ):
  print 'Starting Thrashers...'
  lastline_list = procutils.runOnEachDriveWait( manager, THRASH_CMD % { 'count': count, 'libdrive_name': '%(libdrive_name)s', 'block_name': '%(block_name)s', 'block_path': '%(block_path)s' }, maxTime )

  for drive in lastline_list:
    print '%s: %s' % ( drive.name, lastline_list[drive] )


def doMunch( manager, count, maxTime, desc ):
  rounds = 2
  tmp = count / ( 100 * rounds )

  print 'Starting Munchers...'
  lastline_list = procutils.runOnEachDriveWait( manager, MUNCH_CMD % { 'rand': ( tmp * 58 ), 'sweep': ( tmp * 10 ), 'full': tmp, 'count': rounds, 'libdrive_name': '%(libdrive_name)s', 'block_name': '%(block_name)s', 'block_path': '%(block_path)s' }, maxTime )

  for drive in lastline_list:
    print '%s: %s' % ( drive.name, lastline_list[drive] )


def doSpinDownSleep( manager, delay, desc ):
  print 'Putting Disks to Sleep ...'
  procutils.runOnEachDrive( manager, SUSPEND_CMD )

  if len( manager.working_list ) < 1: # no point in waiting, they all errored out
    return

  print 'Sleep Started at %s for %.2f hours.' % ( time.strftime( "%a, %d %b %Y %H:%M:%S +0000", time.gmtime() ), delay / 60.0 )
  time.sleep( delay * 60 )

  print 'Waking drives ...'
  procutils.runOnEachDrive( manager, WAKE_CMD )

  print 'Post Wake delay (let the drives get going) ...'
  time.sleep( POST_WAKE_DELAY )


def doPartitionFormatMount( manager, maxTime ):
  print 'Partitioning...'
  procutils.runOnEachDrive( manager, FDISK_CMD, '0,,L\n\n' )

  if len( manager.working_list ) < 1: # no point in waiting, they all errored out
    return

  time.sleep( 120 ) # let udev get it's job done

  print 'Formatting...'
  procutils.runOnEachDriveWait( manager, MKFS_CMD, maxTime )

  print 'Making Mount Point...'
  procutils.runOnEachDrive( manager, MK_MOUNT_POINT_CMD )

  print 'Mounting Disks...'
  procutils.runOnEachDrive( manager, MOUNT_CMD )


def doUnmountCheckFilesystem( manager, maxTime ):
  print 'Unmounting...'
  procutils.runOnEachDrive( manager, UNMOUNT_CMD )

  print 'Fscking...'
  procutils.runOnEachDriveWait( manager, FSCK_CMD, maxTime )


def doFilesystemTest( manager, maxTime ):
  print 'Starting Bonnie...'
  procutils.runOnEachDriveWait( manager, BONNIE_CMD, maxTime )


def doClearPartitionTable( manager, maxTime ):
  print 'Clearing Partition Table...'
  procutils.runOnEachDriveWait( manager, CLRPARTTABLE_CMD, maxTime )


def doWipeDrive( manager, maxTime ):
  print 'Wiping Drive...'
  procutils.runOnEachDriveWait( manager, WIPE_CMD, maxTime )


def doConveyanceTest( manager, maxTime ):
  print 'Starting Conveyance Tests...'
  procutils.runOnEachDriveWait( manager, CONVEYANCETEST_CMD, maxTime )


def doShortSelfTest( manager, maxTime ):
  print 'Starting Self Tests...'
  procutils.runOnEachDriveWait( manager, SHORTSELFTEST_CMD, maxTime )


def doSelfTest( manager, maxTime ):
  print 'Starting Self Tests...'
  procutils.runOnEachDriveWait( manager, SELFTEST_CMD, maxTime )
