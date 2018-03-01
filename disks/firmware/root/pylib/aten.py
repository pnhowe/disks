
from platoclient.libhardware import dmiInfo

from handler import Handler

"""
for ATEN IPMI's, commonly found on Supermicro Motherboards
"""

class AtenTarget( object ):
  def __init__( self, model, version ):
    self.model = model
    self.version = version

  def __str__( self ):
    return 'AtenTarget model: %s' % self.model

  def __repr__( self ):
    return 'AtenTarget model: %s version: %s' % ( self.model, self.version )


class Aten( Handler ):
  def __init__( self, *args, **kwargs ):
    super( Aten, self ).__init__( *args, **kwargs )

  def _ipmiVersion( self ):
    ( rc, lines ) = self._execute( 'aten_getversion', '/bin/ipmitool mc info' )
    if rc != 0:
      return None

    for line in lines:
      if line.startswith( 'Firmware Revision' ):
        return line.split( ':' )[1].strip()

    return None

  def getTargets( self ):
    target = None
    dmi = dmiInfo()

    try:
      board = dmi[ 'Base Board Information' ][0]
    except ( KeyError, IndexError ):
      print 'Unable to get Board Info'
      return None

    version = self._ipmiVersion()
    if version is None:
      print 'Unable to IPMI version'
      return None

    if board[ 'Product Name' ] in ( 'X9DRE-TF+/X9DR7-TF+', ):
      target = AtenTarget( board[ 'Product Name'], version )

    if target:
      return [ ( target, target.model, target.version ) ]
    else:
      return []

  def updateTarget( self, target, filename ):
    return False
    # lUpdate -f fwuperade.bin -i kcs -r y
    # do the update, sleep for 2 min, then allow reboot
"""
[stgr01lab] root@plato{~}: ipmitool mc info
Device ID                 : 32
Device Revision           : 1
Firmware Revision         : 1.35
IPMI Version              : 2.0
Manufacturer ID           : 47488
Manufacturer Name         : Unknown (0xB980)
Product ID                : 43707 (0xaabb)
Product Name              : Unknown (0xAABB)
Device Available          : yes
Provides Device SDRs      : no
Additional Device Support :
    Sensor Device
    SDR Repository Device
    SEL Device
    FRU Inventory Device
    IPMB Event Receiver
    IPMB Event Generator
    Chassis Device
Aux Firmware Rev Info     :
    0x01
    0x00
    0x00
    0x00
"""
