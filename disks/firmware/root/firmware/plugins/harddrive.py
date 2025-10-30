
from ..handler import Handler
from libdrive.libdrive import DriveManager

PRIORITY = 100
NAME = 'Harddrive'


class Harddrive( Handler ):
  def __init__( self, *args, **kwargs ):
    self.dm = DriveManager()

  def getTargets( self ):
    result = []
    for drive in self.dm.drive_list:
      result.append( ( drive, drive.model, drive.version ) )

    return result

  def updateTarget( self, target, filename ):
    return False
    ( rc, _ ) = self._execute( f'harddrive_update_{target.serial}', f'/sbin/hdparm --yes-i-know-what-i-am-doing --please-destroy-my-drive --fwdownload {filename} {target.blockdev}' )

    return rc == 0
