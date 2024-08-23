import os
import stat

from installer.procutils import chroot_execute
from installer.httputils import http_getfile


def postInstall( install_root, profile, value_map ):
  for item in profile.items( 'general' ):
    if item[0].startswith( 'after_cmd_' ):
      chroot_execute( "sh -c '{0}'".format( item[1] ) )

  if 'postinstall_script' in value_map:  # TODO: http proxy, see also bootstrap
    target_path = os.path.join( install_root, 'postinstall.sh' )
    open( target_path, 'w' ).write( http_getfile( value_map[ 'postinstall_script' ] ) )
    os.chmod( target_path, os.stat( target_path ).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH )
    chroot_execute( '/postinstall.sh' )
    os.unlink( target_path )

  for command in value_map.get( 'postinstall_command_list', [] ):
    chroot_execute( "sh -c '{0}'".format( command ) )
