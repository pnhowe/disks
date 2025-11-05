import sys
import time
import subprocess
import re


# NOTE: the retry_count is set to -1 in most places here b/c sometimes things get a bit sticky during bootstrap, and we
#       don't want to have to go through lots of work resetting things.  That being said, the delay after a while get's
#       pretty long and just as well have a retry count of 20 or something, we will see what comes of this.

_setMessage = None


class Bootstrap:
  def __init__( self, identifier, contractor ):
    self.identifier = identifier
    self.request = contractor.request

    self.token = self.request( 'call', '/api/v1/Auth/User(login)', { 'username': 'bootstrap', 'password': 'bootstrap' }, retry_count=-1 )
    contractor.cinp.setAuth( 'bootstrap', self.token )

    self.request( 'call', '/api/v1/Survey/Cartographer(register)', { 'identifier': identifier }, retry_count=-1 )

  def logout( self ):
    try:
      self.request( 'call', '/api/v1/Auth/User(logout)', { 'token': self.token } )
    except Exception:
      pass

  def lookup( self, info_map ):
    return self.request( 'call', '/api/v1/Survey/Cartographer:{0}:(lookup)'.format( self.identifier ), { 'info_map': info_map }, retry_count=-1  )

  def setMessage( self, message ):
    self.request( 'call', '/api/v1/Survey/Cartographer:{0}:(setMessage)'.format( self.identifier ), { 'message': message }, retry_count=-1  )

  def done( self ):
    return self.request( 'call', '/api/v1/Survey/Cartographer:{0}:(done)'.format( self.identifier ), {}, retry_count=-1  )

  def setIdMap( self, foundation_locator, id_map ):
    return self.request( 'call', '/api/v1/Building/Foundation:{0}:(setIdMap)'.format( foundation_locator ), { 'id_map': id_map }, retry_count=-1  )


def getLLDP():
  counter = 0
  lldp_values = {}
  results = {}
  while True:
    proc = subprocess.run( [ '/sbin/lldpcli', 'show', 'neighbors', '-f', 'keyvalue' ], shell=False, stdout=subprocess.PIPE )
    lldp_data = str( proc.stdout, 'utf-8' ).strip()
    if len( lldp_data ) > 10:
      for line in lldp_data.splitlines():
        if '=' not in line:
          continue
        ( key, value ) = line.split( '=' )
        lldp_values[key] = value
      break

    else:
      if counter >= 3:  # 30 seconds should be enough, lldp was started before bootstrap started
        _setMessage( 'lldp timeout waiting for data, skipping...' )
        return results

      counter += 1
      time.sleep( 10 )

  for item in lldp_values:
    ( protocol, interface, name ) = item.split( '.', 2 )  # protocol, interface

    if interface not in results:
      results[ interface ] = {}

    if name == 'chassis.mac':
      results[ interface ][ 'mac' ] = lldp_values[ item ]

    elif name == 'chassis.name':
      results[ interface ][ 'name' ] = lldp_values[ item ]

    elif name in ( 'port.local', 'port.ifname' ):
      parts = re.sub( '[^0-9/]', '', lldp_values[ item ] ).split( '/' )
      if len( parts ) == 1:
        results[ interface ][ 'slot' ] = 1
        results[ interface ][ 'port' ] = int( parts[0] )
        results[ interface ][ 'subport' ] = 0

      elif len( parts ) == 2:
        results[ interface ][ 'slot' ] = int( parts[0] )
        results[ interface ][ 'port' ] = int( parts[1] )
        results[ interface ][ 'subport' ] = 0

      elif len( parts ) == 3:
        results[ interface ][ 'slot' ] = int( parts[0] )
        results[ interface ][ 'port' ] = int( parts[1] )
        results[ interface ][ 'subport' ] = int( parts[2] )

      else:
        _setMessage( 'I don\'t know how to handle this lldp local port "{0}"'.format( lldp_values[ item ] ) )
        sys.exit( 1 )

  return results


def getIpAddress( interface ):
  proc = subprocess.run( [ '/sbin/ip', 'addr', 'show', 'dev', interface ], shell=False, stdout=subprocess.PIPE )
  lines = str( proc.stdout, 'utf-8' ).strip().splitlines()
  return lines[2].split()[1].split( '/' )[0]


def cpuPhysicalCount():
  wrk = []
  cpuinfo = open( '/proc/cpuinfo', 'r' )
  for line in cpuinfo.readlines():
    if line.startswith( 'physical id' ) and line not in wrk:
      wrk.append( line )

  return len( wrk )


def cpuLogicalCount():
  wrk = []
  cpuinfo = open( '/proc/cpuinfo', 'r' )
  for line in cpuinfo.readlines():
    if line.startswith( 'processor' ) and line not in wrk:
      wrk.append( line )

  return len( wrk )


def getRAMAmmount():
  meminfo = open( '/proc/meminfo', 'r' )
  for line in meminfo.readlines():
    if line.startswith( 'MemTotal' ):
      return int( line.split( ':' )[1].strip().split( ' ' )[0] ) / 1024
