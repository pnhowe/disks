import logging
import os
import json
import socket
import time
import random
import math
from datetime import datetime
from urllib import request

__VERSION__ = '0.1'
DELAY_MULTIPLIER = 15
# delay of 15 results in a delay of:
# min delay = 0, 10, 16, 20, 24, 26, 29, 31, 32, 34, 35, 37, 38, 39, 40 ....
# max delay = 0, 20, 32, 40, 48, 52, 58, 62, 64, 68, 70, 74, 76, 78, 80 .....


def getClient():
  host = os.environ.get( 'controller_host', 'controller' )
  if host.startswith( 'file://' ):
    return LocalFileClient( host[ 7: ] )

  else:
    return HTTPClient( host='http://{0}'.format( os.environ.get( 'controller_host', 'controller' ) ), proxy=os.environ.get( 'controller_proxy', None ) )  # TODO: have do_task put on the http


class HTTPErrorProcessorPassthrough( request.HTTPErrorProcessor ):
  def http_response( self, request, response ):
    return response


class Timeout( Exception ):
  pass


class CommunicationError( Exception ):
  pass


class ResponseError( Exception ):
  pass


class InvalidRequest( Exception ):
  pass


class NotFound( Exception ):
  pass


class ServerError( Exception ):
  pass


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

  def getConfig( self, config_uuid=None, config_id=None ):
    parms = {}
    # if config_uuid is not None:
    #   parms[ 'config_uuid' ] = config_uuid
    # elif config_id is not None:
    #   parms[ 'config_id' ] = config_id

    return self.request( 'get', '/config/config/', parms, timeout=10, retry_count=2 )  # the defaults cause libconfig to hang to a long time when it can't talk to the controller

  def signalComplete( self ):
    return
    try:
      result = self.request( 'get', '/provisioner/signal-complete/', {}, retry_count=5 )
    except CommunicationError as e:
      raise Exception( 'controller: Exception during signal-complete: "{0}"'.format( e ) )

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
      except CommunicationError as e:
        raise Exception( 'controller: Exception during signal-alert: "{0}"'.format( e ) )

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
    except CommunicationError as e:
      raise Exception( 'controller: Exception during post-message: "{0}"'.format( e ) )

    if result is None:
      return

    if result == 'No Job':
      return
      # Don't raise PlatoPXENoJobException, it's non critical and it would be a burden to catch it everytime postMessage is called

    if result != 'Saved':
      raise ValueError( 'Expected "Saved" got "{0}"'.format( result ) )


