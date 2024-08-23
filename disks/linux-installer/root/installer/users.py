import shutil
import glob
import os
from configparser import NoOptionError, NoSectionError
from installer.procutils import chroot_execute


def setupUsers( mount_point, profile, value_map ):
  pass_hash = value_map.get( 'root_password_hash', '\\$6\\$rootroot\\$oLo.loyMV45VA7/0sKV5JH/xBAXiq/igL4hQrGz3yd9XUavmC82tZm1lxW2N.5eLxQUlqp53wXKRzifZApP0/1' )  # root

  chroot_execute( '/usr/sbin/usermod -p {0} root'.format( pass_hash ) )

  try:
    skel_dir = profile.get( 'users', 'skel_dir' )
  except ( NoOptionError, NoSectionError ):
    skel_dir = None

  if skel_dir is not None:
    src_dir = os.path.join( mount_point, skel_dir )
    dest_dir = os.path.join( mount_point, 'root' )
    for filename in glob.glob( '{0}*'.format( src_dir ) ):
      print( 'copy "{0}" to "{1}"'.format( filename, filename.replace( src_dir, dest_dir ) ) )
      shutil.copytree( filename, filename.replace( src_dir, dest_dir ), symlinks=True )

  for user in value_map.get( 'user_list', [] ):
    print( 'create user {0}'.format( user[ 'name' ] ) )
    name = user[ 'name' ]
    args = []
    try:
      args.append( '--group {0}'.format( user[ 'group' ] ) )
      chroot_execute( '/usr/sbin/addgroup {0}'.format( user[ 'group' ] ) )
    except KeyError:
      pass

    args.append( '--shell /bin/bash' )

    try:
      args.append( '--password {0}'.format( user[ 'password_hash' ] ) )
      args.append( '--create-home' )  # don't need a home directory if they don't have a password
      if skel_dir:
        args.append( '--skel {0}'.format( skel_dir ) )

    except KeyError:
      pass

    chroot_execute( '/usr/sbin/useradd {0} {1}'.format( ' '.join( args ), name ) )

    for item in user.get( 'sudo_list', [] ):
      open( '{0}/etc/sudoers'.format( mount_point ), 'a' ).write( '{0} {1}\n'.format( name, item ) )
