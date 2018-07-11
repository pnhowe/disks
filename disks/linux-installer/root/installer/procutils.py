import os
import subprocess
import shlex
from datetime import datetime

debug_stdout = None
chroot_path = None

global_env = os.environ
global_env[ 'DEBIAN_PRIORITY' ] = 'critical'
global_env[ 'DEBIAN_FRONTEND' ] = 'noninteractive'

# anything in the chroot shoudn't have access to http_proxy
# the resulting OS will not have that if it's not explicty set some place
# need to force anything inside the resulting OS to get to the proxy correctly
chroot_env = global_env.copy()
try:
  del chroot_env[ 'http_proxy' ]
except KeyError:
  pass
try:
  del chroot_env[ 'https_proxy' ]
except KeyError:
  pass


def open_output( filename ):
  global debug_stdout
  debug_stdout = open( filename, 'w' )


def set_chroot( path ):
  global chroot_path
  chroot_path = path


def _execute( cmd, stdout, stderr, stdin, env ):
  debug_stdout.write( '\n=================================================\n' )
  debug_stdout.write( '{0}\n'.format( datetime.utcnow() ) )
  debug_stdout.write( 'Executing:\n' )
  debug_stdout.write( cmd )

  if stdin:
    debug_stdout.write( '\nstdin:\n' )
    debug_stdout.write( stdin )

  debug_stdout.write( '\n-------------------------------------------------\n' )
  try:
    if stdin:
      proc = subprocess.Popen( shlex.split( cmd ), env=env, stdout=stdout, stderr=stderr, stdin=subprocess.PIPE )
      ( output, _ ) = proc.communicate( stdin.encode() )

    else:
      proc = subprocess.Popen( shlex.split( cmd ), env=env, stdout=stdout, stderr=stderr )
      ( output, _ ) = proc.communicate()

  except Exception as e:
    raise Exception( 'Exception "{0}" while executing "{1}"'.format( e, cmd ) )

  debug_stdout.write( '\n-------------------------------------------------\n' )
  debug_stdout.write( 'rc: {0}\n'.format( proc.returncode ) )
  debug_stdout.write( '{0}\n'.format( datetime.utcnow() ) )
  debug_stdout.flush()

  if proc.returncode != 0:
    raise Exception( 'Error Executing "{0}"'.format( cmd ) )

  if output is None:
    return None
  else:
    return output.decode()


def execute( cmd, stdin=None ):
  _execute( cmd, debug_stdout, debug_stdout, stdin, global_env )


def execute_lines( cmd, stdin=None ):
  stdout = _execute( cmd, subprocess.PIPE, debug_stdout, stdin, global_env )
  return stdout.splitlines()


def chroot_execute( cmd, stdin=None ):
  _execute( '/sbin/chroot {0} {1}'.format( chroot_path, cmd ), debug_stdout, debug_stdout, stdin, chroot_env )


def chroot_execute_lines( cmd, stdin=None ):
  stdout = _execute( '/sbin/chroot {0} {1}'.format( chroot_path, cmd ), subprocess.PIPE, debug_stdout, stdin, chroot_env )
  return stdout.splitlines()
