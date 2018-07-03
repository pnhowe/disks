import json
from libconfig.providers.provider import Provider


class FileProvider( Provider ):
  def __init__( self, filepath ):
    super().__init__()
    self.config_values = json.loads( open( filepath, 'r' ).read() )
    self.id = self.config_values[ 'id' ]
    self.uuid = self.config_values[ 'uuid' ]
    del self.config_values[ 'id' ]
    del self.config_values[ 'uuid' ]
