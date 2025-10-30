
from ..handler import Handler
from libhardware.libhardware import pciInfo

PRIORITY = 80
NAME = 'Mellanox'


class Mellanox( Handler ):
  def __init__( self ):
    super().__init__()

  def getTargets( self ):
    result = []
    for address, entry in pciInfo().items():
      if not address.endswith( '.00' ):  # we don't want to deal with the sub-devices, they share with the primary
        continue

      if entry[ 'vendor' ] == 5555:  # 0x15B3
        ( rc, line_list ) = self._execute( f'mstflint_query_{address}', f'mstflint -d {address} q' )
        if rc != 0:
          raise Exception( 'Error getting info from Mellanox card at "{address}"')

        value_map = {}
        for line in line_list:
          try:
            ( key, value ) = line.split( ':' )
            value_map[ key.strip() ] = value.strip()
          except ValueError:
            continue

        result.append( ( address, value_map[ 'PSID' ], value_map[ 'FW Version' ] ) )

    return result

  def updateTarget( self, target, filename ):
    ( rc, _ ) = self._execute( f'mstflint_flash_{target}', f'mstflint -d {target} -i {filename} -y burn' )
    if rc != 0:
      return False

    ( rc, _ ) = self._execute( f'mstfwreset_{target}', f'mstfwreset -d {target} reset' )
    return rc == 0
