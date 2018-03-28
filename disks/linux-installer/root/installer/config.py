import os
import json
import ConfigParser
import StringIO
from platoclient.libconfig import Config
from platoclient.libplato import Plato
from platoclient.jinja2 import FileSystemLoader, Environment

config = None
template = None

config_values = { '_installer': {} }

def initConfig( install_root, template_path, profile_path ):
  global config, template

  if os.access( '/config_values.json', os.R_OK ):
    plato = Plato( host='file:///config_values.json' )

  else:
    plato = Plato( host=os.environ.get( 'plato_host', 'http://plato' ), proxy=os.environ.get( 'plato_proxy', None ) )

  config = Config( plato, template_path, '/tmp/config.db', install_root, 'linux-installer' )
  plato.allow_config_change = True
  config.updateConfigCacheFromMaster()
  plato.allow_config_change = True

  env = Environment( loader=FileSystemLoader( os.path.dirname( profile_path ) ) )
  template = env.get_template( os.path.basename( profile_path ) )


def getProfile():
  values = config.getConfigCache()
  values.update( config.extra_values )
  configfp = StringIO.StringIO( template.render( values ) )
  profile = ConfigParser.RawConfigParser()
  try:
    profile.readfp( configfp )

  except ConfigParser.Error, e:
    raise Exception( 'Error Parsing distro profile: %s' % e )

  return profile


def writeShellHelper():
  config_data = open( '/target/config_data', 'w' )
  cache = config.getConfigCache()
  cache.update( config.extra_values )
  for value in cache:
    config_data.write( '%s=\'%s\'\n' % ( value, str( cache[ value ] ).replace("'", "'\"'\"'") ) ) # TODO: use shlex.quote(s) when it's aviable
  config_data.close()


def updateConfig( category, newvals ):
  global config_values
  config_values[ '_installer' ][ category ] = newvals

  config.setOverlayValues( config_values )


def renderTemplates( template_list ):
  print 'Rendering templates "%s"...' % ','.join( template_list )
  template_list = [ i.strip() for i in template_list ]
  config.configPackage( '', template_list, True, False, True, True )


def baseConfig( profile ):
  renderTemplates( profile.get( 'config', 'base_templates' ).split( ',' ) )


def fullConfig():
  template_list = config.getTemplateList( '' )
  renderTemplates( template_list )
