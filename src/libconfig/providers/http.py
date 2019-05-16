from dateutil import parser as datetimeparser
from libconfig.providers.provider import Provider


class HTTPProvider( Provider ):
  def __init__( self, controller ):
    super().__init__()
    self.controller = controller

  def getValues( self, config_uuid ):
    result = self.controller.getConfig( config_uuid )
    self.uuid = result[ '_structure_config_uuid' ]
    self.last_modified = datetimeparser.parse( result[ '__last_modified' ] )

    return result
