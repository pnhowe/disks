import json
import os
from datetime import datetime
from libconfig.providers.provider import Provider


class FileProvider( Provider ):
  def __init__( self, filepath ):
    super().__init__()
    self.filepath = filepath

  def getValues( self, config_uuid ):
    result = json.loads( open( self.filepath, 'r' ).read() )
    self.uuid = result[ 'uuid' ]
    self.last_modified = datetime.fromtimestamp( os.path.getmtime( self.filepath ) )

    return result
