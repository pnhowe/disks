import os
import importlib

# Handler Priorities
# 20s - BIOS
# 40s - BMC/IPMI
# 60s - HBA/RAID
# 80s - NIC/Infiniband
# 100s - Harddrives/SSD/NVME


class Manager( object ):
  def __init__( self ):
    self.handler_module_map = {}
    self.handler_priority_map = {}
    plugin_dir = os.path.join( os.path.dirname( __file__ ), 'plugins' )
    for item in os.scandir( plugin_dir ):
      if not item.is_file() or item.name == '__init__.py':
        continue

      handler_name = os.path.splitext( os.path.basename( item.name ) )[0]
      module = importlib.import_module( f'firmware.plugins.{handler_name}' )
      self.handler_priority_map[ handler_name ] = module.PRIORITY
      self.handler_module_map[ handler_name ] = module

  def getHandlers( self ):
    result = []
    name_list = [ i[0] for i in sorted( self.handler_priority_map.items(), key=lambda x: x[1] ) ]
    for name in name_list:
      module = self.handler_module_map[ name ]
      result.append( ( name, getattr( module, module.NAME ) ) )

    return result
