from procutils import chroot_execute, execute
from httputils import http_getfile
from config import renderTemplates

manager_type = None
divert_list = []

def configSources( install_root, profile, config ):
  global manager_type
  manager_type = profile.get( 'packaging', 'manager_type' )

  if manager_type not in ( 'apt', 'yum', 'zypper' ):
    raise Exception( 'Unknwon Packaging manager type "%s"' % manager_type )

  if manager_type == 'yum':
    execute( 'ash -c "rm %s/etc/yum.repos.d/*"' % install_root )

  key_uris = []
  for repo in config['repo_list']:
    if 'key_uri' in repo:
      if repo['type'] != manager_type:
        continue

      uri = repo['key_uri']
      if uri not in key_uris:
        try:
          proxy = repo['proxy']
        except:
          proxy = None
        tmp = http_getfile( uri, proxy=proxy )

        if manager_type == 'apt':
          chroot_execute( '/usr/bin/apt-key add -', tmp )

        if 'key_file' in repo:
          open( '%s%s' % ( install_root, repo['key_file'] ), 'w' ).write( tmp )

        key_uris.append( uri )

  renderTemplates( profile.get( 'packaging', 'source_templates' ).split( ',' ) )

  print 'Updating Repo Data...'
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get update' )

  #yum dosen't need a repo update


def installBase( install_root, profile ):
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get install -q -y %s' % profile.get( 'packaging', 'base' ) )

  elif manager_type == 'yum':
    chroot_execute( '/usr/bin/yum -y groupinstall %s' % profile.get( 'packaging', 'base' ) )
    chroot_execute( '/usr/bin/yum -y reinstall yum centos-release' ) # find a better way to figure out what needs to be re-installed
    execute( 'ash -c "rm %s/etc/yum.repos.d/*"' % install_root ) # clean up extra repos that some package might have left behind... this is the last time we will do this.... any package after this we will allow to keep their repos, we are really just after the base ones
    renderTemplates( profile.get( 'packaging', 'source_templates' ).split( ',' ) )

  elif manager_type == 'zypper':
    chroot_execute( '/usr/bin/zypper --non-interactive install %s' % profile.get( 'packaging', 'base' ) )


def installOtherPackages( profile, config ):
  package_list = profile.get( 'packaging', 'packages' )
  try:
    package_list += ' %s' % config['packages']
  except KeyError:
    pass

  package_list = package_list.strip()

  tmp = []
  for package in package_list.split( ' ' ):
    parts = package.split( ':' )

    if len( parts ) == 1:
      tmp.append( package )
    elif parts[0] == manager_type:
      tmp.append( parts[1] )

  if package_list:
    installPackages( ' '.join( tmp ) )


def installPackages( packages ):
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get install -q -y %s' % packages )
  elif manager_type == 'yum':
    chroot_execute( '/usr/bin/yum -y install %s' % packages  )
  elif manager_type == 'zypper':
    chroot_execute( '/usr/bin/zypper --non-interactive install %s' % packages )


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
      chroot_execute( '/usr/bin/dpkg-divert --divert %s.linux-installer --rename %s' % ( name, name ) )
      chroot_execute( 'ln -s %s %s' % ( target, name ) )


def undivert( profile ):
  if manager_type == 'apt':
    for ( name, target ) in divert_list:
      chroot_execute( 'rm %s' % name )
      chroot_execute( '/usr/bin/dpkg-divert --rename --remove %s' % name )


def preBaseSetup( profile ):
  renderTemplates( profile.get( 'packaging', 'prebase_templates' ).split( ',' ) )

  if manager_type == 'apt':
    for item in profile.items( 'packaging' ):
      if item[0].startswith( 'selection_' ):
        chroot_execute( '/usr/bin/debconf-set-selections', item[1] )

  for item in profile.items( 'packaging' ):
    if item[0].startswith( 'prebase_cmd_' ):
      chroot_execute( item[1] )


def cleanPackaging():
  if manager_type == 'apt':
    chroot_execute( '/usr/bin/apt-get clean' )
  elif manager_type == 'yum':
    chroot_execute( '/usr/bin/yum clean all' )
    execute( '/bin/find -name *.rpmnew -exec rm {} \;' )
    execute( '/bin/find -name *.rpmsave -exec rm {} \;' )
  elif manager_type == 'zypper':
    chroot_execute( '/usr/bin/zypper --non-interactive clean' )
    execute( '/bin/find -name *.rpmnew -exec rm {} \;' )
    execute( '/bin/find -name *.rpmsave -exec rm {} \;' )
