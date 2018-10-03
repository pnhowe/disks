import json
from libconfig.providers.provider import Provider


class FileProvider( Provider ):
  def __init__( self, filepath ):
    super().__init__()
    self.config_values = json.loads( open( filepath, 'r' ).read() )
    try:
      self.id = self.config_values[ '_structure_id' ]
      self.uuid = self.config_values[ '_structure_config_uuid' ]

    except KeyError:
      self.id = self.config_values[ 'id' ]
      self.uuid = self.config_values[ 'uuid' ]
      self.config_values[ '_structure_id' ] = self.id  # There are some templates that refere to _structure_id, for now hack this so they work
      self.config_values[ '_structure_config_uuid' ] = self.uuid
