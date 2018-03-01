import subprocess
import fcntl
import sys
import os
import time
import shlex
import string
import re

# these times are in (seconds)
POLL_DELAY = 60 # polling delay for check when things are done
MID_DRIVE_DELAY = 2 # delay between starting long operations on drives
TERMINATE_DELAY = 10 # number of seconds to wait for long operations to respond to terminate before killing
LOG_DIR = None

def setLogDir( log_dir ):
  global LOG_DIR

  LOG_DIR = log_dir

def runOnEachDrive( manager, cmd, stdin=None ):
  printable = re.compile( '[^%s]' % string.printable )

  block_list = []

  if len( manager.working_list ) < 1:
    return

  for drive in manager.working_list:
    print '%s' % drive.name,
    sys.stdout.flush()

    if not drive.block_name and ( '%(block_name)' in cmd or '%(block_path)' in cmd ):
      print ' <no block, skipped...>',
      sys.stdout.flush()
      continue

    if drive.block_name and drive.block_name in block_list: # no block_name and we got this far... don't need it
      print ' <allready have, skipped...>',
      sys.stdout.flush()
      continue

    block_list.append( drive.block_name )

    if LOG_DIR:
      log_file = open( '%s%s' % ( LOG_DIR, drive.name ), 'a' )

    tmpcmd = cmd % { 'block_name': drive.block_name, 'block_path': drive.block_path, 'libdrive_name': drive.libdrive_name }
    if LOG_DIR:
      log_file.write( '-------------------\n%s\n' % tmpcmd )
      log_file.flush()

    if stdin is not None:
      proc = subprocess.Popen( shlex.split( tmpcmd ), stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE )
      ( stdout, stderr ) = proc.communicate( stdin )
    else:
      proc = subprocess.Popen( shlex.split( tmpcmd ), stdout=subprocess.PIPE, stderr=subprocess.PIPE )
      ( stdout, stderr ) = proc.communicate()

    if LOG_DIR:
      if stdout:
        log_file.write( '========== STDOUT ==========\n' )
        log_file.write( printable.sub( '', stdout ) )
      if stderr:
        log_file.write( '\n========== STDERR ==========\n' )
        log_file.write( printable.sub( '', stderr ) )
      log_file.write( '\n========== RC: %s ==========\n' % proc.returncode )

    if proc.returncode != 0:
      print ' Failed'
      manager.prune( drive, '"%s" failed. rc: %d, out: "%s", err: "%s"' % ( tmpcmd, proc.returncode, printable.sub( '', stdout.strip() )[-60:], printable.sub( '', stderr.strip() )[-60:] ) )

    if LOG_DIR:
      log_file.close()

  print


