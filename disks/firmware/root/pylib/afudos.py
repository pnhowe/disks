import os
import time

from platoclient.libhardware import dmiInfo

from handler import Handler

"""
afudos is a DOS cli for updating the firmware in American Megatrends BIOSes
"""

class AfudosTarget( object ):
  def __init__( self, model, version ):
    self.model = model
    self.version = version

  def __str__( self ):
    return 'AfudosTarget model: %s' % self.model

  def __repr__( self ):
    return 'AfudosTarget model: %s version: %s' % ( self.model, self.version )


class Afudos( Handler ):
  def __init__( self, *args, **kwargs ):
    super( Afudos, self ).__init__( *args, **kwargs )

  def getTargets( self ):
    dmi = dmiInfo()

    try:
      bios = dmi[ 'BIOS Info' ][0]
    except ( KeyError, IndexError ):
      print 'Unable to get BIOS Info'
      return None

    try:
      board = dmi[ 'Base Board Information' ][0]
    except ( KeyError, IndexError ):
      print 'Unable to get Board Info'
      return None

    if bios[ 'Vendor' ] != 'American Megatrends Inc.':
      return []

    target = AfudosTarget( board[ 'Product Name' ], bios[ 'Version' ] )

    return [ ( target, target.model, target.version ) ]

  def updateTarget( self, target, filename ):
    self._prep_wrk_dir()

    filename_list = []
    filename_list.append( filename )

    cmd = """
echo "Updating..."

echo "afudos.exe  %s  /P  /B  /N  /K  /R /FDT /MER /OPR"

echo "Done."
""" % ( os.path.basename( filename ) )

    filename_list.append( '/resources/afudos.exe' )

    line_list = self._execute_quemu( 'afudos_update', cmd, filename_list, None, None, None )
    if line_list is None:
      return False

    return False
    #time.sleep( 30 )
    #self._execute( 'ipmi_cycle', [ '/bin/ipmitool', 'power', 'cycle' ] )

    #return True
