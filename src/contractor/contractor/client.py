import logging
import os
import json
import time
import random
import math
from datetime import datetime
from contractor.cinp.client import CInP, Timeout, ResponseError


__VERSION__ = '0.1'
DELAY_MULTIPLIER = 15
# delay of 15 results in a delay of:
# min delay = 0, 10, 16, 20, 24, 26, 29, 31, 32, 34, 35, 37, 38, 39, 40 ....
# max delay = 0, 20, 32, 40, 48, 52, 58, 62, 64, 68, 70, 74, 76, 78, 80 .....


def getClient():
  host = os.environ.get( 'contractor_host', 'http://contractor' )
  if host.startswith( 'file://' ):
    return LocalFileClient( host[ 7: ] )

  else:
    return HTTPClient( host=host, proxy=os.environ.get( 'contractor_proxy', None ) )  # TODO: have do_task put on the http


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
  def __init__( self ):
    super().__init__()

  def getConfig( self, config_uuid=None ):
    return {}

  def signalComplete( self ):
    return
    try:
      result = self.request( 'get', '/provisioner/signal-complete/', {}, retry_count=5 )
    except ResponseError as e:
      raise Exception( 'contractor: Exception during signal-complete: "{0}"'.format( e ) )

    if result is None:
      return

    if result == 'No Job':
      raise NoJob()

    if result != 'Carry On':
      raise ValueError( 'Expected "Carry On" got "{0}"'.format( result ) )

  def signalAlert( self, msg ):
    print( '! {0} !'.format( msg ) )
    return

    while True:
      try:
        result = self.request( 'get', '/provisioner/signal-alert/', { 'msg': msg } )
      except ResponseError as e:
        raise Exception( 'contractor: Exception during signal-alert: "{0}"'.format( e ) )

      if result is None:
        return

      if result == 'Is Dispatched':
        time.sleep( 5 )
        continue

      break

    if result == 'No Job':
      raise NoJob()

    if result != 'Alert Raised':
      raise ValueError( 'Expected "Alert Raised" got "{0}"'.format( result ) )

  def postMessage( self, msg ):
    print( '* {0} *'.format( msg ) )
    return

    try:
      result = self.request( 'get', '/provisioner/post-message/', { 'msg': msg } )
    except ResponseError as e:
      raise Exception( 'contractor: Exception during post-message: "{0}"'.format( e ) )

    if result is None:
      return

    if result == 'No Job':
      return
      # Don't raise PlatoPXENoJobException, it's non critical and it would be a burden to catch it everytime postMessage is called

    if result != 'Saved':
      raise ValueError( 'Expected "Saved" got "{0}"'.format( result ) )

  def lookup( self, info_map ):
    return None

  def setLocated( self, structure_id, id_map ):
    pass


class HTTPClient( Client ):
  def __init__( self, host, proxy ):
    super().__init__()
    self.cinp = CInP( host=host, root_path='/api/v1/', proxy=proxy )
    # self.cinp.opener.addheaders[ 'User-Agent' ] += ' - config agent'

  def request( self, method, uri, data=None, timeout=30, retry_count=0 ):
    retry = 0
    while True:
      logging.debug( 'contractor: request: retry {0} of {1}, timeout: {2}'.format( retry, retry_count, timeout ) )
      try:
        if method == 'raw get':
          ( http_code, values, _ ) = self.cinp._request( 'RAWGET', uri, header_map={}, timeout=timeout )
          if http_code != 200:
            logging.warning( 'cinp: unexpected HTTP Code "{0}" for GET'.format( http_code ) )
            raise ResponseError( 'Unexpected HTTP Code "{0}" for GET'.format( http_code ) )
          return values

        elif method == 'call':
          return self.cinp.call( uri, data, timeout=timeout )

      except ( ResponseError, Timeout ) as e:
        if not retry_count == -1 and retry >= retry_count:
          raise e
        logging.debug( 'contractor: request: Got Excpetion "{0}", request {1} of {2} retrying...'.format( e, retry, retry_count ) )

      retry += 1
      _backOffDelay( retry )

  def getConfig( self, config_uuid=None, foundation_locator=None ):
    if config_uuid is not None:
      return self.request( 'raw get', '/config/config/c/{0}'.format( config_uuid ), timeout=10, retry_count=2 )
    elif foundation_locator is not None:
      return self.request( 'raw get', '/config/config/f/{0}'.format( foundation_locator ), timeout=10, retry_count=2 )
    else:
      return self.request( 'raw get', '/config/config/', timeout=10, retry_count=2 )  # the defaults cause libconfig to hang to a long time when it can't talk to contractor

  def lookup( self, info_map ):
    return self.request( 'call', '/api/v1/Building/Foundation(lookup)', { 'info_map': info_map }, retry_count=100 )

  def setIdMap( self, foundation_locator, id_map ):
    return self.request( 'call', '/api/v1/Building/Foundation:{0}:(setIdMap)'.format( foundation_locator ), { 'id_map': id_map } )

  def setLocated( self, foundation_locator ):
    self.request( 'call', '/api/v1/Building/Foundation:{0}:(setLocated)'.format( foundation_locator ), {} )


class LocalFileClient( Client ):
  def __init__( self, config_file ):
    self.config = json.loads( open( config_file, 'r' ).read() )
    self.config[ 'last_modified' ] = datetime.fromtimestamp( os.path.getctime( config_file ) ).strftime( '%Y-%m-%d %H:%M:%S' )

  def getConfig( self, config_uuid=None ):
    return self.config.copy()


class StaticConfigClient( Client ):
  def __init__( self, config_values ):
    self.config = config_values

  def getConfig( self, config_uuid=None ):
    return self.config.copy()
