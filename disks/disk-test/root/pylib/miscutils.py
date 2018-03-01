import sys
import os
import time
import subprocess

LIGHTUP_CMD = '/bin/dd if=%(drive)s of=/dev/null bs=512'

def doSaveResults( manager ):
  #TODO: Add the running hours of the drive to the output

  print 'Bad Drives...'
  for drive in manager.bad_reasons:
    print '%s' % drive.name,
    sys.stdout.flush()
    if not drive.block_path:
      print ' <no block, skipped...>',
      sys.stdout.flush()
      continue

    try:
      fp = file( drive.block_path, 'r+' )
      fp.seek( -512, os.SEEK_END )
      msg = 'FAIL\n%s\n%s' % ( time.strftime( "%a, %d %b %Y %H:%M:%S +0000", time.gmtime() ), manager.bad_reasons[drive] )
      fp.write( msg[:510] )
      fp.close()
    except Exception as e:
      print 'Error writing status to drive "%s", this drive was allready flagged as bad' % drive.name
      # don't need to put in dead_list, allready there
  print

  print 'Good Drives...'
  for drive in manager.working_list:
    print '%s' % drive.name,
    sys.stdout.flush()
    if not drive.block_path:
      print ' <no block, skipped...>',
      sys.stdout.flush()
      continue

    try:
      fp = file( drive.block_path, 'r+' )
      fp.seek( -512, os.SEEK_END )
      msg = 'PASS\n%s\n' % time.strftime( "%a, %d %b %Y %H:%M:%S +0000", time.gmtime() )
      while len( msg ) < 510:
        msg += '\0\0\0\0\0\0\0\0'
      fp.write( msg[:510] )
      fp.close()
    except Exception as e:
      print 'Error writing status to drive "%s", this drive was suposed to be good.  Shame Shame.' % drive.name
      manager.prune( drive, 'Exception when saving results to drive: %s ' % e )

  print


def doLightupDrives( manager ):
  """
  Light up drive activity light
  """
  tmpBuff = ''

  if len( manager.working_list ) < 1:
    print 'There are no good drives'
    while tmpBuff != 'Opps':
      print 'Type "Opps" and press <Enter> to Continue...'
      tmpBuff = sys.stdin.readline().strip()

    return

  print 'I will now light up the good drives. Will not last forever ;-)'

  while tmpBuff != 'Lights':
    print 'Type "Lights" and press <Enter> to Continue...'
    tmpBuff = sys.stdin.readline().strip()

  lightuper_list = []
  for drive in manager.working_list:
    print '%s' % drive.name,
    sys.stdout.flush()
    if drive.block_path:
      lightuper_list.append( subprocess.Popen( ( LIGHTUP_CMD % { 'drive': drive.block_path } ).split() ) )
    else:
      print ' <no block, skipped...>',
      sys.stdout.flush()

  print

  tmpBuff = ''
  while tmpBuff != 'All Done':
    print 'Type "All Done" and press <Enter> to Continue...'
    tmpBuff = sys.stdin.readline().strip()

  print "Stopping drives lights..."
  for lightuper in lightuper_list:
    lightuper.terminate()
