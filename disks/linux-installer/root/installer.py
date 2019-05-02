#!/usr/bin/env python3
import os
import stat
import sys
import optparse
import shutil

from controller.client import getClient
from installer.procutils import open_output, set_chroot, execute, execute_lines, chroot_execute
from installer.filesystem import partition, mkfs, mount, remount, unmount, writefstab, fsConfigValues, grubConfigValues, MissingDrives, installFilesystemUtils
from installer.bootstrap import bootstrap
from installer.packaging import configSources, installBase, installOtherPackages, updatePackages, divert, undivert, preBaseSetup, cleanPackaging
from installer.booting import installBoot
from installer.misc import postInstallScripts
from installer.config import getProfile, initConfig, writeShellHelper, baseConfig, fullConfig, updateConfig, getValues
from installer.users import setupUsers

STDOUT_OUTPUT = '/dev/instout'

oparser = optparse.OptionParser( description='Linux Installer', usage='Use either --distro and --version or --package.' )

oparser.add_option( '-d', '--distro', help='Distro [debian/centos/sles/opensles/rhel]', dest='distro' )
oparser.add_option( '-v', '--version', help='Distro Version [precise/5/...]', dest='version' )
oparser.add_option( '-p', '--package', help='Path to package, if --source is specified, this is a path on the source, other wise it is a local path.', dest='package' )
oparser.add_option( '-s', '--source', help='HTTP Source (ie: http://mirrors/ubuntu/)', dest='source' )
oparser.add_option( '-t', '--target', help='Install Target [drive/image/chroot] (default: drive)', dest='target', default='drive' )

( options, args ) = oparser.parse_args()

controller = getClient()

open_output( STDOUT_OUTPUT )

if options.package and options.distro:
  print( '--package and --distro can not be used at the same time.' )
  sys.exit( 1 )

if options.package:
  if options.source:
    controller.postMessage( 'Downloading Install Package...' )
    execute( '/bin/wget -O /tmp/package {0}{1}'.format(  options.source, options.package ) )
    options.package = '/tmp/package'

  if not os.access( options.package, os.R_OK ):
    raise Exception( 'Unable to find image package "{0}"'.format( options.package ) )

  controller.postMessage( 'Extracting Install Package...' )
  os.makedirs( '/package' )
  execute( '/bin/tar -C /package --exclude=image* -xzf {0}'.format( options.package ) )

  profile_file = '/package/profile'
  template_path = '/package/templates'

else:
  if options.distro not in os.listdir( '/profiles/' ):
    print( 'Unknown Distro "{0}"'.format( options.distro ) )
    sys.exit( 1 )

  if options.version not in os.listdir( os.path.join( '/profiles', options.distro ) ):
    print( 'Unknown Version "{0}" of Distro "{1}"'.format( options.version, options.distro ) )
    sys.exit( 1 )

  if options.target not in ( 'drive', ):
    print( 'Unknown Target "{0}"'.format( options.target ) )
    sys.exit( 1 )

  if not options.source:
    print( 'Source required' )
    sys.exit( 1 )

  profile_file = os.path.join( '/profiles', options.distro, options.version )
  template_path = os.path.join( '/templates', options.distro, options.version )
  print( 'Using profile "{0}"'.format( profile_file ) )
  print( 'Using template "{0}"'.format( template_path ) )

if options.target == 'drive':
  install_root = '/target'

set_chroot( install_root )

controller.postMessage( 'Setting Up Configurator...' )
initConfig( install_root, template_path, profile_file )
updateConfig( 'filesystem', fsConfigValues() )

value_map = getValues()

profile = getProfile()

controller.postMessage( 'Loading Kernel Modules...' )
for item in profile.items( 'kernel' ):
  if item[0].startswith( 'load_module_' ):
    execute( '/sbin/modprobe {0}'.format( item[1] ) )

if options.target == 'drive':
  controller.postMessage( 'Partitioning....' )
  try:
    partition( profile, value_map )
  except MissingDrives as e:
    print( 'Timeout while waiting for drive "{0}"'.format( e ) )
    sys.exit( 1 )

  controller.postMessage( 'Making Filesystems....' )
  mkfs()

