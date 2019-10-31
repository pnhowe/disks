import os
import stat

from installer.procutils import chroot_execute
from installer.httputils import http_getfile


def postInstallScripts( install_root, value_map ):
  if 'postinstall_script' in value_map:  # TODO: http proxy, see also bootstrap
    target_path = os.path.join( install_root, 'postinstall' )
    open( target_path, 'w' ).write( http_getfile( value_map[ 'postinstall_script' ] ) )
    os.chmod( target_path, os.stat( target_path ).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH )
    chroot_execute( '/postinstall' )
    os.unlink( target_path )

  for command in value_map.get( 'postinstall_commands', [] ):
    chroot_execute( "sh -c '" + command + "'" )
