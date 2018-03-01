import re
from subprocess import Popen, PIPE
from BIOSCfg import BIOSCfg

class SYSCfg( BIOSCfg ):
  class_map = { 'NW': 'Network', 'HDD': 'Harddrive', 'EFI': 'EFI', 'DVD': 'DVD' }

  @classmethod
  def detect( cls, baseboard ): # make sure to update bootstrap if these change
    if baseboard in ( 'S2600JF' ):
      return cls( 13 )

    return None


  def __init__( self, version, *args, **kwargs ):
    super( SYSCfg, self ).__init__( *args, **kwargs )
    self._configCache = {}
    if version == 13:
      self.cli = '/sbin/syscfg'
      self.version = 13
    else:
      raise Exception( 'Unknown syscfg version "%s"' % version )

    open( '/tmp/syscfg.log', 'a' ).write( 'Using "%s"\n' % self.cli )

  def _execute( self, cmd ):
    proc = Popen( [ self.cli ] + cmd, stdout=PIPE )
    ( stdout, _ ) = proc.communicate() # we don't care about return code, dosen't mean anything with syscfg, thanks a lot Intell

    open( '/tmp/syscfg.log', 'a' ).write( 'CMD: %s\nOutput: %s\n' % ( cmd, stdout ) )

    return stdout

  def _loadConfigCache( self ):
    self._configCache = {}
    self._execute( [ '/s', '/tmp/syscfg_save.ini', '/b' ] )

    category = None
    group = None
    tmp = open( '/tmp/syscfg_save.ini', 'r' )
    for line in tmp.readlines():
      result = re.match( '\[([^:\]]*)::([^\]]*)\]', line )
      if result:
        category = result.group( 1 )
        group = result.group( 2 )
        if category == 'BIOS':
          self._configCache[ group ] = {}
        continue

      result = re.match( '([^=]+)=(\w+)\s*;(.*)', line )
      if result:
        options = {}
        for part in result.group( 3 ).split( ':' )[1:]:
          result2 = re.match( '(.+)=(.+)', part.strip() )
          if result2:
            options[ result2.group( 2 ) ] = result2.group( 1 )

        if not options:
          options = None
        if category == 'BIOS':
          self._configCache[ group ][ result.group( 1 ).strip() ] = ( options.keys()[ options.values().index( result.group( 2 ).strip() ) ], options )
        continue

    tmp.close()

  def changePassword( self, newpassword ):
    out = self._execute( [ '/bap', self.password, newpassword ] )

    if out.splitlines()[-1] != 'Successfully Completed':
      return False

    self.password = newpassword

    return True

  def checkPassword( self ):
    out = self._execute( [ '/bap', self.password, self.password ] )

    if out.splitlines()[-1] != 'Successfully Completed':
      return False

    return True

  def setValue( self, group, item, value ):
    cmd = [ '/bcs', '', group, item, value ]
    if self.password:
      cmd[1] = self.password

    out = self._execute( cmd )

    if out.splitlines()[-1] != 'Successfully Completed':
      return False

    return True

  def getValue( self, group, item ):
    if not self._configCache:
      self._loadConfigCache()

    try:
      return self._configCache[ group ][ item ]
    except KeyError:
      return ( None, None )

  def getBoot( self ):
    out = self._execute( [ '/bbo' ] )

    lines = out.splitlines()

    result = []

    curclass = None
    for line in lines:
      if line.startswith( '::' ):
        tmp = line.split( '(' )[1].split( ')' )[0].strip()
        try:
          curclass = self.class_map[ tmp ]
        except KeyError:
          raise Exception( 'Unknown Booting class "%s"' % tmp )

      if not curclass:
        continue

      tmp = line.split( '.' )
      if len( tmp ) == 2:
        result.append( [ curclass, tmp[1].strip() ] )

    return result

  def setBoot( self, boot ):
    curent = self.getBoot()
    class_map = dict( zip( self.class_map.values(), self.class_map.keys() ) )

    try:
      tmp = [ class_map[ item[0] ] for item in boot ]
    except KeyError:
      raise Exception( 'Unknown boot class "%s"' % item[0] )

    target_class_order = [ tmp[0] ]
    for item in tmp:
      if target_class_order[-1] != item:
        target_class_order.append( item )

    try:
      tmp = [ class_map[ item[0] ] for item in curent ]
    except KeyError:
      raise Exception( 'Unknown boot class "%s"' % item[0] )

    curent_class_order = [ tmp[0] ]
    for item in tmp:
      if curent_class_order[-1] != item:
        curent_class_order.append( item )

    print 'curent %s  target %s' % ( curent_class_order, target_class_order )

    dirty = False

    if curent_class_order != target_class_order:
      print 'Setting Class boot order to %s...' % target_class_order
      cmd = [ '/bbo', '' ] + target_class_order
      if self.password:
        cmd[1] = self.password
      out = self._execute( cmd )
      if out.splitlines()[-1] not in ( 'Reordering boot devices using \'/bbo\' switch should be followed by an immediate system reset approximately with in one minute.', 'Successfully Completed' ):
        return False
      dirty = True

    curent_classes = {}
    target_classes = {}
    for item in curent:
      try:
        curent_classes[ class_map[ item[0] ] ].append( item[1] )
      except KeyError:
        curent_classes[ class_map[ item[0] ] ] = [ item[1] ]

    for item in boot:
      if len( item ) > 1:
        try:
          target_classes[ class_map[ item[0] ] ].append( item[1] )
        except KeyError:
          target_classes[ class_map[ item[0] ] ] = [ item[1] ]

    for tmp in target_classes:
      if tmp not in curent_classes:
        if not dirty:
          print 'Expected boot class not aviable, check BIOS settings to be sure the class is enabled, or something in the class is enabled'
          return False

        else:
          continue

      curent_order = curent_classes[ tmp ]
      target_order = target_classes[ tmp ]
      if curent_order == target_order:
        continue

      print "make %s like %s" % ( curent_order, target_order )

      cmd = [ tmp ]
      for item in target_order:
        cmd.append( str( 1 + curent_order.index( item ) ) )

      print 'Setting class boot order %s...' % cmd
      cmd = [ '/bbo', '' ] + cmd
      if self.password:
        cmd[1] = self.password
      out = self._execute( cmd )
      if out.splitlines()[-1] != 'Successfully Completed':
        return False

      dirty = True

    return True
