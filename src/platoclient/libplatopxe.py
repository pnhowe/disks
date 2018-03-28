import urllib
import time

from platoclient.libplato import Plato, PlatoConnectionException

class PlatoPxeException( Exception ):
  pass

class PlatoPxeNoJobException( PlatoPxeException ):
  pass

class PlatoPxeUnknowResponse( PlatoPxeException ):
  pass


class PlatoPXE( Plato ):
  def __init__( self, *args, **kwargs ):
    if bool( set( [ 'local_config', 'no_config', 'no_ip' ] ) & set( open( '/proc/cmdline', 'r' ).read().split( ' ' ) ) ):
      super( PlatoPXE, self ).__init__( static_config={} )
    else:
      super( PlatoPXE, self ).__init__( *args, **kwargs )

  def signalComplete( self ):
    try:
      result = self.getRequest( 'provisioner/signal-complete/', {}, retry_count=0 )
    except PlatoConnectionException as e:
      raise Exception( 'libplatopxe: Exception during signal-complete: "%s"' % e )

    if result is None:
      return

    if result == 'No Job':
      raise PlatoPxeNoJobException()

    if result != 'Carry On':
      raise PlatoPxeUnknowResponse( 'Expected "Carry On" got "%s"' % result )

  def signalAlert( self, msg ):
    print '! %s !' % msg

    while True:
      try:
        result = self.getRequest( 'provisioner/signal-alert/', { 'msg': msg }, retry_count=0 )
      except PlatoConnectionException as e:
        raise Exception( 'libplatopxe: Exception during signal-alert: "%s"' % e )

      if result is None:
        return

      if result == 'Is Dispatched':
        time.sleep( 5 )
        continue

      break

    if result == 'No Job':
      raise PlatoPxeNoJobException()

    if result != 'Alert Raised':
      raise PlatoPxeUnknowResponse( 'Expected "Alert Raised" got "%s"' % result )

  def postMessage( self, msg ):
    print '* %s *' % msg

    try:
      result = self.getRequest( 'provisioner/post-message/', { 'msg': msg }, retry_count=0 )
    except PlatoConnectionException as e:
      raise Exception( 'libplatopxe: Exception during post-message: "%s"' % e )

    if result is None:
      return

    if result == 'No Job':
      return
      #Don't raise PlatoPXENoJobException, it's non critical and it would be a burden to catch it everytime postMessage is called

    if result != 'Saved':
      raise PlatoPxeUnknowResponse( 'Expected "Saved" got "%s"' % result )

  def readEnv( self, name ):
    try:
      result = self.getRequest( 'provisioner/read-env/', { 'name': name }, retry_count=0 )
    except PlatoConnectionException as e:
      raise Exception( 'libplatopxe: Exception during read-env: "%s"' % e )

    if result is None:
      return

    if result == 'No Job':
      raise PlatoPxeNoJobException()

    if not result.startswith( 'Value:' ):
      print 'libplatopxe: WARNING... Value not read.  result: "%s"' % result

    return result.split( ':' )[1]

  def writeEnv( self, name, value ):
    try:
      result = self.getRequest( 'provisioner/write-env/', { 'name': name, 'value': value }, retry_count=0 )
    except PlatoConnectionException as e:
      raise Exception( 'libplatopxe: Exception during write-env: "%s"' % e )

    if result is None:
      return

    if result == 'No Job':
      raise PlatoPxeNoJobException()

    if result != 'Saved':
      raise PlatoPxeUnknowResponse( 'Expected "Saved" got "%s"' % result )
