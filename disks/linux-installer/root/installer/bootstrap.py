import re
import shutil
from procutils import execute, chroot_execute


def bootstrap( mount_point, source, profile, config ):
  bootstrap_type = profile.get( 'bootstrap', 'type' )

  # debians copy over /etc/hostname and /etc/resolv.conf, but cent dosen't (SELS Unknown), and pre_base_cmd is chrooted, so for now we will do these here
  shutil.copyfile( '/etc/hostname', '%s/etc/hostname' % mount_point )
  shutil.copyfile( '/etc/resolv.conf', '%s/etc/resolv.conf' % mount_point )

  if bootstrap_type == 'debbootstrap':
    execute( '/usr/sbin/debootstrap --arch amd64 %s %s %s' % ( profile.get( 'bootstrap', 'distro' ), mount_point, source ) )

  elif bootstrap_type == 'squashimg':
    version = profile.get( 'bootstrap', 'version' )
    print 'Downloading image...'
    execute( '/bin/wget -O /tmp/bootstrap.img %s%s/os/x86_64/images/install.img' % ( source, version ) )

    print 'Extractig image...'
    execute( '/bin/unsquashfs -f -d %s /tmp/bootstrap.img' % mount_point )
    execute( 'rm /tmp/bootstrap.img' )

    print 'Retreiving Package List...'
    execute( '/bin/wget -q -O /tmp/pkglist %s%s/os/x86_64/Packages/' % ( source, version ) )
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

    execute( 'rm /tmp/pkglist' ) # if there are more packages installed here, make sure to update the list in packaging.py - installBase
    print '   YUM rpm filename "%s"' % yum_filename
    print '   release rpm filename "%s"' % release_filename

    print 'Retreiving YUM...'
    execute( '/bin/wget -q -O /%s/tmp.rpm %s%s/os/x86_64/Packages/%s' % ( mount_point, source, version, yum_filename ) )
    print 'Instaiing YUM...'
    chroot_execute( '/usr/bin/rpm -i --nodeps /tmp.rpm' )
    chroot_execute( 'rm /tmp.rpm' )

    print 'Retreiving Release...'
    execute( '/bin/wget -q -O /%s/tmp.rpm %s%s/os/x86_64/Packages/%s' % ( mount_point, source, version, release_filename ) )
    print 'Instaiing Release...'
    chroot_execute( '/usr/bin/rpm -i --nodeps /tmp.rpm' )
    chroot_execute( 'rm /tmp.rpm' )

  else:
    raise Exception( 'Unknown "%s" type' % bootstrap_type )
