import sys
import time
import subprocess
import re


contractor = None
config = None


def ipmicommand( cmd, ignore_failure=False ):
  proc = subprocess.run( [ '/bin/ipmitool' ] + cmd.split() )
  if proc.returncode != 0:
    if ignore_failure:
      print( 'WARNING: ipmi cmd "{0}" failed, ignored...'.format( cmd ) )
    else:
      contractor.postMessage( 'Ipmi Error with: "%s"' % cmd )
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
        contractor.postMessage( 'lldp timeout waiting for data, skipping...' )
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
        contractor.postMessage( 'I don\'t know how to handle this lldp local port "%s"' % lldp_values[item] )
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


def getIPMIMAC():
  proc = subprocess.run( [ '/bin/ipmitool', 'lan', 'print', config[ 'ipmi_lan_channel' ] ], stdout=subprocess.PIPE )
  lines = str( proc.stdout, 'utf-8' ).strip().splitlines()
  for line in lines:
    if line.startswith( 'MAC Address' ):
      return line[ 25: ].strip()

  return None


def getIpAddress( interface ):
  proc = subprocess.run( [ '/sbin/ip', 'addr', 'show', 'dev', interface ], shell=False, stdout=subprocess.PIPE )
  lines = str( proc.stdout, 'utf-8' ).strip().splitlines()
  return lines[2].split()[1].split( '/' )[0]
