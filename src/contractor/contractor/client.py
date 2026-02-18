import logging
import os
import json
import time
import random
import math
from datetime import datetime, timezone
from .cinp.client import CInP, Timeout, ResponseError


__VERSION__ = '0.1'
DELAY_MULTIPLIER = 15
# delay of 15 results in a delay of:
# min delay = 0, 10, 16, 20, 24, 26, 29, 31, 32, 34, 35, 37, 38, 39, 40 ....
# max delay = 0, 20, 32, 40, 48, 52, 58, 62, 64, 68, 70, 74, 76, 78, 80 .....


def getClient( job_config=None ):
  host = os.environ.get( 'contractor_host', 'http://contractor' )
  config_uuid = os.environ.get( 'config_uuid', None )

  if host.startswith( 'none://' ):
    client = NoConfigClient( config_uuid )

  elif host.startswith( 'http://' ) or host.startswith( 'https://' ):
    client = HTTPClient( host, os.environ.get( 'contractor_proxy', None ), config_uuid )

  else:
    client = LocalFileClient( host, config_uuid )

  if job_config is not None:
    job = json.loads( open( job_config, 'r' ).read() )[ 'job' ]
    client.job_id = job[ 'job_id' ]
    client.cookie = job[ 'cookie' ]

  return client


class NoJob( Exception ):
  pass


def JSONDefault( obj ):
   if isinstance( obj, datetime ):
     return obj.isoformat()

   return json.JSONEncoder.default( obj )


def _backOffDelay( count ):
  if count < 1:  # math.log dosen't do so well below 1
    count = 1

  factor = int( DELAY_MULTIPLIER * math.log( count ) )
  time.sleep( int( factor + ( random.random() * factor ) ) )


class Client():
  def __init__( self, config_uuid=None ):
    super().__init__()
    self.config_uuid = config_uuid

  def getConfig( self ):
    return {}

  def getDiskTemplate( self ):
    return ''

  def login( self ):
    pass

  def postMessage( self, msg ):
    print( '* {0} *'.format( msg ) )

  def signalAlert( self, msg ):
    print( '! {0} !'.format( msg ) )

  def signalComplete( self ):
    print( '### Complete ###' )


class NoConfigClient( Client ):
  pass


class HTTPClient( Client ):
  def __init__( self, host, proxy, config_uuid ):
    super().__init__( config_uuid )
    self.cinp = CInP( host=host, root_path='/api/v1/', proxy=proxy )
    self.job_id = None

    # self.cinp.opener.addheaders[ 'User-Agent' ] += ' - config agent'

  def request( self, method, uri, data=None, filter=None, timeout=30, retry_count=0, no_parse=False ):
    retry = 0
    while True:
      logging.debug( 'contractor: request: retry {0} of {1}, timeout: {2}'.format( retry, retry_count, timeout ) )
      try:
        if method == 'raw get':
          ( http_code, values, _ ) = self.cinp._request( 'RAWGET', uri, header_map={}, timeout=timeout, no_parse=no_parse )
          if http_code != 200:
            logging.warning( 'cinp: Unexpected HTTP Code "{0}" for GET'.format( http_code ) )
            raise ResponseError( 'Unexpected HTTP Code "{0}" for GET'.format( http_code ) )

          return values

        elif method == 'call':
          return self.cinp.call( uri, data, timeout=timeout )

        elif method == 'list':
          return self.cinp.list( uri, filter, data, timeout=timeout )

        elif method == 'update':
          return self.cinp.update( uri, data, timeout=timeout )

        else:
          raise ValueError( 'Unknown method "{0}"'.format( method ) )

      except ( ResponseError, Timeout ) as e:
        if not retry_count == -1 and retry >= retry_count:
          raise e

        logging.debug( 'contractor: request: Got Excpetion "{0}", request {1} of {2} retrying...'.format( e, retry, retry_count ) )

      retry += 1
      _backOffDelay( retry )

  def login( self ):
    token = self.request( 'call', '/api/v1/Auth/User(login)', { 'username': 'jobsig', 'password': 'jobsig' } )
    self.cinp.setAuth( 'jobsig', token )

  def postMessage( self, msg ):
    super().postMessage( msg )
    resp = self.request( 'call', '/api/v1/Foreman/BaseJob:{0}:(postMessage)'.format( self.job_id ), { 'msg': msg } )

    if resp != 'Posted':
      print( 'WARNING! Message Signaling Failed: "{0}"'.format( resp ) )

  def signalAlert( self, msg ):
    super().signalAlert( msg )
    resp = self.request( 'call', '/api/v1/Foreman/BaseJob:{0}:(signalAlert)'.format( self.job_id ), { 'msg': msg } )

    if resp != 'Alerted':
      print( 'WARNING! Alert Signaling Failed: "{0}"'.format( resp ) )

  def signalComplete( self ):
    super().signalComplete( )
    resp = self.request( 'call', '/api/v1/Foreman/BaseJob:{0}:(signalComplete)'.format( self.job_id ), { 'cookie': self.cookie } )

    if resp != 'Recieved':
      print( 'WARNING! Complete Signaling Failed: "{0}"'.format( resp ) )

  def getConfig( self, foundation_locator=None ):
    if self.config_uuid is not None:
      return self.request( 'raw get', '/config/config/c/{0}'.format( self.config_uuid ), timeout=10, retry_count=2 )
    elif foundation_locator is not None:
      return self.request( 'raw get', '/config/config/f/{0}'.format( foundation_locator ), timeout=10, retry_count=2 )
    else:
      return self.request( 'raw get', '/config/config/', timeout=10, retry_count=2 )  # the defaults cause libconfig to hang to a long time when it can't talk to contractor

  def getDiskTemplate( self ):
    if self.config_uuid is not None:
      return self.request( 'raw get', '/config/pxe_template/c/{0}'.format( self.config_uuid ), timeout=10, retry_count=2, no_parse=True )
    else:
      return self.request( 'raw get', '/config/pxe_template/', timeout=10, retry_count=2, no_parse=True )


def staticConfigValues():
  result = {}
  try:
    result[ '**MAC**' ] = open( '/sys/class/net/eth0/address', 'r' ).read().strip()
  except OSError:
    result[ '**MAC**' ] = '00:00:00:00:00:00'

  return result


def replaceStaticConfigValues( config ):
  for key, value in staticConfigValues().items():
    config = config.replace( key, value )

  return config


class LocalFileClient( Client ):
  def __init__( self, config_file, config_uuid ):
    super().__init__( config_uuid )
    self.config = json.loads( replaceStaticConfigValues( open( config_file, 'r' ).read() ) )
    self.config[ '__last_modified' ] = datetime.fromtimestamp( os.path.getctime( config_file ) ).replace( tzinfo=timezone.utc ).isoformat()

  def getConfig( self ):
    return self.config.copy()


class StaticConfigClient( Client ):
  def __init__( self, config_values, config_uuid ):
    super().__init__( config_uuid )
    self.config = config_values

  def getConfig( self ):
    return self.config.copy()
