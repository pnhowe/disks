
import re
import os
import time

from ..handler import Handler
from libhardware.libhardware import dmiInfo

# this handler can not be parallelized
PRIORITY = 20
NAME = 'SAA'

BIOS_VERSION_MATCHER = re.compile( r' *BIOS version\.+([a-zA-Z0-9\._\-]+)' )
BMC_VERSION_MATCHER = re.compile( r' *BMC version\.+([a-zA-Z0-9\._\-]+)' )


class SAABIOSTarget( object ):
  def __repr__( self ):
    return 'SAABIOSTarget'


class SAABMCTarget( object ):
  def __repr__( self ):
    return 'SAABMCTarget'


class SAA( Handler ):
  def __init__( self ):
    super().__init__()
    self.model = None
    self.username = os.getenv( 'BMC_USERNAME' )
    self.password = os.getenv( 'BMC_PASSWORD' )

  def _getInterfaceInfo( self ):
    if self.model.startswith( 'X12' ):
      return ''

    if self.model.startswith( 'X13' ):
      return f"-I Redfish_HI -u '{self.username}' -p '{self.password}'"

    return ''

  def _BIOSFlashOptions( self ):
    if self.model.startswith( 'X14' ):
      return '--preserve_nv --preserve_mer'

    return ''

  def getTargets( self ):
    result = []
    baseBoard = dmiInfo()[ 'Base Board Information' ][0]
    if baseBoard[ 'Manufacturer' ] != 'Supermicro':
      return result

    self.model = baseBoard[ 'Product Name' ]
    interface = self._getInterfaceInfo()

    ( rc, line_list ) = self._execute( 'saa_bios_getinfo', f'saa {interface} -c GetBiosInfo' )
    if rc != 0:
      raise Exception( 'Error getting BIOS information')
    for line in line_list:
      match = BIOS_VERSION_MATCHER.match( line )
      if match:
        result.append( ( SAABIOSTarget(), f'BIOS_{self.model}', match.group( 1 ) ) )

    ( rc, line_list ) = self._execute( 'saa_bmc_getinfo', f'saa {interface} -c GetBmcInfo' )
    if rc != 0:
      raise Exception( 'Error getting BMC information')
    for line in line_list:
      match = BMC_VERSION_MATCHER.match( line )
      if match:
        result.append( ( SAABMCTarget(), f'BMC_{self.model}', match.group( 1 ) ) )

    return result

  def updateTarget( self, target, filename ):
    interface = self._getInterfaceInfo()

    if isinstance( target, SAABIOSTarget ):
      ( rc, _ ) = self._execute( 'saa_bios_flash', f'saa {interface} -c UpdateBios --preserve_setting {self._BIOSFlashOptions()} --staged 1 --file {filename}' )
      if rc != 0:
        print( 'Error updating BIOS' )
        return False

      self._execute( 'reboot', 'reboot' )
      # we need to reboot the system load the new BIOS, so start the restart and wait forever for it to happen
      time.sleep( 600 )  # 10 min should be enough, other wise this will fail out
      print( 'Failed to reset' )
      return False

    if isinstance( target, SAABMCTarget ):
      ( rc, _ ) = self._execute( 'saa_bmc_flash', f'saa {interface} -c UpdateBmc --file {filename}' )
      # the BMC should of reset before the command returns
      return rc == 0

    raise Exception( 'Unknown Target Type' )
