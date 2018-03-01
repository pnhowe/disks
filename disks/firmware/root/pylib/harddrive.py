
from handler import Handler
from platoclient.libdrive import DriveManager

class Harddrive( Handler ):
  def __init__( self, *args, **kwargs ):
    super( Harddrive, self ).__init__( *args, **kwargs )
    self.dm = DriveManager()

  def getTargets( self ):
    result = []
    for drive in self.dm.drive_list:
      result.append( ( drive, drive.model, drive.version ) )

    return result

  def updateTarget( self, target, filename ):
    return False
    ( rc, _ ) = self._execute( 'harddrive_update_%s' % target.serial, '/sbin/hdparm --yes-i-know-what-i-am-doing --please-destroy-my-drive --fwdownload %s %s' ( filename, target.blockdev ) )

    return rc == 0
