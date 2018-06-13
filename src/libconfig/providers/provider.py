
class Provider():
  def __init__( self ):
    super().__init__()
    self.config_values = {}
    self.id = None
    self.uuid = None

  def getConfig( self ):
    return self.config_values
