import re
import shutil
import os
from configparser import ConfigParser, NoOptionError

from installer.procutils import execute, execute_lines, chroot_execute


def bootstrap( mount_point, source, profile ):  # TODO: bootstrap http proxy, also see misc
  bootstrap_type = profile.get( 'bootstrap', 'type' )

  # debians copy over /etc/hostname and /etc/resolv.conf, but cent dosen't (SELS Unknown), and pre_base_cmd is chrooted, so for now we will do these here
  shutil.copyfile( '/etc/hostname', os.path.join( mount_point, 'etc/hostname' ) )
  shutil.copyfile( '/etc/resolv.conf', os.path.join( mount_point, 'etc/resolv.conf' ) )

  try:
    packages = profile.get( 'bootstrap', 'packages' )
  except NoOptionError:
    packages = ''

  if bootstrap_type == 'debbootstrap':
    if packages:
      execute( '/usr/sbin/debootstrap --arch amd64 --include {0} {1} {2} {3}'.format( packages, profile.get( 'bootstrap', 'distro' ), mount_point, source ) )
    else:
      execute( '/usr/sbin/debootstrap --arch amd64 {0} {1} {2}'.format( profile.get( 'bootstrap', 'distro' ), mount_point, source ) )

  elif bootstrap_type == 'squashimg':  # we should change this to using the squash fs as a intermediat step to do a boot strap from scratch https://wiki.centos.org/HowTos/ManualInstall
    version = profile.get( 'bootstrap', 'version' )
    repo_root = '{0}{1}/os/x86_64/'.format( source, version )

    print( 'Getting Treeinfo...' )
    execute( '/bin/wget -O /tmp/treeinfo {0}.treeinfo'.format( repo_root ) )
    treeinfo = ConfigParser()
    treeinfo.read( '/tmp/treeinfo' )
    image = treeinfo.get( 'stage2', 'mainimage' )
    print( '   Stage 2 image "{0}"'.format( image ) )

    print( 'Downloading image...' )
    execute( '/bin/wget -O {0}/bootstrap.img {1}{2}'.format( mount_point, repo_root, image ) )

    print( 'Extractig image...' )
    file_list = execute_lines( '/bin/unsquashfs -d . -ls {0}/bootstrap.img'.format( mount_point ) )
    execute( '/bin/unsquashfs -f -d {0} {0}/bootstrap.img'.format( mount_point ) )
    os.unlink( '{0}/bootstrap.img'.format( mount_point ) )

    if file_list[ -1 ] == './LiveOS/rootfs.img':  # Centos 7
      os.rename( '{0}/LiveOS/rootfs.img'.format( mount_point ), '{0}/rootfs.img'.format( mount_point ) )
      os.rmdir( '{0}/LiveOS'.format( mount_point ) )
      os.mkdir( '/tmp/mnt' )
      execute( '/bin/mount -oloop,ro {0}/rootfs.img /tmp/mnt'.format( mount_point ) )
      execute( '/bin/cp -ar /tmp/mnt/. {0}'.format( mount_point ) )
      execute( '/bin/umount /tmp/mnt' )
      os.unlink( '{0}/rootfs.img'.format( mount_point ) )
      os.rmdir( '/tmp/mnt' )
      shutil.copyfile( '/etc/resolv.conf', os.path.join( mount_point, 'etc/resolv.conf' ) )  # get's overwritten by the rootfs

    print( 'Retreiving Package List...' )
    execute( '/bin/wget -q -O /tmp/pkglist {0}Packages/'.format( repo_root ) )
    yum_match = re.compile( '"(yum-[^"]*\.rpm)"' )
    release_match = re.compile( '"(centos-release-[^"]*\.rpm)"' )
    yum_filename = None
    release_filename = None
    for line in open( '/tmp/pkglist', 'r' ).readlines():
      tmp = yum_match.search( line )
      if tmp:
        yum_filename = tmp.group( 1 )

      tmp = release_match.search( line )
      if tmp:
        release_filename = tmp.group( 1 )

      if release_filename and yum_filename:
        break

    execute( 'rm /tmp/pkglist' )  # if there are more packages installed here, make sure to update the list in packaging.py - installBase
    print( '   YUM rpm filename "{0}"'.format( yum_filename ) )
    print( '   release rpm filename "{0}"'.format( release_filename ) )

    print( 'Retreiving YUM...' )
    execute( '/bin/wget -q -O {0} {1}Packages/{2}'.format( os.path.join( mount_point, 'tmp.rpm' ), repo_root, yum_filename ) )
    print( 'Instaing YUM...' )
    chroot_execute( '/usr/bin/rpm -i --nodeps /tmp.rpm' )
    chroot_execute( 'rm /tmp.rpm' )

    print( 'Retreiving Release...' )
    execute( '/bin/wget -q -O {0} {1}Packages/{2}'.format( os.path.join( mount_point, 'tmp.rpm' ), repo_root, release_filename ) )
    print( 'Instaiing Release...' )
    chroot_execute( '/usr/bin/rpm -i --nodeps /tmp.rpm' )
    chroot_execute( 'rm /tmp.rpm' )

  else:
    raise Exception( 'Unknown "{0}" type'.format( bootstrap_type ) )
