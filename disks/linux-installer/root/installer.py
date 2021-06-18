#!/usr/bin/env python3
import os
import sys
import shutil

from contractor.client import getClient
from installer.procutils import open_output, set_chroot, execute, chroot_execute
from installer.filesystem import partition, mkfs, mount, remount, unmount, writefstab, fsConfigValues, grubConfigValues, MissingDrives, installFilesystemUtils
from installer.bootstrap import bootstrap
from installer.packaging import configSources, installBase, installOtherPackages, updatePackages, divert, undivert, preBaseSetup, cleanPackaging
from installer.booting import installBoot
from installer.misc import postInstall
from installer.config import getProfile, initConfig, writeShellHelper, baseConfig, fullConfig, updateConfig, getValues
from installer.users import setupUsers

STDOUT_OUTPUT = '/dev/instout'

contractor = getClient()

config = contractor.getConfig()

target = config.get( 'install_target', 'drive' )

open_output( STDOUT_OUTPUT )

profile_file = '/profile'
template_path = '/templates'

if not os.path.exists( profile_file ):
  print( 'Profile is missing' )
  sys.exit( 1 )

if not os.path.exists( template_path ):
  print( 'Profile is missing' )
  sys.exit( 1 )

if target not in ( 'drive', ):  # eventually also support a chroot
  print( 'Unknown Target "{0}"'.format( target ) )
  sys.exit( 1 )

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

if profile.get( 'bootstrap', 'type' ) is not None:
  bootstrap_source = config.get( 'bootstrap_source', None )
  if not bootstrap_source:
    print( 'bootstrap_source is required when installing via bootstrap' )
    sys.exit( 1 )

  contractor.postMessage( 'Bootstrapping....' )
  bootstrap( install_root, bootstrap_source, profile )
  remount()

contractor.postMessage( 'Writing fstab....' )
writefstab( install_root, profile )

writeShellHelper()

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

contractor.postMessage( 'Post Install...' )
postInstall( install_root, profile, value_map )

contractor.postMessage( 'Removing Diverts...' )
undivert( profile )

contractor.postMessage( 'Cleaning up...' )
cleanPackaging( install_root )

os.unlink( '/target/config_data' )

if os.access( os.path.join( install_root, 'usr/sbin/config-curator' ), os.X_OK ):
  contractor.postMessage( 'Running config-curator...' )
  shutil.copytree( template_path, os.path.join( install_root, 'var/lib/config-curator/templates/linux-installer/' ), symlinks=True )
  chroot_execute( '/usr/sbin/config-curator -c -a -f -b' )

shutil.copyfile( '/tmp/output.log', os.path.join( install_root, 'root/install.log' ) )
shutil.copyfile( '/tmp/detail.log', os.path.join( install_root, 'root/install.detail.log' ) )

contractor.postMessage( 'Unmounting...' )
unmount( install_root, profile )

contractor.postMessage( 'Done!' )
