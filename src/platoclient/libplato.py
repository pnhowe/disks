import urllib
import urllib2
import httplib
import socket
import time
import logging
import random
import math
import os
import ConfigParser
from datetime import datetime
try:
  import json
except ImportError:
  import simplejson
  json = simplejson

__VERSION__ = "0.19"

DELAY_MULTIPLIER = 15
# delay of 15 results in a delay of:
# min delay = 0, 10, 16, 20, 24, 26, 29, 31, 32, 34, 35, 37, 38, 39, 40 ....
# max delay = 0, 20, 32, 40, 48, 52, 58, 62, 64, 68, 70, 74, 76, 78, 80 .....

class DeviceNotFound( Exception ):
  pass

class PlatoException( Exception ):
  pass

class PlatoConnectionException( Exception ):
  pass

class PlatoTimeoutException( Exception ):
  pass

class Plato404Exception( Exception ):
  pass

class PlatoServerErrorException( Exception ):
  pass

class PlatoJSONException( Exception ):
  pass

class PlatoConfigChanged( Exception ):
  pass


def _backOffDelay( count ):
  if count < 1: # math.log dosen't do so well below 1
    count = 1

  factor = int( DELAY_MULTIPLIER * math.log( count ) )
  time.sleep( factor + ( random.random() * factor ) )

class Connector( object ):
  def getConfig( self ):
    return {}

  def getRequest( self, path, parms, timeout=30, retry_count=10 ):
    return None

  def getJSONRequest( self, path, parms, timeout=30, retry_count=10 ):
    return None

  def postRequest( self, path, parms, POSTData, timeout=30, retry_count=10 ):
    return None

  def postJSONRequest( self, path, parms, POSTData, timeout=30, retry_count=10 ):
    return None

class HTTPConnector( Connector ):
  def __init__( self, host, proxy ):
    if host.startswith( ( 'http://', 'https://' ) ):
      self.host = host
    else:
      self.host = 'http://{0}'.format( host )

    if proxy:
      logging.debug( 'libplato: setting proxy to {0}'.format( proxy ) )
      self.opener = urllib2.build_opener( urllib2.ProxyHandler( { 'http': proxy, 'https': proxy } ) )
    else:
      self.opener = urllib2.build_opener( urllib2.ProxyHandler( {} ) ) # no proxying, not matter what is in the enviornment

    self.opener.addheaders = [ ( 'User-agent', 'libplato {0}'.format( __VERSION__ ) ] )
    #TODO: would be cool to set the 'Referrer' to the function that made the request... ie the first function of the Plato object or it's decendants

  def _buildURL( self, path, parms ):
    url = '{0}/{0}'.format( self.host, path )

    if parms:
      url = '{0}?{0}'.format( url, urllib.urlencode( parms ) )

    logging.debug( 'libplato: url: "{0}"'.format( url ) )
    return url

  def _getRequest( self, path, parms, timeout ):
    try:
      resp = self.opener.open( self._buildURL( path, parms ), timeout=timeout )

    except urllib2.HTTPError, e:
      if e.code == 404:
        raise Plato404Exception()

      if e.code == 500:
        raise PlatoServerErrorException()

      raise PlatoConnectionException( 'HTTPError Sending Request({0}), "{1}"'.format( e.code, e.reason ) )

    except urllib2.URLError, e:
      if isinstance( e.reason, socket.timeout ):
        raise PlatoTimeoutException( 'Request Timeout after {0} seconds.'.format( timeout ) )

      raise PlatoConnectionException( 'URLError Sending Request, "{0}"'.format( e.reason ) )

    except httplib.HTTPException, e:
      raise PlatoConnectionException( 'HTTPException Sending Request, "{0}"'.format( e.message ))

    except socket.error, e:
      raise PlatoConnectionException( 'Socket Error Sending Request, errno: {0}, "{1}"'.format( e.errno, e.message ) )

    if resp.code != 200:
      raise PlatoException( 'Error with request, HTTP Error {0}'.format( resp.status ) )

    result = resp.read()
    resp.close()
    return result

  def getRequest( self, path, parms, timeout=30, retry_count=10 ): # set retry_count = -1 for never give up, never surender
    count = 0
    while True:
      logging.debug( 'libplato: getRequest: retry {0} of {1}, timeout: {2}'.format( count, retry_count, timeout ) )
      try:
        return self._getRequest( path, parms, timeout )
      except ( PlatoConnectionException, PlatoTimeoutException ), e:
        if not retry_count == -1 and count >= retry_count:
          raise e

      count += 1
      _backOffDelay( count )

  def getJSONRequest( self, path, parms, timeout=30, retry_count=10 ):
    result = self.getRequest( path, parms, timeout, retry_count )
    try:
      return json.loads( result )
    except TypeError:
      raise PlatoJSONException( 'Error Parsing "{0}"'.format( str( result )[:80] ) )

  def _postRequest( self, path, parms, POSTData, timeout=30 ):
    try:
      resp = self.opener.open( self._buildURL( path, parms ), data=urllib.urlencode( POSTData ), timeout=timeout )

    except urllib2.HTTPError, e:
      if e.code == 404:
        raise Plato404Exception()

      if e.code == 500:
        raise PlatoServerErrorException()

      raise PlatoConnectionException( 'HTTPError Sending Request({0}), "{1}"'.format( e.code, e.reason ) )

    except urllib2.URLError, e:
      if isinstance( e.reason, socket.timeout ):
        raise PlatoTimeoutException( 'Request Timeout after {0} seconds.'.format( timeout ) )

      raise PlatoConnectionException( 'URLError Sending Request, "{0}"'.format( e.reason ) )

    if resp.code != 200:
      raise PlatoException( 'Error with request, HTTP Error {0}'.format( resp.status ) )

    result = resp.read()
    resp.close()
    return result

  def postRequest( self, path, parms, POSTData, timeout=30, retry_count=10 ): # see getRequest
    count = 0
    while True:
      logging.debug( 'libplato: postRequest: retry {0} of {1}, timeout: {2}'.format( count, retry_count, timeout ) )
      try:
        return self._postRequest( path, parms, POSTData, timeout )
      except ( PlatoConnectionException, PlatoTimeoutException ), e:
        if not retry_count == -1 and count >= retry_count:
          raise e

      count += 1
      _backOffDelay( count )

  def postJSONRequest( self, path, parms, POSTData, timeout=30, retry_count=10 ):
    result = self.postRequest( path, parms, POSTData, timeout, retry_count )
    try:
      return json.loads( result )
    except TypeError:
      raise PlatoJSONException( 'Error Parsing "{0}"'.format( str( result )[:80] ) )

  def getConfig( self, config_uuid=None, config_id=None ):
    parms = {}
    if config_uuid is not None:
      parms[ 'config_uuid' ] = config_uuid
    elif config_id is not None:
      parms[ 'config_id' ] = config_id

    try:
      result = self.getJSONRequest( 'config/system_config', parms, timeout=10, retry_count=2 ) # the defaults cause configManager to hang to a long time when it can't talk to plato
    except Plato404Exception:
      raise DeviceNotFound()

    return result


