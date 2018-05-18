import os
from configparser import NoOptionError
from installer.procutils import chroot_execute, execute_lines
from installer.packaging import installPackages
from installer.config import renderTemplates
from installer.filesystem import fsConfigValues


def installBoot( install_root, profile ):
  renderTemplates( profile.get( 'booting', 'bootloader_templates' ).split( ',' ) )

  if os.access( '/proc/mdstat', os.R_OK ):
    installPackages( profile.get( 'packages', 'mdadm_package' ) )

  if execute_lines( '/sbin/lvm pvs' ):
    installPackages( profile.get( 'packages', 'lvm_package' ) )

  installPackages( profile.get( 'packages', 'bootloader_package' ) )

  try:
    installPackages( profile.get( 'packages', 'kernel_package' ) )
  except NoOptionError:
    pass

  try:
    bootloader_install = profile.get( 'booting', 'bootloader_install' )
  except NoOptionError:
    bootloader_install = None

  if bootloader_install:
    for drive in fsConfigValues()[ 'boot_drives' ]:
      chroot_execute( bootloader_install.format( drive ) )
