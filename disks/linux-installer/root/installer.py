#!/usr/bin/env python3
import os
import stat
import sys
import shutil

from contractor.client import getClient
from installer.procutils import open_output, set_chroot, execute, execute_lines, chroot_execute
from installer.filesystem import partition, mkfs, mount, remount, unmount, writefstab, fsConfigValues, grubConfigValues, MissingDrives, installFilesystemUtils
from installer.bootstrap import bootstrap
from installer.packaging import configSources, installBase, installOtherPackages, updatePackages, divert, undivert, preBaseSetup, cleanPackaging
from installer.booting import installBoot
from installer.misc import postInstallScripts
from installer.config import getProfile, initConfig, writeShellHelper, baseConfig, fullConfig, updateConfig, getValues
from installer.users import setupUsers

STDOUT_OUTPUT = '/dev/instout'

contractor = getClient()

config = contractor.getConfig()
image_package = config.get( 'image_package', None )
image_package_location = config.get( 'image_package_location', None )

distro = config.get( 'distro', None )
distro_version = config.get( 'distro_version', None )
bootstrap_source = config.get( 'bootstrap_source', None )

target = config.get( 'install_target', 'drive' )

open_output( STDOUT_OUTPUT )

if image_package and distro:
  print( '"image_package" and "distro" can not be specified at the same time.' )
  sys.exit( 1 )

if image_package:
  if image_package_location:
    contractor.postMessage( 'Downloading Install Package...' )
    execute( '/bin/wget -O /tmp/package {0}{1}'.format( image_package_location, image_package ) )
    image_package = '/tmp/package'

  if not os.access( image_package, os.R_OK ):
    raise Exception( 'Unable to find image package "{0}"'.format( image_package ) )

  contractor.postMessage( 'Extracting Install Package...' )
  os.makedirs( '/package' )
  execute( '/bin/tar -C /package --exclude=image* -xzf {0}'.format( image_package ) )

  profile_file = '/package/profile'
  template_path = '/package/templates'

else:
  if distro not in os.listdir( '/profiles/' ):
    print( 'Unknown Distro "{0}"'.format( distro ) )
    sys.exit( 1 )

  if distro_version not in os.listdir( os.path.join( '/profiles', distro ) ):
    print( 'Unknown Version "{0}" of Distro "{1}"'.format( distro_version, distro ) )
    sys.exit( 1 )

  if target not in ( 'drive', ):
    print( 'Unknown Target "{0}"'.format( target ) )
    sys.exit( 1 )

  if not bootstrap_source:
    print( 'bootstrap_source is required when installing via distro' )
    sys.exit( 1 )

  profile_file = os.path.join( '/profiles', distro, distro_version )
  template_path = os.path.join( '/templates', distro, distro_version )
  print( 'Using profile "{0}"'.format( profile_file ) )
  print( 'Using template "{0}"'.format( template_path ) )

if target == 'drive':
  install_root = '/target'

set_chroot( install_root )

contractor.postMessage( 'Setting Up Configurator...' )
initConfig( install_root, template_path, profile_file )
updateConfig( 'filesystem', fsConfigValues() )

value_map = getValues()

profile = getProfile()

contractor.postMessage( 'Loading Kernel Modules...' )
for item in profile.items( 'kernel' ):
  if item[0].startswith( 'load_module_' ):
    execute( '/sbin/modprobe {0}'.format( item[1] ) )

if target == 'drive':
  contractor.postMessage( 'Partitioning....' )
  try:
    partition( profile, value_map )
  except MissingDrives as e:
    print( 'Timeout while waiting for drive "{0}"'.format( e ) )
    sys.exit( 1 )

  contractor.postMessage( 'Making Filesystems....' )
  mkfs()

updateConfig( 'filesystem', fsConfigValues() )

profile = getProfile()  # reload now with the file system values

contractor.postMessage( 'Mounting....' )
mount( install_root, profile )

if not image_package:
  contractor.postMessage( 'Bootstrapping....' )
  bootstrap( install_root, bootstrap_source, profile )
  remount()

