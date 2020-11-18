import os
import subprocess
import shlex
import time
from datetime import datetime

debug_stdout = None
chroot_path = None

EXEC_RETRY_COUNT = 10

global_env = os.environ
global_env[ 'DEBIAN_PRIORITY' ] = 'critical'
global_env[ 'DEBIAN_FRONTEND' ] = 'noninteractive'

# anything in the chroot shoudn't have access to http_proxy
# the resulting OS will not have that if it's not explicty set some place
# need to force anything inside the resulting OS to get to the proxy correctly
chroot_env = global_env.copy()
for name in ( 'http_proxy', 'https_proxy' ):
  try:
    del chroot_env[ name ]
  except KeyError:
    pass


def open_output( filename ):
  global debug_stdout
  debug_stdout = open( filename, 'w' )


def set_chroot( path ):
  global chroot_path
  chroot_path = path


def _execute( cmd, stdout, stderr, stdin, env, retry_rc_list=None ):
  debug_stdout.write( '\n=================================================\n' )
  debug_stdout.write( '{0}\n'.format( datetime.utcnow() ) )
  debug_stdout.write( 'Executing:\n' )
  debug_stdout.write( cmd )

  retry = EXEC_RETRY_COUNT
  while retry:
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

    if proc.returncode == 0:
      break

    if retry_rc_list is None or proc.returncode not in retry_rc_list:
      raise Exception( 'Error Executing "{0}"'.format( cmd ) )

    debug_stdout.write( 'try "{0}" of "{1}"'.format( EXEC_RETRY_COUNT - retry, EXEC_RETRY_COUNT ) )
    debug_stdout.write( '-------------------------------------------------\n' )
    debug_stdout.flush()

    retry -= 1
    time.sleep( 30 )

  else:
    raise Exception( 'Error Executing "{0}", retries exceded'.format( cmd ) )

  if output is None:
    return None
  else:
    return output.decode()


def execute( cmd, stdin=None, retry_rc_list=None ):
  _execute( cmd, debug_stdout, debug_stdout, stdin, global_env, retry_rc_list )


def execute_lines( cmd, stdin=None, retry_rc_list=None ):
  stdout = _execute( cmd, subprocess.PIPE, debug_stdout, stdin, global_env, retry_rc_list )
  return stdout.splitlines()


def chroot_execute( cmd, stdin=None, retry_rc_list=None ):
  _execute( '/sbin/chroot {0} {1}'.format( chroot_path, cmd ), debug_stdout, debug_stdout, stdin, global_env, retry_rc_list )


def chroot_execute_lines( cmd, stdin=None, retry_rc_list=None ):
  stdout = _execute( '/sbin/chroot {0} {1}'.format( chroot_path, cmd ), subprocess.PIPE, debug_stdout, stdin, global_env, retry_rc_list )
  return stdout.splitlines()
