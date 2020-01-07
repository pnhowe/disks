import sys
import time
import subprocess
import re


_setMessage = None


class Bootstrap:
  def __init__( self, identifier, contractor ):
    self.identifier = identifier
    self.request = contractor.request

    self.request( 'call', '/api/v1/Survey/Cartographer(register)', { 'identifier': identifier } )

  def lookup( self, info_map ):
    return self.request( 'call', '/api/v1/Survey/Cartographer:{0}:(lookup)'.format( self.identifier ), { 'info_map': info_map } )

  def setMessage( self, message ):
    self.request( 'call', '/api/v1/Survey/Cartographer:{0}:(setMessage)'.format( self.identifier ), { 'message': message } )

  def done( self ):
    return self.request( 'call', '/api/v1/Survey/Cartographer:{0}:(done)'.format( self.identifier ), {} )

  def setIdMap( self, foundation_locator, id_map ):
    return self.request( 'call', '/api/v1/Building/Foundation:{0}:(setIdMap)'.format( foundation_locator ), { 'id_map': id_map } )

  def setPXEBoot( self, foundation_locator, pxe ):
    iface_list, info = self.request( 'list', '/api/v1/Utilities/RealNetworkInterface', { 'foundation': '/api/v1/Building/Foundation:{0}:'.format( foundation_locator ) }, filter='foundation' )
    if info[ 'total' ] != info[ 'count' ]:
      raise Exception( 'There are more interface than we got' )  # wow, what kind of machine do you have there?

    for iface in iface_list:
      self.request( 'update', iface, { 'pxe': '/api/v1/BluePrint/PXE:{0}:'.format( pxe ) } )


def ipmicommand( cmd, ignore_failure=False ):
  proc = subprocess.run( [ '/bin/ipmitool' ] + cmd.split() )
  if proc.returncode != 0:
    if ignore_failure:
      print( 'WARNING: ipmi cmd "{0}" failed, ignored...'.format( cmd ) )
    else:
      _setMessage( 'Ipmi Error with: "{0}"'.format( cmd ) )
      sys.exit( 1 )


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
      if counter >= 10:
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


def getIPMIMAC( lan_channel ):
  proc = subprocess.run( [ '/bin/ipmitool', 'lan', 'print', str( lan_channel ) ], stdout=subprocess.PIPE )
  lines = str( proc.stdout, 'utf-8' ).strip().splitlines()
  for line in lines:
    if line.startswith( 'MAC Address' ):
      return line[ 25: ].strip()

  return None


def getIpAddress( interface ):
  proc = subprocess.run( [ '/sbin/ip', 'addr', 'show', 'dev', interface ], shell=False, stdout=subprocess.PIPE )
  lines = str( proc.stdout, 'utf-8' ).strip().splitlines()
  return lines[2].split()[1].split( '/' )[0]
