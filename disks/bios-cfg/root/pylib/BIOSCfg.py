class BIOSCfg( object ):
  def __init__( self, *args, **kwargs ):
    self.password = None

  def setPassword( self, password ):
    self.password = password
