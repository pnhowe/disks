from datetime import datetime


class Provider():
  def __init__( self ):
    super().__init__()
    self.uuid = None
    self.last_modified = datetime.min

  def getValues( self, config_uuid ):
    return {}
