import os
import re
import string
from platoclient import libdrive

NO_THRASH_MODELS = ( 'SATADOM-SL 3ME', )

class DriveManager( object ):
  def __init__( self, *argv, **argk ):
    self.reportFail = None
    self.allreadyDead = None
    self.allreadyThrashed = None
    self.all_list = []
    self.good_list = []
    self.skip_reasons = {}
    self.bad_reasons = {}

    libdrive.setCLIs( tw_cli='/usr/sbin/tw_cli', megaraid='/usr/sbin/MegaCli64' )

    self.dm = libdrive.DriveManager()

    for port in self.dm.port_list:
      port.setFault( False )

    for drive in self.dm.drive_list:
      try:
        drive.model # we are just forcing the info to load
      except:
        print 'Unable to get Info on "%s", skipping...' % drive
        continue

      self.all_list.append( drive )
      self.good_list.append( drive )

    self.all_list.sort()
    self.good_list.sort()

    super( DriveManager, self ).__init__( *argv, **argk )


  def setCallBacks( self, reportFail, allreadyDead, allreadyThrashed ):
    self.reportFail = reportFail
    self.allreadyDead = allreadyDead
    self.allreadyThrashed = allreadyThrashed


  def skipUnThrashable( self, evaluator ):
    mount_list = []
    for tmp in [ line.split()[0].replace( '/dev/', '' ) for line in file( '/etc/mtab' ).readlines() if line.startswith( '/dev/' ) ]:
      while tmp[-1] in string.digits:
        tmp = tmp[:-1]
      mount_list.append( tmp )

    for drive in list( self.good_list ):
      if drive.block_name and os.path.exists( '/sys/block/%s/removable' % drive.block_name ) and file( '/sys/block/%s/removable' % drive.block_name ).read().strip() != '0':  # make sure we don't get cd-roms, usb, etc... (for some reason "hot-swap" hardrives are not "removable", Bonus!! )
        self.prune( drive, 'Is Removable', False, True )
        continue

      if drive.block_name and drive.block_path in mount_list:
        self.prune( drive, 'Is Mounted', False, True )
        continue

      if drive.model in NO_THRASH_MODELS:
        self.prune( drive, 'No Thrash Disk', False, True )
        evaluator.mark_thrashed( drive )
        continue

      # for now regular thrashing to SSDs, someday we will have time to rework dtest
      #if drive.isSSD:  # would be nice to still run self test and bonnie
      #  self.prune( drive, 'Is SSD', False, True )
      #  continue

      if drive.isVirtualDisk:  # run bonnie here too
        self.prune( drive, 'Is Virtual Disk', False, True )
        continue


  def skipWithFilesystems( self ):
    tmp_list = [ re.split( ' *', line.strip() )[-1] for line in open( '/proc/partitions', 'r' ).read().splitlines() ]
    partitioned = list( set( [ re.sub( '[0-9]$', '', item ) for item in tmp_list if re.match( '^sd[a-z]+[0-9]$', item ) ] ) )

    for drive in list( self.good_list ):
      if drive.block_name and drive.block_name in partitioned:
        self.prune( drive, 'Has Partitions', False, True )
    """
    if len( partitioned ) > 0:
      tmpBuff = ''

      print 'There are Drives that have parititons, these will be skiped.'
      while tmpBuff != 'Acknowledged':
        print 'Type "Acknowledged" and press <Enter> to Continue...'
        tmpBuff = sys.stdin.readline().strip()
    """
    """
      if we want to check to see if it has a ext2/3/4 (double check) file system:
      for drive in drive_data['good']:
        fd = open( drive.1 )
        fd.seek( 1080 )
        fd.read( 2 ) == 'S\xef':
    """


  def pruneAllreadyDead( self ):
    if self.allreadyDead:
      for drive in list( self.good_list ):
        if self.allreadyDead( drive ):
          self.prune( drive, 'Allready Dead', False )


  def skipAllreadyThrashed( self ):
    if self.allreadyThrashed:
      for drive in list( self.good_list ):
        if self.allreadyThrashed( drive ):
          self.prune( drive, 'Allready Thrashed', False, True )


  def prune( self, drive, reason, report=True, skip=False ):
    try:
      i = self.good_list.index( drive )
      del self.good_list[i]
    except:
      pass

    if skip:
      self.skip_reasons[drive] = reason
    else:
      self.bad_reasons[drive] = reason
      drive.setFault( True )

    if not skip and report and self.reportFail:
       self.reportFail( drive, reason )


  def __getattr__( self, name ):
    if name == 'working_list':
      return list( self.good_list )

    if name == 'skipped_list':
      return self.skip_reasons.keys()

    if name == 'summary':
      return ' '.join( [ '%s(%s)' % ( d.name, d.location ) for d in self.all_list ] )

    if name == 'bad_summary':
      if len( self.bad_reasons ) == 0:
        return '-- No Bad Drives --'

      result = '==== Bad Drives ====\n'
      for drive in self.bad_reasons:
        result = result + '%s-%s(%s)\t%s\n' % ( drive, drive.location, drive.serial, self.bad_reasons[drive] )

      return result

    if name == 'skip_summary':
      if len( self.skip_reasons ) == 0:
        return '-- No Skipped Drives --'

      result = '==== Skipped Drives ====\n'
      for drive in self.skip_reasons:
        result = result + '%s-%s(%s)\t%s\n' % ( drive, drive.location, drive.serial, self.skip_reasons[drive] )

      return result

    raise AttributeError