updateConfig( 'filesystem', fsConfigValues() )

profile = getProfile()  # reload now with the file system values

controller.postMessage( 'Mounting....' )
mount( install_root, profile )

if not options.package:
  controller.postMessage( 'Bootstrapping....' )
  bootstrap( install_root, options.source, profile )
  remount()

else:
  controller.postMessage( 'Extracting OS Image....' )
  name = execute_lines( '/bin/tar --wildcards image.* -ztf {0}'.format( options.package ) )
  try:
    name = name[0]
  except IndexError:
    raise Exception( 'Unable to find image in package' )
  if name == 'image.cpio.gz':
    execute( '/bin/sh -c "cd {0}; /bin/tar --wildcards image.* -Ozxf {1} | /bin/gunzip | /bin/cpio -id"'.format( install_root, options.package ) )

  elif name == 'image.tar.gz':
    execute( '/bin/sh -c "cd {0}; /bin/tar --wildcards image.* -Ozxf {1} | /bin/gunzip | /bin/tar -xz"'.format( install_root, options.package ) )

  else:
    raise Exception( 'Unable to find image to extract' )

controller.postMessage( 'Writing fstab....' )
writefstab( install_root, profile )

writeShellHelper()

if not options.package:
  controller.postMessage( 'Setting Up Package Manager...' )
  configSources( install_root, profile, value_map )

  controller.postMessage( 'Writing Base OS Config...' )
  baseConfig( profile )

  controller.postMessage( 'Setting up Diverts....' )
  divert( profile )

  controller.postMessage( 'Before Base Setup....' )
  preBaseSetup( profile )

  controller.postMessage( 'Installing Base...' )
  installBase( install_root, profile )
  remount()

  controller.postMessage( 'Setting Up Users....' )
  setupUsers( install_root, profile, value_map )

  updateConfig( 'bootloader', grubConfigValues( install_root ) )

  controller.postMessage( 'Installing Kernel/Boot Loader...' )
  installBoot( install_root, profile )  # bootloader before other packages, incase other packages pulls in bootloader before we have it configured

  controller.postMessage( 'Installing Packages...' )
  installOtherPackages( profile, value_map )

  controller.postMessage( 'Installing Filesystem Utils...' )
  installFilesystemUtils( profile )

  controller.postMessage( 'Updating Packages...' )
  updatePackages()

updateConfig( 'bootloader', grubConfigValues( install_root ) )

controller.postMessage( 'Writing Full Config...' )
fullConfig()

if not options.package:
  for item in profile.items( 'general' ):
    if item[0].startswith( 'after_cmd_' ):
      chroot_execute( item[1] )

  controller.postMessage( 'Post Install Scripts...' )
  postInstallScripts( install_root, value_map )

  controller.postMessage( 'Removing Diverts...' )
  undivert( profile )

  controller.postMessage( 'Cleaning up...' )
  cleanPackaging( install_root )

else:
  controller.postMessage( 'Setting Up Users....' )
  setupUsers( install_root, profile, value_map )

  controller.postMessage( 'Running Package Setup...' )
  if not os.access( '/package/setup.sh', os.R_OK ):
    raise Exception( 'Unable to find package setup file' )

  setup_path = os.path.join( install_root, 'setup.sh' )
  shutil.copyfile( '/package/setup.sh', setup_path )
  os.chmod( setup_path, os.stat( setup_path ).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH )
  chroot_execute( '/setup.sh "{0}"'.format( ' '.join( fsConfigValues()[ 'boot_drives' ] ) ) )
  os.unlink( setup_path )

os.unlink( '/target/config_data' )

if os.access( os.path.join( install_root, 'usr/sbin/config-curator' ), os.X_OK ):
  controller.postMessage( 'Running config-curator...' )
  chroot_execute( '/usr/sbin/config-curator -c -a -f -b' )

shutil.copyfile( '/tmp/output.log', os.path.join( install_root, 'root/install.log' ) )
shutil.copyfile( '/tmp/detail.log', os.path.join( install_root, 'root/install.detail.log' ) )

controller.postMessage( 'Unmounting...' )
unmount( install_root, profile )

controller.postMessage( 'Done!' )
