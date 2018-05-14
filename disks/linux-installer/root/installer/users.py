import shutil
import glob
import os
from configparser import NoOptionError, NoSectionError
from installer.procutils import chroot_execute


def setupUsers( mount_point, profile, config ):
  pass_hash = '\\$1\\$AP1PHkKl\\$4G/8A4Zi5S8CkRfG6iWYZ.'  # 0skin3rd
  try:
    if config[ 'root_pass' ]:
      pass_hash = config[ 'root_pass' ]
  except Exception:
    pass

  chroot_execute( '/usr/sbin/usermod -p {0} root'.format( pass_hash ) )

  try:
    skel_dir = profile.get( 'users', 'skel_dir' )
  except ( NoOptionError, NoSectionError ):
    return  # no skeldir, nothing to copy

  src_dir = os.path.join( mount_point, skel_dir )
  dest_dir = os.path.join( mount_point, 'root' )
  for filename in glob.glob( '{0}*'.format( src_dir ) ):
    print( 'copy "{0}" to "{1}"'.format( filename, filename.replace( src_dir, dest_dir ) ) )
    shutil.copytree( filename, filename.replace( src_dir, dest_dir ) )
