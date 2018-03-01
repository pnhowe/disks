import shutil
import glob
from ConfigParser import NoOptionError, NoSectionError
from procutils import chroot_execute

def setupUsers( mount_point, profile, config ):
  pass_hash = '\\$1\\$AP1PHkKl\\$4G/8A4Zi5S8CkRfG6iWYZ.' # 0skin3rd
  try:
    if config['root_pass']:
      pass_hash = config['root_pass']
  except:
    pass

  chroot_execute( '/usr/sbin/usermod -p %s root' % pass_hash )

  try:
    skel_dir = profile.get( 'users', 'skel_dir' )
  except ( NoOptionError, NoSectionError ):
    skel_dir = None

  src_dir = '%s%s' % ( mount_point, skel_dir )
  dest_dir = '%s/root' % mount_point
  for filename in glob.glob( '%s*' % src_dir ):
    print 'copy %s to %s' % ( filename, filename.replace( src_dir, dest_dir ) )
    shutil.copytree( filename, filename.replace( src_dir, dest_dir ) )