# Pulls config from local json file instead of going upstream
# does not support everything Plato does
class LocalFileConnector( Connector ):
  def __init__( self, config_file ):
    self.config = json.loads( open( config_file, 'r' ).read() )
    self.config[ 'last_modified' ] = datetime.fromtimestamp( os.path.getctime( config_file ) ).strftime( '%Y-%m-%d %H:%M:%S' )

  def getConfig( self, config_uuid=None, config_id=None ):
    return self.config.copy()


class StaticConfigConnector( Connector ):
  def __init__( self, config_values ):
    self.config = config_values

  def getConfig( self, config_uuid=None, config_id=None ):
    return self.config.copy()


class Plato( object ):
  def __init__( self, config_file=None, host=None, proxy=None, static_config=None ):
    if config_file is None and host is None and static_config is None:
      raise Exception( 'Plato Constructor requires either config_file or host' )

    # for host=file:// and config_values targets, these will be None until config_file is built
    # or the normall allow_config_change/getConfig piviot is done
    self.uuid = None
    self.id = None
    self.pod = None
    self.allow_config_change = False

    if static_config is not None:
      self._connector = StaticConfigConnector( static_config )
      return

    if config_file:
      try:
        host = config_file.get( 'plato', 'hostname' )
      except ConfigParser.Error:
        raise Exception( 'Error Getting plato host from config file.' )

      try:
        proxy = config_file.get( 'plato', 'proxy' )
        if not proxy:
          proxy = None
      except ConfigParser.Error:
        logging.warning( 'libplato: No proxy setting found, running without proxy support.' )
        proxy = None

      try:
        self.id = int( config_file.get( 'plato', 'id' ) )
      except ValueError:
        raise Exception( 'Invalid config id' )
      except ConfigParser.Error:
        self.id = None
        logging.warning( 'libplato: No Config Id setting found, relying on Plato Master to figure it out.' )

      try:
        self.uuid = config_file.get( 'plato', 'config_uuid' )
      except ConfigParser.Error:
        self.uuid = None
        logging.warning( 'libplato: No UUID setting found, relying on Plato Master to figure it out.' )

      try:
        self.pod = config_file.get( 'plato', 'pod' )
      except ConfigParser.Error:
        self.pod = None
        logging.warning( 'libplato: No Pod setting found, relying on Plato Master to figure it out.' )

    else:  # if host/proxy is specified in the file, it overrides what we get passed in, if host needs to change, re-gen file with new values
      host = host
      proxy = proxy

    if host.startswith( 'file://' ):
      self._connector = LocalFileConnector( host[ 7: ] )
    else:
      self._connector = HTTPConnector( host, proxy )

  def getConfig( self ):
    result = self._connector.getConfig( config_uuid=self.uuid, config_id=self.id )

    if 'config_uuid' in result:
      if result[ 'config_uuid' ] != self.uuid: #this needs to be re-thought a bit, perhaps if exisint self.uuid is non allow quetly?
        if not self.allow_config_change:
          raise PlatoConfigChanged( 'Config UUID Changed' )
        self.uuid = result[ 'config_uuid' ]

    else:
      self.uuid = None

    if 'id' in result:
      if int( result[ 'id' ] ) != self.id:
        if not self.allow_config_change:
          raise PlatoConfigChanged( 'Config ID Changed' )

        self.id = int( result[ 'id' ] )

    else:
      self.id = 0

    if 'pod' in result:
      if result[ 'pod' ] != self.pod:
        if not self.allow_config_change:
          raise PlatoConfigChanged( 'Pod Changed' )

        self.pod = result[ 'pod' ]

    else:
      self.pod = None

    return result

  def getRequest( self, *args, **kwargs ):
    return self._connector.getRequest( *args, **kwargs )

  def getJSONRequest( self, *args, **kwargs ):
    return self._connector.getJSONRequest( *args, **kwargs )

  def postRequest( self, *args, **kwargs ):
    return self._connector.postRequest( *args, **kwargs )

  def postJSONRequest( self, *args, **kwargs ):
    return self._connector.postJSONRequest( *args, **kwargs )
