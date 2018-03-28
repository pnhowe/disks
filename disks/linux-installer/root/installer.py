#!/usr/bin/python -u
import os
import stat
import sys
import json
import optparse
import shutil

from platopxe.libplatopxe import PlatoPXE
from installer.procutils import open_output, set_chroot, execute, execute_lines, chroot_execute
from installer.filesystem import partition, mkfs, mount, remount, unmount, writefstab, fsConfigValues, grubConfigValues, MissingDrives, installFilesystemUtils
from installer.bootstrap import bootstrap
from installer.packaging import configSources, installBase, installOtherPackages, updatePackages, divert, undivert, preBaseSetup, cleanPackaging
from installer.booting import installBoot
from installer.config import getProfile, initConfig, writeShellHelper, baseConfig, fullConfig, updateConfig
from installer.users import setupUsers

STDOUT_OUTPUT = '/dev/instout'

oparser = optparse.OptionParser( description='Linux Installer', usage='Use either --distro and --version or --package.' )

oparser.add_option( '-d', '--distro', help='Distro [debian/centos/sles/opensles/rhel]', dest='distro' )
oparser.add_option( '-v', '--version', help='Distro Version [precise/5/...]', dest='version' )
oparser.add_option( '-p', '--package', help='Path to package, if --source is specified, this is a path on the source, other wise it is a local path.', dest='package' )
oparser.add_option( '-s', '--source', help='HTTP Source (ie: http://mirrors/ubuntu/)', dest='source' )
oparser.add_option( '-t', '--target', help='Install Target [drive/image/chroot] (default: drive)', dest='target', default='drive' )

( options, args ) = oparser.parse_args()

platopxe = PlatoPXE( host=os.environ.get( 'plato_host', 'plato' ), proxy=os.environ.get( 'plato_proxy', None ) )

open_output( STDOUT_OUTPUT )

if options.package and options.distro:
  print '--package and --distro can not be used at the same time.'
  sys.exit( 1 )

if options.package:
  if options.source:
    platopxe.postMessage( 'Downloading Install Package...' )
    execute( '/bin/wget -O /tmp/package %s%s' % ( options.source, options.package ) )
    options.package = '/tmp/package'

  if not os.access( options.package, os.R_OK ):
    raise Exception( 'Unable to find image package "%s"' % options.package )

  platopxe.postMessage( 'Extracting Install Package...' )
  os.makedirs( '/package' )
  execute( '/bin/tar -C /package --exclude=image* -xzf %s' % options.package )

  profile_file = '/package/profile'
  template_path = '/package/templates'

else:
  if options.distro not in os.listdir( '/profiles/' ):
    print 'Unknown Distro "%s"' % options.distro
    sys.exit( 1 )

  if options.version not in os.listdir( '/profiles/%s/' % options.distro ):
    print 'Unknown Version "%s" of Distro "%s"' % ( options.version, options.distro )
    sys.exit( 1 )

  if options.target not in ( 'drive', ):
    print 'Unknown Target "%s"' % options.target
    sys.exit( 1 )

  if not options.source:
    print 'Source required'
    sys.exit( 1 )

  profile_file = '/profiles/%s/%s' % ( options.distro, options.version )
  template_path = '/templates/%s/%s' % ( options.distro, options.version )

if os.access( '/genconfig.sh', os.X_OK ):
  platopxe.postMessage( 'Running genconfig...' )
  execute( '/genconfig.sh' )

config = {}
if not os.access( '/config.json', os.R_OK ):
  print 'Config file /config.json not found'
config = json.loads( open( '/config.json', 'r' ).read() )

if options.target == 'drive':
  install_root = '/target'

set_chroot( install_root )

platopxe.postMessage( 'Setting Up Configurator...' )
initConfig( install_root, template_path, profile_file )
updateConfig( 'filesystem', fsConfigValues() )

profile = getProfile()

platopxe.postMessage( 'Loading Kernel Modules...' )
for item in profile.items( 'kernel' ):
  if item[0].startswith( 'load_module_' ):
    execute( '/sbin/modprobe %s' % item[1] )

if options.target == 'drive':
  platopxe.postMessage( 'Partitioning....' )
  try:
    partition( profile, config )
  except MissingDrives as e:
    print 'Timeout while waiting for drive "%s"' % e
    sys.exit( 1 )

  platopxe.postMessage( 'Making Filesystems....' )
  mkfs()

updateConfig( 'filesystem', fsConfigValues() )

profile = getProfile() # reload now with the file system values

platopxe.postMessage( 'Mounting....' )
mount( install_root, profile )

