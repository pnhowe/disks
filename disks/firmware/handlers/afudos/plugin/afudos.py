import os

from libhardware.libhardware import dmiInfo

from ..handler import Handler

"""
afudos is a DOS cli for updating the firmware in American Megatrends BIOSes
"""
PRIORITY = 20
NAME = 'AfuDOS'


class AfudosTarget( object ):
  def __init__( self, model, version ):
    self.model = model
    self.version = version

  def __str__( self ):
    return f'AfudosTarget model: "{self.model}"'

  def __repr__( self ):
    return f'AfudosTarget model: "{self.model}" version: "{self.version}"'


class Afudos( Handler ):
  def __init__( self ):
    super().__init__()

  def getTargets( self ):
    dmi = dmiInfo()

    try:
      bios = dmi[ 'BIOS Info' ][0]
    except ( KeyError, IndexError ):
      raise Exception( 'Unable to get BIOS Info' )

    try:
      board = dmi[ 'Base Board Information' ][0]
    except ( KeyError, IndexError ):
      raise Exception( 'Unable to get Board Info' )

    if bios[ 'Vendor' ] != 'American Megatrends Inc.':
      return []

    target = AfudosTarget( board[ 'Product Name' ], bios[ 'Version' ] )

    return [ ( target, target.model, target.version ) ]

  def updateTarget( self, target, filename ):
    self._prep_wrk_dir()

    filename_list = []
    filename_list.append( filename )

    cmd = f"""
echo "Updating..."

echo "afudos.exe {os.path.basename( filename )} /P /B /N /K /R /FDT /MER /OPR"

echo "Done."
"""

    filename_list.append( '/resources/afudos.exe' )

    line_list = self._execute_quemu( 'afudos_update', cmd, filename_list, None, None, None )
    if line_list is None:
      return False

    return True
    # time.sleep( 30 )
    # self._execute( 'ipmi_cycle', [ '/bin/ipmitool', 'power', 'cycle' ] )

    # return True
