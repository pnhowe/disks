import json
import urllib
from platoclient.libplato import Plato

class PlatoClientException( Exception ):
  pass

def _parseDriveStatus( result ):
  if result is None:
    return None

  if result == 'Not Found':
    return ( None, None )

  tmp = result.splitlines()

  if len( tmp ) < 2 or not tmp[0].startswith( 'Health:' ) or not tmp[1].startswith( 'Status:' ):
    raise Exception( 'Unexpected Result from reporting drive status: "{0}"'.format( result ) )

  health = None

  try:
    health = int( tmp[0].split()[1] )
  except:
    raise Exception( 'Unexpected Health Value from getting status: "{0}"'.format( result ) )

  if health >= 8:
    return ( False, health )

  if tmp[1].split()[1] not in ( 'O', 'S', 'N' ):
    return ( False, health )

  return ( True, health )


def _parseHardwareStatus( result ):
  if result is None:
    return None

  if result == 'Not Found':
    return ( None, None )

  tmp = result.splitlines()

  if len( tmp ) < 1 or not tmp[0].startswith( 'Health:' ):
    raise Exception( 'Unexpected Result from reporting hardware status: "{0}"'.format( result ) )

  health = None

  try:
    health = int( tmp[0].split()[1] )
  except:
    raise Exception( 'Unexpected Health Value from getting status: "{0}"'.format( result ) )

  if health >= 8:
    return ( False, health )

  return ( True, health )


def _parseConfigStatus( result ):
  if result is None:
    return None

  if result == 'Not Found':
    return ( None, None )

  tmp = result.splitlines()

  if len( tmp ) < 1 or not tmp[0].startswith( 'Health:' ):
    raise Exception( 'Unexpected Result from reporting config status: "{0}"'.format( result ) )

  health = None

  try:
    health = int( tmp[0].split()[1] )
  except:
    raise Exception( 'Unexpected Health Value from getting status: "{0}"'.format( result ) )

  if health >= 8:
    return ( False, health )

  return ( True, health )


class PlatoClient( Plato ):
  def __init__( self, *args, **kwargs ):
    super( PlatoClient, self ).__init__( *args, **kwargs )

  def removeDrive( self, port ):
    POSTData = { 'data': json.dumps( { 'location': port.location, 'store': False } ) }

    result = self.postRequest( 'storage/removedrive/', {}, POSTData )

    if result is None:
      return

    if result != 'Saved':
      raise PlatoClientException( 'Error Removing Drive From Machine: {0}'.format( result ) )

  def checkDriveStatus( self, drive ):
    result = self.getRequest( 'storage/checkdrivestatus/{0}/'.format( urllib.quote( drive.serial ), { 'location': drive.location, 'name': drive.name } ) )

    return _parseDriveStatus( result )

  def checkDriveThrashed( self, drive ):
    result = self.getRequest( 'storage/checkdrivethrashed/{0}/'.format( urllib.quote( drive.serial ), {} ) )

    if result == 'Thrashed':
      return True
    elif result == 'Not Thrashed':
      return False
    elif result == 'Not Found' or result is None:
      return None
    else:
      raise PlatoClientException( 'Unknown Result When Getting Thrashed Status: "{0}"'.format( result ) )

  def reportDriveStatus( self, drive, event, status, check_only=False, current_health=None  ): # status from drive.driveReportingStatus()
    POSTData = { 'data': json.dumps( { 'info': drive.reporting_info, 'event': event, 'location': drive.location, 'name': drive.name, 'status': status, 'check_only': check_only, 'current_health': current_health } ) }

    result = self.postRequest( 'storage/recorddrivestatus/{0}/'.format( urllib.quote( drive.serial ), {}, POSTData ) )

    return _parseDriveStatus( result )

  def reportHardwareStatus( self, status ):
    POSTData = { 'data': json.dumps( { 'status': status } ) }

    result = self.postRequest( 'device/recordhardwarestatus/', {}, POSTData )

    return _parseHardwareStatus( result )

  def reportConfigStatus( self, status ):
    POSTData = { 'data': json.dumps( { 'status': status } ) }

    result = self.postRequest( 'config/recordconfigstatus/', {}, POSTData )

    return _parseConfigStatus( result )
