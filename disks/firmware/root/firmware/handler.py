import subprocess
import shlex
import fcntl
import os
import re
import string
import shutil
import time
from datetime import datetime, timezone

printable = re.compile( f'[^{string.printable}]'.encode() )


class FileAndConsoleWriter( object ):
  def __init__( self, filename ):
    self.console = open( '/dev/tty4', 'w')
    self.file = open( f'/tmp/{filename}', 'w' )

  def write( self, buff ):
    self.file.write( buff )
    self.console.write( buff )

  def flush( self ):
    self.file.flush()


class Handler( object ):
  def __init__( self ):
    self.WORK_DIR = '/wrk'

  def getTargets( self ):
    return []  # return list of ( target_handle, model, version ), return None on failure

  def updateTarget( self, target, filename ):
    return False

  def _prep_wrk_dir( self ):
    try:
      os.makedirs( self.WORK_DIR )

    except OSError as e:
      if e.errno == 17:  # allready exists
        shutil.rmtree( self.WORK_DIR )
        os.makedirs( self.WORK_DIR )

      else:
        raise e

  def _get_pci_info( self, pci_address ):
    result = {}
    line_list = open( f'/sys/bus/pci/devices/{pci_address}/uevent', 'r' ).readlines()
    for line in line_list:
      ( key, value ) = line.strip().split( '=', 1 )
      result[ key.lower() ] = value
      if key == 'PCI_ID':
        ( vendor_id, product_id ) = value.split( ':', 1 )
        result[ 'vendor_id' ] = vendor_id
        result[ 'product_id' ] = product_id

    return result

  def _execute_quemu( self, name, cmd, filename_list, pci=None ):
    if pci:
      print( 'Unbinding PCI Devices...' )
      open( '/sys/bus/pci/drivers/pci-stub/new_id', 'w' ).write( f'{pci[ 'vendor' ]} {pci[ 'product' ]}' )
      open( f'/sys/bus/pci/drivers/{pci[ 'driver' ]}/unbind', 'w' ).write( pci[ 'address' ] )
      open( '/sys/bus/pci/drivers/pci-stub/bind', 'w' ).write( pci[ 'address' ] )

    print( 'Building disk image...' )
    shutil.copyfile( '/qemu/dosboot.img.tpl', '/qemu/dosboot.img' )
    ( rc, _ ) = self._execute( f'{name}_mount', 'mount -o loop /qemu/dosboot.img /target' )
    if rc != 0:
      print( 'Error mounting dosboot.img' )
      return None

    for filename in filename_list:
      shutil.copyfile( filename, f'/target/{os.path.basename( filename )}' )

    open( '/target/autoexec.bat', 'w' ).write( f"""
echo "Executing Task..."

{cmd}

echo "Powering off...."
fdapm poweroff
""" )

    ( rc, _ ) = self._execute( f'{name}_umount', 'umount /target' )
    if rc != 0:
      print( 'Error mounting dosboot.img' )
      return None

    args = []
    if pci:
      args.append( f'-device pci-assign,host={pci[ 'address' ]}' )

    args.append( f'-serial file:/tmp/{name}_qemu_output' )

    print( 'Starting QEMU...' )
    ( rc, line_list ) = self._execute( f'{name}_qemu', '/qemu/qemu-system-x86_64 -machine pc -enable-kvm -m 512 -cpu host -kernel memdisk -initrd dosboot.img -net none -display none --option-rom sgabios.bin ' + ' '.join( args ), cwd='/qemu/' )

    # don't get the PCI device back, now that it's new firmware it might cause problems with the driver
    # we will catch it after the reboot
    # if pci:
    #  print 'UnBinding PCI Devices...'
    #  open( '/sys/bus/pci/drivers/pci-stub/unbind', 'w' ).write( pci[ 'address' ] )
    #  open( '/sys/bus/pci/drivers/pci-stub/remove_id', 'w' ).write( f'{pci[ 'vendor' ]} {pci[ 'product' ]}' )
    #  open( f'/sys/bus/pci/drivers/{pci[ 'driver' ]}/bind', 'w' ).write( pci[ 'address' ] )

    os.unlink( '/qemu/dosboot.img' )

    if rc != 0:
      print( 'Error in QEMU execution' )
      return None

    return line_list

  def _execute( self, name, cmd, cwd='/' ):
    return_lines = ''
    log = FileAndConsoleWriter( name )
    log.write( f'{datetime.now( timezone.utc )}\n' )
    log.write( 'Executing:\n' )
    log.write( cmd )

    log.write( '\n-------------------------------------------------\n' )
    log.flush()

    proc = subprocess.Popen( shlex.split( cmd ), stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd )

    fd = proc.stdout.fileno()
    fl = fcntl.fcntl( fd, fcntl.F_GETFL )
    fcntl.fcntl( fd, fcntl.F_SETFL, fl | os.O_NONBLOCK )
    fd = proc.stderr.fileno()
    fl = fcntl.fcntl( fd, fcntl.F_GETFL )
    fcntl.fcntl( fd, fcntl.F_SETFL, fl | os.O_NONBLOCK )

    while proc.poll() is None:
      try:
        tmp = proc.stdout.read()
      except IOError:
        time.sleep( 1 )
        continue
      if tmp:
        log.write( f'\n========== STDOUT at: {datetime.now( timezone.utc )}==========\n' )
        while tmp:
          log.write( printable.sub( b'', tmp ).decode() )
          return_lines = ( return_lines + printable.sub( b'', tmp ).decode() )[-5000:]
          tmp = proc.stdout.read()

      try:
        tmp = proc.stderr.read()
      except IOError:
        time.sleep( 1 )
        continue
      if tmp:
        log.write( f'\n========== STDERR at: {datetime.now( timezone.utc )}==========\n' )
        while tmp:
          log.write( printable.sub( b'', tmp ).decode() )
          tmp = proc.stdout.read()

      log.flush()

    try:
      tmp = proc.stdout.read()
    except IOError:
      tmp = None
    if tmp:
      log.write( f'\n========== STDOUT at: {datetime.now( timezone.utc )}==========\n' )
      while tmp:
        log.write( printable.sub( b'', tmp ).decode() )
        return_lines = ( return_lines + printable.sub( b'', tmp ).decode() )[-5000:]
        tmp = proc.stdout.read()

    try:
      tmp = proc.stderr.read()
    except IOError:
      tmp = None
    if tmp:
      log.write( f'\n========== STDERR at: {datetime.now( timezone.utc )}==========\n' )
      while tmp:
        log.write( printable.sub( b'', tmp ).decode() )
        tmp = proc.stdout.read()

    log.flush()

    log.write( '\n-------------------------------------------------\n' )
    log.write( f'rc: {proc.returncode}\n' )
    log.write( f'{datetime.now( timezone.utc )}\n' )
    log.flush()

    return ( proc.returncode, return_lines.splitlines() )