else:
  contractor.postMessage( 'Extracting OS Image....' )
  name = execute_lines( '/bin/tar --wildcards image.* -ztf {0}'.format( image_package ) )
  try:
    name = name[0]
  except IndexError:
    raise Exception( 'Unable to find image in package' )
  if name == 'image.cpio.gz':
    execute( '/bin/sh -c "cd {0}; /bin/tar --wildcards image.* -Ozxf {1} | /bin/gunzip | /bin/cpio -id"'.format( install_root, image_package ) )

  elif name == 'image.tar.gz':
    execute( '/bin/sh -c "cd {0}; /bin/tar --wildcards image.* -Ozxf {1} | /bin/gunzip | /bin/tar -xz"'.format( install_root, image_package ) )

  else:
    raise Exception( 'Unable to find image to extract' )

contractor.postMessage( 'Writing fstab....' )
writefstab( install_root, profile )

writeShellHelper()

if not image_package:
  contractor.postMessage( 'Setting Up Package Manager...' )
  configSources( install_root, profile, value_map )

  contractor.postMessage( 'Writing Base OS Config...' )
  baseConfig( profile )

  contractor.postMessage( 'Setting up Diverts....' )
  divert( profile )

  contractor.postMessage( 'Before Base Setup....' )
  preBaseSetup( profile, value_map )

  contractor.postMessage( 'Installing Base...' )
  installBase( install_root, profile )
  remount()

  contractor.postMessage( 'Setting Up Users....' )
  setupUsers( install_root, profile, value_map )

  updateConfig( 'bootloader', grubConfigValues( install_root ) )

  contractor.postMessage( 'Installing Kernel/Boot Loader...' )
  installBoot( install_root, profile )  # bootloader before other packages, incase other packages pulls in bootloader before we have it configured

  contractor.postMessage( 'Installing Packages...' )
  installOtherPackages( profile, value_map )

  contractor.postMessage( 'Installing Filesystem Utils...' )
  installFilesystemUtils( profile )

  contractor.postMessage( 'Updating Packages...' )
  updatePackages()

updateConfig( 'bootloader', grubConfigValues( install_root ) )

contractor.postMessage( 'Writing Full Config...' )
fullConfig()

if not image_package:
  for item in profile.items( 'general' ):
    if item[0].startswith( 'after_cmd_' ):
      chroot_execute( item[1] )

  contractor.postMessage( 'Post Install Scripts...' )
  postInstallScripts( install_root, value_map )

  contractor.postMessage( 'Removing Diverts...' )
  undivert( profile )

  contractor.postMessage( 'Cleaning up...' )
  cleanPackaging( install_root )

else:
  contractor.postMessage( 'Setting Up Users....' )
  setupUsers( install_root, profile, value_map )

  contractor.postMessage( 'Running Package Setup...' )
  if not os.access( '/package/setup.sh', os.R_OK ):
    raise Exception( 'Unable to find package setup file' )

  setup_path = os.path.join( install_root, 'setup.sh' )
  shutil.copyfile( '/package/setup.sh', setup_path )
  os.chmod( setup_path, os.stat( setup_path ).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH )
  chroot_execute( '/setup.sh "{0}"'.format( ' '.join( fsConfigValues()[ 'boot_drives' ] ) ) )
  os.unlink( setup_path )

os.unlink( '/target/config_data' )

if os.access( os.path.join( install_root, 'usr/sbin/config-curator' ), os.X_OK ):
  contractor.postMessage( 'Running config-curator...' )
  chroot_execute( '/usr/sbin/config-curator -c -a -f -b' )

shutil.copyfile( '/tmp/output.log', os.path.join( install_root, 'root/install.log' ) )
shutil.copyfile( '/tmp/detail.log', os.path.join( install_root, 'root/install.detail.log' ) )

contractor.postMessage( 'Unmounting...' )
unmount( install_root, profile )

contractor.postMessage( 'Done!' )
