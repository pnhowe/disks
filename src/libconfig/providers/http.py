from libconfig.providers.provider import Provider


class HTTPProvider( Provider ):
  def __init__( self, controller ):
    super().__init__()
    self.config_values = controller.getConfig()
    self.id = self.config_values[ '_structure_id' ]
    self.uuid = self.config_values[ '_structure_config_uuid' ]
    del self.config_values[ '_structure_id' ]
    del self.config_values[ '_structure_config_uuid' ]
