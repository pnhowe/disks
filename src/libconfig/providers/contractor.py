from dateutil import parser as datetimeparser
from libconfig.providers.provider import Provider


class ContractorProvider( Provider ):
  def __init__( self, contractor ):
    super().__init__()
    self.contractor = contractor

  def getValues( self, config_uuid ):
    result = self.contractor.getConfig( config_uuid )
    self.uuid = result[ '_structure_config_uuid' ]
    self.last_modified = datetimeparser.parse( result[ '__last_modified' ] )

    return result