def runOnEachDriveWait( manager, cmd, maxTime ):
  global MID_DRIVE_DELAY, POLL_DELAY, TERMINATE_DELAY

  block_list = []
  drive_list = []

  proc_list = {}
  cmd_list = {}
  stdout_list = {}
  stderr_list = {}
  lastline_list = {}
  log_file_list = {}
  done_flag = {}

  if len( manager.working_list ) < 1:
    return lastline_list

  for drive in manager.working_list:
    print '%s' % drive.name,
    sys.stdout.flush()

    if not drive.block_name and ( '%(block_name)' in cmd or '%(block_path)' in cmd ):
      print ' <no block, skipped...>',
      sys.stdout.flush()
      continue

    if drive.block_name and drive.block_name in block_list: # no block_name and we got this far... don't need it
      print ' <allready have, skipped...>',
      sys.stdout.flush()
      continue

    block_list.append( drive.block_name )
    drive_list.append( drive )

    if LOG_DIR:
      log_file_list[ drive.name ] = open( '%s%s' % ( LOG_DIR, drive.name ), 'a' )
      done_flag[ drive.name ] = False

    tmpcmd = cmd % { 'block_name': drive.block_name, 'block_path': drive.block_path, 'libdrive_name': drive.libdrive_name }
    if LOG_DIR:
      log_file_list[ drive.name ].write( '-------------------\n%s\n' % tmpcmd )
      log_file_list[ drive.name ].flush()

    proc = subprocess.Popen( shlex.split( tmpcmd ), stdout=subprocess.PIPE, stderr=subprocess.PIPE )

    # set stdout/stderr to non block... makes checking easier
    fd = proc.stdout.fileno()
    fl = fcntl.fcntl( fd, fcntl.F_GETFL )
    fcntl.fcntl( fd, fcntl.F_SETFL, fl | os.O_NONBLOCK )
    fd = proc.stderr.fileno()
    fl = fcntl.fcntl( fd, fcntl.F_GETFL )
    fcntl.fcntl( fd, fcntl.F_SETFL, fl | os.O_NONBLOCK )

    proc_list[drive] = proc
    cmd_list[drive] = tmpcmd
    stdout_list[drive] = ''
    stderr_list[drive] = ''
    time.sleep( MID_DRIVE_DELAY )

  print
  printable = re.compile( '[^%s]' % string.printable )
  startTime = time.time()
  done = False
  print # extra line so when we cursor up we don't overwrite anything
  if maxTime > 0:
    endTime = startTime + ( maxTime * 60 )
    while time.time() < endTime and not done:
      print chr( 27 ) + '[1A' + 'Max time remaining: %.2f (hours)     ' % ( ( endTime - time.time() ) / 3600.0 )
      time.sleep( POLL_DELAY )
      done = True
      for drive in drive_list:
        done = done & ( proc_list[drive].poll() is not None )

        try:
          while True:
            tmp = proc_list[drive].stdout.read()
            if LOG_DIR and tmp:
              log_file_list[ drive.name ].write( '\n========== STDOUT at: %.2f - %.2f==========\n' % ( ( time.time() - startTime ) / 3600.0, ( endTime - time.time() ) / 3600.0 ) )
              log_file_list[ drive.name ].write( printable.sub( '', tmp ) )
              log_file_list[ drive.name ].flush()
            if len( tmp ) == 0:
              break
            stdout_list[drive] = ( stdout_list[drive] + printable.sub( '', tmp ) )[-100:]
        except:
          pass

        try:
          while True:
            tmp = proc_list[drive].stderr.read()
            if LOG_DIR and tmp:
              log_file_list[ drive.name ].write( '\n========== STDERR at: %.2f - %.2f ==========\n' % ( ( time.time() - startTime ) / 3600.0, ( endTime - time.time() ) / 3600.0 ) )
              log_file_list[ drive.name ].write( printable.sub( '', tmp ) )
              log_file_list[ drive.name ].flush()
            if len( tmp ) == 0:
              break
            stderr_list[drive] = ( stderr_list[drive] + printable.sub( '', tmp ) )[-100:]
        except:
          pass

        if LOG_DIR and ( proc_list[drive].poll() is not None ):
          if not done_flag[ drive.name ]:
            log_file_list[ drive.name ].write( '\n========== Elapsed Time: %.2f (hours)\n' % ( ( time.time() - startTime ) / 3600.0 ) )
            done_flag[ drive.name ] = True
            log_file_list[ drive.name ].flush()

  else:
    while not done:
      print chr( 27 ) + '[1A' + 'Elapsed Time: %.2f (hours)     ' % ( ( time.time() - startTime ) / 3600.0 )
      time.sleep( POLL_DELAY )
      done = True
      for drive in drive_list:
        done = done & ( proc_list[drive].poll() is not None )

        try:
          while True:
            tmp = proc_list[drive].stdout.read()
            if LOG_DIR and tmp:
              log_file_list[ drive.name ].write( '\n========== STDOUT at: %.2f ==========\n' % ( ( time.time() - startTime ) / 3600.0 ) )
              log_file_list[ drive.name ].write( printable.sub( '', tmp ) )
              log_file_list[ drive.name ].flush()
            if len( tmp ) == 0:
              break
            stdout_list[drive] = ( stdout_list[drive] + printable.sub( '', tmp ) )[-100:]
        except:
          pass

        try:
          while True:
            tmp = proc_list[drive].stderr.read()
            if LOG_DIR and tmp:
              log_file_list[ drive.name ].write( '\n========== STDERR at: %.2f ==========\n' % ( ( time.time() - startTime ) / 3600.0 ) )
              log_file_list[ drive.name ].write( printable.sub( '', tmp ) )
              log_file_list[ drive.name ].flush()
            if len( tmp ) == 0:
              break
            stderr_list[drive] = ( stderr_list[drive] + printable.sub( '', tmp ) )[-100:]
        except:
          pass

        if LOG_DIR and ( proc_list[drive].poll() is not None ):
          if not done_flag[ drive.name ]:
            log_file_list[ drive.name ].write( '\n========== Elapsed Time: %.2f (hours)\n' % ( ( time.time() - startTime ) / 3600.0 ) )
            done_flag[ drive.name ] = True
            log_file_list[ drive.name ].flush()

    print 'Took %.2f (hours).' % ( ( time.time() - startTime ) / 3600.0 )

  print 'Checking results...'
  for drive in drive_list:
    rc = proc_list[drive].poll()

    try:
      while True:
        tmp = proc_list[drive].stdout.read()
        if LOG_DIR and tmp:
          log_file_list[ drive.name ].write( '\n========== STDOUT ==========\n' )
          log_file_list[ drive.name ].write( printable.sub( '', tmp ) )
        if len( tmp ) == 0:
          break
        stdout_list[drive] = ( stdout_list[drive] + printable.sub( '', tmp ) )[-100:]
    except:
      pass

    try:
      while True:
        tmp = proc_list[drive].stderr.read()
        if LOG_DIR and tmp:
          log_file_list[ drive.name ].write( '\n========== STDERR ==========\n' )
          log_file_list[ drive.name ].write( printable.sub( '', tmp ) )
        if len( tmp ) == 0:
          break
        stderr_list[drive] = ( stderr_list[drive] + printable.sub( '', tmp ) )[-100:]
    except:
      pass

    if LOG_DIR:
      if rc is None:
        log_file_list[ drive.name ].write( '\n========== TIMED OUT ==========\n' )
      else:
        log_file_list[ drive.name ].write( '\n========== RC: %s ==========\n' % proc.returncode )

    if rc is None:
      print 'Timed out for %s' % drive.name
      manager.prune( drive, '"%s" did not complete in time' % cmd_list[drive] )
      proc_list[drive].terminate()
      time.sleep( TERMINATE_DELAY )
      if proc_list[drive].poll() is None:
        proc_list[drive].kill()

    elif rc != 0:
      print 'Failed on %s' % drive.name
      print 'Error: "%s"' % stderr_list[drive].strip()
      manager.prune( drive, '"%s" failed. rc: %d, out: "%s", err: "%s"' % ( cmd_list[drive], proc_list[drive].returncode, stdout_list[drive].strip()[-60:], stderr_list[drive].strip()[-60:] ) )

    tmp = stdout_list[drive].strip()
    lastline_list[drive] = tmp[tmp.rfind( '\n' ):].strip()

    if LOG_DIR:
      log_file_list[ drive.name ].close()

  print

  return lastline_list