class HTTPClient( Client ):
  def __init__( self, host, proxy=None ):
    super().__init__()

    if not host.startswith( ( 'http:', 'https:' ) ):
      raise ValueError( 'hostname must start with http(s):' )

    if host[-1] == '/':
      raise ValueError( 'hostname must not end with "/"' )

    self.host = host
    self.proxy = proxy
    logging.debug( 'controller: new client host: "{0}", via: "{1}"'.format( self.host, self.proxy ) )

    if self.proxy:  # not doing 'is not None', so empty strings don't try and proxy   # have a proxy option to take it from the envrionment vars
      self.opener = request.build_opener( HTTPErrorProcessorPassthrough, request.ProxyHandler( { 'http': self.proxy, 'https': self.proxy } ) )
    else:
      self.opener = request.build_opener( HTTPErrorProcessorPassthrough, request.ProxyHandler( {} ) )

    self.opener.addheaders = [
                                ( 'User-Agent', 'bootable disks client {0}'.format( __VERSION__ ) ),
                                ( 'Accepts', 'application/json' ),
                                ( 'Accept-Charset', 'utf-8' ),
                              ]

  def _request( self, method, uri, data=None, timeout=30 ):
    logging.debug( 'controller: making "{0}" request to "{1}"'.format( method, uri ) )

    if data is None:
      data = ''.encode( 'utf-8' )
    else:
      data = json.dumps( data, default=JSONDefault ).encode( 'utf-8' )

    url = '{0}{1}'.format( self.host, uri )
    req = request.Request( url, data=data, headers={ 'Content-Type': 'application/json;charset=utf-8' } )
    req.get_method = lambda: method
    try:
      resp = self.opener.open( req, timeout=timeout )

    except request.HTTPError as e:
      raise CommunicationError( 'HTTPError "{0}"'.format( e ) )

    except request.URLError as e:
      if isinstance( e.reason, socket.timeout ):
        raise Timeout( 'Request Timeout after {0} seconds'.format( timeout ) )

      raise CommunicationError( 'URLError "{0}" for "{1}" via "{2}"'.format( e, url, self.proxy ) )

    except socket.timeout:
      raise Timeout( 'Request Timeout after {0} seconds'.format( timeout ) )

    except socket.error as e:
      raise CommunicationError( 'Socket Error "{0}"'.format( e ) )

    http_code = resp.code
    if http_code not in ( 200, 400, 404, 500 ):
      raise ResponseError( 'HTTP code "{0}" unhandled'.format( resp.code ) )

    logging.debug( 'controller: got http code "{0}"'.format( http_code ) )

    buff = str( resp.read(), 'utf-8' ).strip()
    if not buff:
      data = None
    else:
      try:
        data = json.loads( buff )
      except ValueError:
        data = None
        if http_code not in ( 400, 500 ):  # these two codes can deal with non dict data
          logging.warning( 'controller: Unable to parse response "{0}"'.format( buff[ 0:200 ] ) )
          raise ResponseError( 'Unable to parse response "{0}"'.format( buff[ 0:200 ] ) )

    resp.close()

    if http_code == 400:
      try:
        message = data[ 'message' ]
      except ( KeyError, ValueError, TypeError ):
        message = buff[ 0:200 ]

      logging.warning( 'controller: Invalid Request "{0}"'.format( message ) )
      raise InvalidRequest( message )

    if http_code == 404:
      logging.warning( 'controller: Not Found' )
      raise NotFound()

    if http_code == 500:
      if isinstance( data, dict ):
        try:
          message = 'Server Error "{0}"\n{1}'.format( data[ 'message' ], data[ 'trace' ] )
        except KeyError:
          try:
            message = 'Server Error "{0}"'.format( data[ 'message' ] )
          except KeyError:
            message = data

      else:
        message = 'Server Error: "{0}"'.format( buff[ 0:200 ] )

      logging.error( 'controller: {0}'.format( message ) )
      raise ServerError( message )

    return data

  def request( self, method, uri, data=None, timeout=30, retry_count=0 ):
    count = 0
    while True:
      logging.debug( 'controller: request: retry {0} of {1}, timeout: {2}'.format( count, retry_count, timeout ) )
      try:
        return self._request( method, uri, data, timeout )
      except ( CommunicationError, Timeout ) as e:
        if not retry_count == -1 and count >= retry_count:
          raise e
        logging.debug( 'controller: request: Got Excpetion "{0}", request {1} of {2} retrying...'.format( e, count, retry_count ) )

      count += 1
      _backOffDelay( count )


class LocalFileClient( Client ):
  def __init__( self, config_file ):
    self.config = json.loads( open( config_file, 'r' ).read() )
    self.config[ 'last_modified' ] = datetime.fromtimestamp( os.path.getctime( config_file ) ).strftime( '%Y-%m-%d %H:%M:%S' )

  def getConfig( self, config_uuid=None, config_id=None ):
    return self.config.copy()

  def request( self, method, uri, data=None, timeout=30, retry_count=0 ):
    return {}


class StaticConfigClient( Client ):
  def __init__( self, config_values ):
    self.config = config_values

  def getConfig( self, config_uuid=None, config_id=None ):
    return self.config.copy()

  def request( self, method, uri, data=None, timeout=30, retry_count=0 ):
    return {}