if not options.package:
  platopxe.postMessage( 'Bootstrapping....' )
  bootstrap( install_root, options.source, profile, config )
  remount()

else:
  platopxe.postMessage( 'Extracting OS Image....' )
  name = execute_lines( '/bin/tar --wildcards image.* -ztf %s' % options.package )
  try:
    name = name[0]
  except IndexError:
    raise Exception( 'Unable to find image in package' )
  if name == 'image.cpio.gz':
    execute( '/bin/sh -c "cd %s; /bin/tar --wildcards image.* -Ozxf %s | /bin/gunzip | /bin/cpio -id"' % ( install_root, options.package ) )

  elif name == 'image.tar.gz':
    execute( '/bin/sh -c "cd %s; /bin/tar --wildcards image.* -Ozxf %s | /bin/gunzip | /bin/tar -xz"' % ( install_root, options.package ) )

  else:
    raise Exception( 'Unable to find image to extract' )

platopxe.postMessage( 'Writing fstab....' )
writefstab( install_root, profile )

writeShellHelper()

if not options.package:
  platopxe.postMessage( 'Setting Up Package Manager...' )
  configSources( install_root, profile, config )

  platopxe.postMessage( 'Writing Base OS Config...' )
  baseConfig( profile )

  if os.access( '/before.sh', os.X_OK ):
    platopxe.postMessage( 'Running Before Packages Task...' )
    execute( '/before.sh' )

  platopxe.postMessage( 'Setting up Diverts....' )
  divert( profile )

  platopxe.postMessage( 'Before Base Setup....' )
  preBaseSetup( profile )

  platopxe.postMessage( 'Installing Base...' )
  installBase( install_root, profile )
  remount()

  platopxe.postMessage( 'Setting Up Users....' )
  setupUsers( install_root, profile, config )

  updateConfig( 'bootloader', grubConfigValues( install_root ) )

  platopxe.postMessage( 'Installing Kernel/Boot Loader...' )
  installBoot( install_root, profile ) # bootloader before other packages, incase other packages pulls in bootloader before we have it configured

  platopxe.postMessage( 'Installing Packages...' )
  installOtherPackages( profile, config )

  platopxe.postMessage( 'Installing Filesystem Utils...' )
  installFilesystemUtils( profile )

  platopxe.postMessage( 'Updating Packages...' )
  updatePackages()

updateConfig( 'bootloader', grubConfigValues( install_root ) )

platopxe.postMessage( 'Writing Full Config...' )
fullConfig()

if not options.package:
  for item in profile.items( 'general' ):
    if item[0].startswith( 'after_cmd_' ):
      chroot_execute( item[1] )

  if os.access( '/after.sh', os.X_OK ):
    platopxe.postMessage( 'Running After Packages Task...' )
    shutil.copyfile( '/after.sh', '%s/after.sh' % install_root )
    os.chmod( '%s/after.sh' % install_root, os.stat( '%s/after.sh' % install_root ).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH )
    chroot_execute( '/after.sh "%s"' % ' '.join( fsConfigValues()['boot_drives'] ) )
    os.unlink( '%s/after.sh' % install_root )

  platopxe.postMessage( 'Removing Diverts...' )
  undivert( profile )

  platopxe.postMessage( 'Cleaning up...' )
  cleanPackaging()

else:
  platopxe.postMessage( 'Setting Up Users....' )
  setupUsers( install_root, profile, config )

  platopxe.postMessage( 'Running Package Setup...' )
  if not os.access( '/package/setup.sh', os.R_OK ):
    raise Exception( 'Unable to find package setup file' )

  shutil.copyfile( '/package/setup.sh', '%s/setup.sh' % install_root )
  os.chmod( '%s/setup.sh' % install_root, os.stat( '%s/setup.sh' % install_root ).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH )
  chroot_execute( '/setup.sh "%s"' % ' '.join( fsConfigValues()['boot_drives'] ) )
  os.unlink( '%s/setup.sh' % install_root )

os.unlink( '/target/config_data' )

if os.access( '%s/usr/sbin/configManager' % install_root, os.X_OK ):
  platopxe.postMessage( 'Running configManager...' )
  chroot_execute( '/usr/sbin/configManager -c -a -f -b' )

shutil.copyfile( '/tmp/output.log', '%s/root/install.log' % install_root )
shutil.copyfile( '/tmp/detail.log', '%s/root/install.detail.log' % install_root )

platopxe.postMessage( 'Unmounting...' )
unmount( install_root )

platopxe.postMessage( 'Done!' )
