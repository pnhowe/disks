import os
import configparser
from io import StringIO
from libconfig.libconfig import Config
from libconfig.jinja2 import FileSystemLoader, Environment
from libconfig.providers import FileProvider, HTTPProvider
from controller.client import getClient

config = None
template = None

config_values = { '_installer': {} }


def initConfig( install_root, template_path, profile_path ):
  global config, template

  if os.access( '/config_values.json', os.R_OK ):  # TODO: when standalone libconfig is figured out, it will probably need a built in controller Client, that could be started from the Client in the bootdisks, at that point get the static, file, http sources unified there and here.  Also need to allow top level do_task to send down hints
    provider = FileProvider( '/config_values.json' )

  else:
    provider = HTTPProvider( getClient() )

  config = Config( provider, template_path, '/tmp/config.db', install_root, 'linux-installer' )
  # plato.allow_config_change = True
  config.updateConfigCacheFromMaster()
  # plato.allow_config_change = False

  env = Environment( loader=FileSystemLoader( os.path.dirname( profile_path ) ) )
  template = env.get_template( os.path.basename( profile_path ) )


def getProfile():
  values = config.getConfigCache()
  values.update( config.extra_values )
  configfp = StringIO( template.render( values ) )
  profile = configparser.RawConfigParser()
  try:
    profile.readfp( configfp )

  except configparser.Error as e:
    raise Exception( 'Error Parsing distro profile: "{0}"'.format( e ) )

  return profile


def writeShellHelper():
  config_data = open( '/target/config_data', 'w' )
  cache = config.getConfigCache()
  cache.update( config.extra_values )
  for value in cache:
    config_data.write( '{0}=\'{1}\'\n'.format( value, str( cache[ value ] ).replace( "'", "'\"'\"'" ) ) )  # TODO: use shlex.quote(s) when it's aviable
  config_data.close()


def updateConfig( category, newvals ):
  global config_values
  config_values[ '_installer' ][ category ] = newvals

  config.setOverlayValues( config_values )


def renderTemplates( template_list ):
  print( 'Rendering templates "{0}"...'.format( ','.join( template_list ) ) )
  template_list = [ i.strip() for i in template_list ]
  config.configPackage( '', template_list, True, False, True, True )


def baseConfig( profile ):
  renderTemplates( profile.get( 'config', 'base_templates' ).split( ',' ) )


def fullConfig():
  template_list = config.getTemplateList( '' )
  renderTemplates( template_list )
