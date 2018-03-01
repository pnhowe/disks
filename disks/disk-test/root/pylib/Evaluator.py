import sys
import json

from platopxe.libplatopxe import PlatoPXE
from platoclient.libplato import Plato404Exception

# { attrib id, ( 'desc', value max for "bad", rate (value at check - drive first spin up value) max for "bad" ) }
SMART_ATTRIBS = {
  'Seagate': { # 1, 7, 195 ignored, segate seem to operate normally with big numbers there
    #5:   ( 'Relocated Sector Count',      10,    5 ),  # the raw value and the normalized value don't move in the same direction, have to ignore for now
    10:  ( 'Spin Retry Count',             5,    1 ),
    184: ( 'End-to-End error Count',       2,    1 ),
    #188: ( 'Command Timeout Cont (Potential power/Comm problems)', 2, 1 ),
    193: ( 'Load Cycle Count',        400000,   60 ),  # double check this is here, count all the operations on the drive and add a few
    194: ( 'Temperature',                 45,   30 ),
    197: ( 'Current Pending Sector Count', 2,    2 ),
    198: ( 'Uncorrectable Sctor Count',   15,    10 ),
    },
  'Western Digitial' : {
    1:   ( 'Read Error Rate',           1000,  500 ),
    5:   ( 'Relocated Sector Count',      10,    5 ),
    7:   ( 'Seek Error Rate',           1000,  500 ),
    10:  ( 'Spin Retry Count',             1,    1 ),
    11:  ( 'Calibration Retry Count',      1,    1 ),
    193: ( 'Load Cycle Count',        400000,   60 ),
    194: ( 'Temperature',                 45,   30 ),
    196: ( 'Relocate Event Count',        10,    5 ),
    197: ( 'Current Pending Sector Count', 2,    2 ),
    198: ( 'Uncorrectable Sctor Count',   10,    5 ),
    200: ( 'Multi Zone Error Rate',       15,    10 ),
    },
  'Hitachi' : {
    5:   ( 'Relocated Sector Count',      10,    5 ),
    10:  ( 'Spin Retry Count',             1,    1 ),
    193: ( 'Load Cycle Count',        400000,   60 ),
    194: ( 'Temperature',                 45,   30 ),
    196: ( 'Relocate Event Count',        10,    5 ),
    197: ( 'Current Pending Sector Count', 2,    2 ),
    198: ( 'Uncorrectable Sctor Count',   10,    5 ),
    },
  'SATADOM': {
    198: ( 'Program Fail Count Chip',     20,    0 ),
    },
  'Unknown' : {
    1:   ( 'Read Error Rate',           2000, 1000 ),
    5:   ( 'Relocated Sector Count',      10,    5 ),
    7:   ( 'Seek Error Rate',           2000, 1000 ),
    10:  ( 'Spin Retry Count',             1,    1 ),
    11:  ( 'Calibration Retry Count',      1,    1 ),
    184: ( 'End-to-End error Count',       2,    1 ),
    188: ( 'Command Timeout Cont (Potential power/Comm problems)', 2, 1 ),
    193: ( 'Load Cycle Count',        400000,   60 ), # suposed suposed to last to 600,000(according to some spec) and it shoudn't be parking much during the test
    194: ( 'Temperature',                 45,   30 ), # not susposed to go over 50 (the spec saies 60?) we want to leave some wiggle room incase things happen
    195: ( 'Hardware ECC Recovered',    2000, 1000 ),
    196: ( 'Relocate Event Count',        10,    5 ),
    197: ( 'Current Pending Sector Count', 2,    2 ),
    198: ( 'Uncorrectable Sctor Count',   10,    5 ),
    200: ( 'Multi Zone / Write Error Rate',15,   10 ),
    201: ( 'Soft Read Error Rate',      2000, 1000 ),
    221: ( 'G-Sense Error Rate',          10,    5 ),
    223: ( 'Load/Unload Retry Count',     10,    5 ),
    225: ( 'Load/Unload Cycle Count', 300000,   40 ),
    },
}

def getType( model ):  # keep up to date with getType in plato-master/plato/Device/DriveUtils.py  and getType in Evaluator
  model = model.lower()
  if model.startswith( 'st' ):
    return 'Seagate'

  if model.startswith( 'wd' ):
    return 'Western Digitial'

  if model.startswith( 'hd' ) or model.startswith( 'samsung' ):
    return 'Samsung'

  if model.startswith( 'hu' ) or model.startswith( 'hitachi' ):
    return 'Hitachi'

  if model.startswith( 'intel ssd' ):
    return 'Intel SSD'

  if model.startswith( 'satadom' ):
    return 'SATADOM'

  return 'Unknown'

class Evaluator( object ):
  def __init__( self, drive_manager, *argv, **argk ):
    self.drive_manager = drive_manager
    self.drive_manager.setCallBacks( self.reportFail, self.allreadyDead, self.allreadyThrashed )

    super( Evaluator, self ).__init__( *argv, **argk )

  def reportFail( self, drive, reason ):
    pass

  def allreadyDead( self, drive ):
    return False

  def allreadyThrashed( self, drive ):
    return False

  def store( self, drive ):
    pass

  def mark_thrashed( self, drive ):
    pass

  def evaluate_all( self, operation ):
    print 'Smart Scan...'
    for drive in self.drive_manager.working_list:
      print '%s' % drive.name,
      sys.stdout.flush()

      self.evaluate( drive, operation )

    print

  def mark_good_thrashed( self ):
    print 'Marking Good Drives as Thrashed...'
    for drive in self.drive_manager.working_list:
      print '%s' % drive.name,
      sys.stdout.flush()

      self.mark_thrashed( drive )

    print

  def store_good( self ):
    print 'Storing Good Drives...'
    for drive in self.drive_manager.working_list:
      print '%s' % drive.name,
      sys.stdout.flush()

      self.store( drive )

    print

  def store_skipped( self ):
    print 'Storing Skipped Drives...'
    for drive in self.drive_manager.skipped_list:
      print '%s' % drive.name,
      sys.stdout.flush()

      try:
        self.store( drive )
      except Plato404Exception: # some skipped disks aren't on plato
        pass

    print

