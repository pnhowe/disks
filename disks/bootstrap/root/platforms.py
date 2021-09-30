import os
import time
import sys
import subprocess


_setMessage = None


def getPlatform( hardware ):
  if os.path.exists( '/dev/ipmi0' ):
    return IPMIPlatform()

  if 'Management Controller Host Interface' in hardware[ 'dmi' ]:
    return RedFishPlatform()

  elif os.path.exists( '/dev/mei0' ):
    return AMTPlatform()  # NOTE: Intel has pulled the AMT SDK, and I can't find any other tools to get the AMT's MAC and set the ip locally, for new we skip this... keep an eye on https://software.intel.com/en-us/amt-sdk

  return UnknownPlatform()


class UnknownPlatform:
  def __init__( self ):
    pass

  @property
  def type( self ):
    return 'Unknown'

  def startIdentify( self ):
    pass

  def stopIdentify( self ):
    pass

  def setup( self, config ):
    return False

  def setAuth( self, config ):
    return False

  def setIp( self, config ):
    return False

  def updateNetwork( self, config, network ):
    pass

  def prep( self ):
    pass


class IPMIPlatform( UnknownPlatform ):
  def __init__( self ):
    pass

  @property
  def type( self ):
    return 'IPMI'

  def _command( cmd, ignore_failure=False ):
    proc = subprocess.run( [ '/bin/ipmitool' ] + cmd.split() )
    if proc.returncode != 0:
      if ignore_failure:
        print( 'WARNING: ipmi cmd "{0}" failed, ignored...'.format( cmd ) )
      else:
        _setMessage( 'Ipmi Error with: "{0}"'.format( cmd ) )
        sys.exit( 1 )

  def startIdentify( self ):
    self._command( 'chassis identify force', True )

  def stopIdentify( self ):
    pass

  def setup( self, config ):
    self._command( 'sol set force-encryption true {0}'.format( config[ 'ipmi_lan_channel' ] ), True )  # these two stopped working on some new SM boxes, not sure why.
    self._command( 'sol set force-authentication true {0}'.format( config[ 'ipmi_lan_channel' ] ), True )
    self._command( 'sol set enabled true {0}'.format( config[ 'ipmi_lan_channel' ] ) )
    self._command( 'sol set privilege-level user {0}'.format( config[ 'ipmi_lan_channel' ] ), True )  # dosen't work on some SM boxes?
    self._command( 'sol payload enable {0} 5'.format( config[ 'ipmi_lan_channel' ] ) )

    return True

  def setAuth( self, config ):
    return False
    # remove the other users first
    # lib.ipmicommand( 'user disable 5' )
    # lib.ipmicommand( 'user set name 5 {0}_'.format( ipmi_username ) )  # some ipmi's don't like you to set the username to the same as it is allready....Intel!!!
    # lib.ipmicommand( 'user set name 5 {0}'.format( ipmi_username ) )
    # lib.ipmicommand( 'user set password 5 {0}'.format( ipmi_password ) )
    # lib.ipmicommand( 'user enable 5' )
    # lib.ipmicommand( 'user priv 5 4 {0}'.format( config[ 'ipmi_lan_channel' ] ) )  # 4 = ADMINISTRATOR
    # return True

  def setIp( self, config ):
    self._command( 'lan set {0} arp generate off'.format( config[ 'ipmi_lan_channel' ] ), True )  # disable gratious arp, dosen't work on some Intel boxes?
    self._command( 'lan set {0} ipsrc static'.format( config[ 'ipmi_lan_channel' ] ) )
    self._command( 'lan set {0} ipaddr {1}'.format( config[ 'ipmi_lan_channel' ], config[ 'ipmi_ip_address' ] ) )
    self._command( 'lan set {0} netmask {1}' .format( config[ 'ipmi_lan_channel' ], config[ 'ipmi_ip_netmask' ] ) )
    self._command( 'lan set {0} defgw ipaddr {1}'.format( config[ 'ipmi_lan_channel' ], config.get( 'ipmi_ip_gateway', '0.0.0.0' ) ) )  # use the address 0.0.0.0 dosen't allways work for disabeling defgw

    try:
      self._command( 'lan set {0} vlan id {1}'.format( config[ 'ipmi_lan_channel' ], config[ 'ipmi_ip_vlan' ] ) )
    except KeyError:
      pass

    return True

  def updateNetwork( self, config, network ):
    proc = subprocess.run( [ '/bin/ipmitool', 'lan', 'print', str( config[ 'ipmi_lan_channel' ] ) ], stdout=subprocess.PIPE )
    lines = str( proc.stdout, 'utf-8' ).strip().splitlines()
    for line in lines:
      if line.startswith( 'MAC Address' ):
        network[ 'ipmi' ] = line[ 25: ].strip()
        return

  def prep( self ):
    print( 'Letting BMC Settle...' )
    time.sleep( 30 )  # make sure the bmc has saved everything

    print( 'Resetting BMC...' )  # so the intel boards's BMCs like to go out to lunch and never come back, wich messes up power off and bios config in the subtask, have to do something here later
    _setMessage( 'Resetting BMC...' )
    self._command( 'bmc reset cold' )  # reset the bmc, make sure it comes up the way we need it
    print( 'Letting BMC come back up...' )
    time.sleep( 60 )  # let the bmc restart

    # bmc info can hang for ever use something like http://stackoverflow.com/questions/1191374/subprocess-with-timeout
    # to kill it and restart and try again.
    print( 'Waiting For BMC to come back...' )
    _setMessage( 'Waiting For BMC to come back...' )
    self._command( 'bmc info' )  # should hang until it comes back, need a timeout for this
    self._command( 'bmc info' )

    print( 'Clearing BMC Event Log...' )
    self._command( 'sel clear', True )  # clear the eventlog, clean slate, everyone deserves it


class AMTPlatform( UnknownPlatform ):
  def __init__( self ):
    pass

  @property
  def type( self ):
    return 'AMT'


class RedFishPlatform( UnknownPlatform ):
  def __init__( self ):
    pass

  @property
  def type( self ):
    return 'RedFish'
