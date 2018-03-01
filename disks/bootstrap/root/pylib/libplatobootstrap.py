import json
import sys
from platoclient.libplato import Plato

def _parseProfileStatus( result ):
  if result == 'Not Found':
    return ( None, None )

  tmp = result.splitlines()

  if len( tmp ) < 1 or not tmp[0].startswith( 'State:' ):
    raise Exception( 'Unexpected Result from reporting hardware profile: "%s"' % result )

  state = tmp[0].split()[1]

  return state == 'Good'

class PlatoBootstrap( Plato ):
  def __init__( self, strap, *args, **kwargs ):
    self.strap = strap
    super( PlatoBootstrap, self ).__init__( *args, **kwargs )


  def doPoll( self, strap, lldp ):
    POSTData = { 'lldp': json.dumps( lldp ) }
    return self.postRequest( 'provisioner/strappoll/%s/' % strap, {}, POSTData )


  def strapDone( self ):
    result = self.getRequest( 'provisioner/strapdone/%s/' % self.strap, {} )

    return result == 'Sounds Good'


  def strapStatus( self, status ):
    print 'status: %s' % status

    parms = { 'status': status }

    result = self.getRequest( 'provisioner/strapstatus/%s/' % self.strap, parms )

    if result != 'Recorded':
      print 'Error Updating status, "%s"' % result
      sys.exit( 1 )


  def reportHardwareProfile( self, hardware, network, disks, eval_only=False ):
    POSTData = { 'data': json.dumps( { 'hardware': hardware, 'network': network, 'disks': disks } ) }

    params = {}
    if self.id:
      params['config_id'] = self.id #TODO: modify libplato to auto append the config_id, then take this out

    if eval_only:
      params['eval_only'] = ''

    result = self.postRequest( 'provisioner/reporthardwareprofile/', params, POSTData )

    if result.find( ':' ) < 1:
      return ( False, 'Unknwon response "%s"' % result )

    ( status, message ) = result.split( ':', 1 )

    return ( status == 'Good', message.strip() )
