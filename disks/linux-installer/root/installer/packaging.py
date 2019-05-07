import os
from installer.procutils import chroot_execute, execute, chroot_env
from installer.httputils import http_getfile
from installer.config import renderTemplates

manager_type = None
divert_list = []


def configSources( install_root, profile, value_map ):
  global manager_type
  manager_type = profile.get( 'packaging', 'manager_type' )

  if manager_type not in ( 'apt', 'yum', 'zypper' ):
    raise Exception( 'Unknwon Packaging manager type "{0}"'.format( manager_type ) )

  if manager_type == 'yum':
    execute( 'ash -c "rm {0}/etc/yum.repos.d/*"'.format( install_root ) )

  key_uris = []
  for repo in value_map[ 'repo_list' ]:
    if 'key_uri' in repo:
      if repo[ 'type' ] != manager_type:
        continue

      uri = repo[ 'key_uri' ]
      if uri not in key_uris:
        try:
          proxy = repo[ 'proxy' ]
        except Exception:
          proxy = None

        tmp = http_getfile( uri, proxy=proxy )

        if manager_type == 'apt':
          chroot_execute( '/usr/bin/apt-key add -', tmp.decode() )

        if 'key_file' in repo:
          key_file_path = '{0}/{1}'.format( install_root, repo[ 'key_file' ] )
          if not os.path.isdir( os.path.dirname( key_file_path ) ):
            os.makedirs( os.path.dirname( key_file_path ) )
          open( key_file_path, 'wb' ).write( tmp )

        key_uris.append( uri )

  renderTemplates( profile.get( 'packaging', 'source_templates' ).split( ',' ) )

  print( 'Updating Repo Data...' )
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get update' )

  # yum dosen't need a repo update


def installBase( install_root, profile ):
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get install -q -y {0}'.format( profile.get( 'packaging', 'base' ) ) )

  elif manager_type == 'yum':
    chroot_execute( '/usr/bin/yum -y groupinstall {0}'.format( profile.get( 'packaging', 'base' ) ) )
    chroot_execute( '/usr/bin/yum -y reinstall yum centos-release' )  # find a better way to figure out what needs to be re-installed
    execute( 'ash -c "rm {0}/etc/yum.repos.d/*"'.format( install_root ) )  # clean up extra repos that some package might have left behind... this is the last time we will do this.... any package after this we will allow to keep their repos, we are really just after the base ones
    renderTemplates( profile.get( 'packaging', 'source_templates' ).split( ',' ) )

  elif manager_type == 'zypper':
    chroot_execute( '/usr/bin/zypper --non-interactive install {0}'.format( profile.get( 'packaging', 'base' ) ) )


def installOtherPackages( profile, value_map ):
  package_list = profile.get( 'packaging', 'packages' ).split( ' ' )
  try:
    package_list += value_map[ 'package_list' ]
  except KeyError:
    pass

  tmp = []
  for package in package_list:
    parts = package.split( ':' )

    if len( parts ) == 1:
      tmp.append( package )
    elif parts[0] == manager_type:
      tmp.append( parts[1] )

  if package_list:
    installPackages( ' '.join( tmp ) )


def installPackages( packages ):
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get install -q -y {0}'.format( packages ) )
  elif manager_type == 'yum':
    chroot_execute( '/usr/bin/yum -y install {0}'.format( packages ) )
    chroot_execute( '/usr/bin/rpm --query {0}'.format( packages ) )  # yum does not return error if something does not exist, so check to see if what we saied we wanted installed is installed
  elif manager_type == 'zypper':
    chroot_execute( '/usr/bin/zypper --non-interactive install {0}'.format( packages ) )


def updatePackages():
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get update' )
    chroot_execute( '/usr/bin/apt-get upgrade -q -y' )
  elif manager_type == 'yum':
    chroot_execute( '/usr/bin/yum -y update' )
  elif manager_type == 'zypper':
    chroot_execute( '/usr/bin/zypper --non-interactive update' )


def divert( profile ):
  global divert_list

  for item in profile.items( 'packaging' ):
    if item[0].startswith( 'divert_' ):
      ( name, target ) = item[1].split( ':' )
      divert_list.append( ( name.strip(), target.strip() ) )

  if manager_type == 'apt':
    for ( name, target ) in divert_list:
      chroot_execute( '/usr/bin/dpkg-divert --divert {0}.linux-installer --rename {0}'.format( name ) )
      chroot_execute( 'ln -s {0} {1}'.format( target, name ) )


def undivert( profile ):
  if manager_type == 'apt':
    for ( name, target ) in divert_list:
      chroot_execute( 'rm {0}'.format( name ) )
      chroot_execute( '/usr/bin/dpkg-divert --rename --remove {0}'.format( name ) )


def preBaseSetup( profile ):
  renderTemplates( profile.get( 'packaging', 'prebase_templates' ).split( ',' ) )

  for item in profile.items( 'packaging' ):
    if item[0].startswith( 'install_env_var_' ):
      ( name, value ) = item[1].split( ':', 1 )
      chroot_env[ name ] = value  # tehinically we are affecting all the chroot commands, for now this is ok.

  if manager_type == 'apt':
    for item in profile.items( 'packaging' ):
      if item[0].startswith( 'selection_' ):
        chroot_execute( '/usr/bin/debconf-set-selections', item[1] )

  for item in profile.items( 'packaging' ):
    if item[0].startswith( 'prebase_cmd_' ):
      chroot_execute( item[1] )


def cleanPackaging( install_root ):
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get clean' )

  elif manager_type == 'yum':
    chroot_execute( '/usr/bin/yum clean all' )
    execute( '/bin/find {0} \( -path {0}/proc -o -path {0}/sys \) -prune -o -name *.rpmnew -exec rm {{}} \;'.format( install_root ) )
    execute( '/bin/find {0} \( -path {0}/proc -o -path {0}/sys \) -prune -o -name *.rpmsave -exec rm {{}} \;'.format( install_root ) )

  elif manager_type == 'zypper':
    chroot_execute( '/usr/bin/zypper --non-interactive clean' )
    execute( '/bin/find {0} \( -path {0}/proc -o -path {0}/sys \) -prune -o -name *.rpmnew -exec rm {{}} \;'.format( install_root ) )
    execute( '/bin/find {0} \( -path {0}/proc -o -path {0}/sys \) -prune -o -name *.rpmsave -exec rm {{}} \;'.format( install_root ) )