class LocalEvaluator( Evaluator ):
  def __init__( self, *argv, **argk ):
    self.initial = {}
    self.last = {}
    super( LocalEvaluator, self ).__init__( *argv, **argk )

  def evaluate( self, drive, operation ):
    try:
      tmp_list = drive.smartAttrs()
      attrib_list = {}
      for attr in tmp_list:
        attrib_list[attr] = tmp_list[attr][3]
    except Exception as e:
      self.drive_manager.prune( drive, 'Exception "%s" while getting SMART attrs for "%s"' % ( e, drive.name ) )
      return

    if drive not in self.initial:
      self.initial[drive] = attrib_list

    drive_type = getType( drive.model )

    for attrib in attrib_list:
      if attrib not in SMART_ATTRIBS[drive_type]:
        continue

      if attrib_list[attrib] > SMART_ATTRIBS[drive_type][attrib][1]:
        self.drive_manager.prune( drive, '%s: smart Attrib (%d)"%s" value "%d" above threshold of "%d"' % ( operation, attrib, SMART_ATTRIBS[drive_type][attrib][0], attrib_list[attrib], SMART_ATTRIBS[drive_type][attrib][1] ), False )
        return

      if attrib in self.initial[drive]: # for some reason not all smart scans get all the attribs, this prevents the script from dying in thoes cases
        rate = attrib_list[attrib] - self.initial[drive][attrib]
        if rate > SMART_ATTRIBS[drive_type][attrib][2]:
          self.drive_manager.prune( drive, '%s: smart Attrib (%d)"%s" rate "%d" above rate of "%d"' % ( operation, attrib, SMART_ATTRIBS[drive_type][attrib][0], rate, SMART_ATTRIBS[drive_type][attrib][2] ), False )
          return

class PlatoDriveTest( PlatoPXE ):
  def __init__( self, *args, **kwargs ):
    super( PlatoDriveTest, self ).__init__( *args, **kwargs )

  def removeDrive( self, port=None, store=False ):
    if port is None:
      POSTData = { 'data': json.dumps( { 'location': '-ALL-', 'store': store } ) }
    else:
      POSTData = { 'data': json.dumps( { 'location': port.location, 'store': store } ) }

    result = self.postRequest( 'storage/removedrive/', {}, POSTData, retry_count=40 )

    if result != 'Saved':
      raise Exception( 'Error Removing Drive From Machine: %s' % result )

  def markThrashed( self, drive ): # move the libplatoext under dtest
    result = self.getRequest( 'storage/markdrivethrashed/%s/' % drive.serial, {}, retry_count=40 )

    if result != 'Saved':
      raise Exception( 'Error Marking Drive Thrashed: %s' % result )


class ReportEvaluator( Evaluator ):
  def __init__( self, plato, *argv, **kwargs ):
    self.plato = plato
    plato.__class__ = PlatoDriveTest
    self.plato.allow_config_change = True
    self.plato.getConfig()
    self.plato.allow_config_change = False
    self.plato.removeDrive()  #detatches all disks current attached to this device
    super( ReportEvaluator, self ).__init__( *argv, **kwargs )

  def reportFail( self, drive, reason, *argv, **kwargs ):
    drive_status = {}
    drive_status['host_flag_bad'] = '%s: "%s"' % ( reason, drive.name )

    self.plato.reportDriveStatus( drive, 'Drive Test Fail: "%s"' % reason, drive_status )
    super( ReportEvaluator, self ).reportFail( drive, reason, *argv, **kwargs )

  def allreadyDead( self, drive ):
    stat = self.plato.checkDriveStatus( drive ) # none = not found, true = good, false = bad

    if stat[0] or ( stat[0] is None ):
      return False
    else:
      return True

  def allreadyThrashed( self, drive ):
    return bool( self.plato.checkDriveThrashed( drive ) ) # none = not found, true = Thrashed, false = not Thrashed

  def mark_thrashed( self, drive ):
    self.plato.markThrashed( drive )

  def store( self, drive ):
    self.plato.removeDrive( port=drive.port, store=True )

  def evaluate( self, drive, operation ):
    try:
      drive_status = drive.driveReportingStatus()
    except Exception as e:
      self.drive_manager.prune( drive, 'Exception "%s" getting Disk info after "%s"' % ( e, operation ) ) # needs to report, having trouble doing normal reporting
      #don't care about result, it's dead jim
      return
    status = self.plato.reportDriveStatus( drive, 'Drive Test: "%s"' % operation, drive_status )
    if not status[0]:
      self.drive_manager.prune( drive, 'Plato Reports as bad, Health: %s' % status[1], False )

def getEvaluator( drivemanager, plato=None ):
  if plato:
    return ReportEvaluator( drive_manager=drivemanager, plato=plato )
  else:
    return LocalEvaluator( drive_manager=drivemanager )
