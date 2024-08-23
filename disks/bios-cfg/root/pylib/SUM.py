from subprocess import Popen, PIPE
import re
from BIOSCfg import BIOSCfg


class SUM( BIOSCfg ):
  class_map = { 'Network': 'Network', 'Hard Disk': 'Harddrive', 'UEFI': 'EFI', 'CD/DVD': 'DVD' }
  class_name = { 'Network special boot instance': 'Network', 'HDD special boot instance': 'Harddrive', 'CD/DVD special boot instance': 'DVD' }

  @classmethod
  def detect( cls, baseboard ):  # make sure to update bootstrap if these change
    if baseboard.startswith( 'X9' ):
      return cls( '1.3' )

    if baseboard.startswith( 'X10' ):
      return cls( '1.5' )

    return None

  def __init__( self, version, *args, **kwargs ):
    super( SUM, self ).__init__( *args, **kwargs )
    self._configCache = {}
    self._bootCache = {}
    self._append_mode = False
    self._force_legacy_boot = False
    self._boot_item_regex = '^%s[a-z]'
    if version == '1.3':
      self._append_mode = True
      self.cli = '/sbin/sum1_3'

    elif version == '1.5':
      self._append_mode = True
      self._force_legacy_boot = True
      self.cli = '/sbin/sum1_5'
      self._boot_item_regex = '^Legacy .* #%s'

    else:
      raise Exception( 'Unknown syscfg version "%s"' % version )

    # for some reason version 1.5/x10 mobo only sets the last call, where 1.3 will set all of them,
    #so append mode will keep adding hopping that any errors that happen will allow at least the ones that came before to work

    if self._append_mode:
      open( '/tmp/sum_set', 'w' ).close()

  def _execute( self, cmd ):
    proc = Popen( [ self.cli ] + cmd, stdout=PIPE )
    ( stdout, _ ) = proc.communicate()

    open( '/tmp/sum.log', 'a' ).write( 'CMD: %s\nOutput: %s\nrc:%s' % ( cmd, stdout, proc.returncode ) )

    if proc.returncode != 0:
      raise Exception( 'Failed call to SUM' )

    return stdout

  def _loadConfigCache( self ):
    self._configCache = {}
    self._execute( [ '-c', 'GetCurrentBiosCfgTextFile', '--file', '/tmp/sum_get', '--overwrite' ] )

    group = None
    tmp = open( '/tmp/sum_get', 'r' )
    for line in tmp.readlines():
      result = re.match( '\[([^\]]*)\]', line )
      if result:
        group = result.group( 1 )
        self._configCache[ group ] = {}
        continue

      result = re.match( '([^=]+)=(\w+)\s*\/\/(.*)', line )
      if result:
        options = {}
        for part in result.group( 3 ).split( ',' ):
          result2 = re.search( '\*?([0-9A-Fa-f]+) \((.+)\)', part ) # yes search not match
          if result2:
            options[ result2.group( 1 ) ] = result2.group( 2 )

        if not options:
          options = None
        self._configCache[ group ][ result.group( 1 ).strip() ] = ( result.group( 2 ).strip(), options )
        continue

    tmp.close()
    self._bootCache = self._configCache['Boot']
    del self._configCache['Boot']

  def changePassword( self, newpassword ):
    return True

  def checkPassword( self ):
    return True

  def setValue( self, group, item, value ):
    tmp = open( '/tmp/sum_set', ( 'a' if self._append_mode else 'w' ) )
    tmp.write( '[%s]\n' % group )
    tmp.write( '%s=%s\n' % ( item, value ) )
    tmp.close()

    try:
      self._execute( [ '-c', 'ChangeBiosCfg', '--file', '/tmp/sum_set' ] )
      return True

    except Exception as e:
      print 'Exception while setting value "%s"' % e
      return False

  def getValue( self, group, item ):
    if not self._configCache:
      self._loadConfigCache()

    try:
      return self._configCache[ group ][ item ]
    except KeyError:
      return ( None, None )

  def getBoot( self ):
    if not self._bootCache:
      self._loadConfigCache()

    result = []

    entry_map = {}
    for name in self.class_name:
      entry_map[ self.class_name[ name ] ] = []
      for item in self._bootCache:
        ( value, values ) = self._bootCache[ item ]
        if item == name:
          entry_map[ self.class_name[ name ] ].append( values[ value ] )

    for counter in range( 1, 10 ):
      for item in self._bootCache:
        ( value, values ) = self._bootCache[ item ]
        if re.search( self._boot_item_regex % counter, item ):
          try:
            for entry in entry_map[ self.class_map[ values[ value ] ] ]:
              result.append( [ self.class_map[ values[ value ] ], entry ] )

          except KeyError:
            pass # ignoring the boot types we don't have common classes for

    return result

  def setBoot( self, boot ):
    if not self._bootCache:
      self._loadConfigCache()

    if self._force_legacy_boot:
      if self._bootCache[ 'Boot Mode Select' ][0] != '00': # 'Boot Mode Select' has to be there, other wise there is trouble
         self.setValue( 'Boot', 'Boot Mode Select', '00' )
         return True

    tmp = open( '/tmp/sum_set', ( 'a' if self._append_mode else 'w' ) )
    tmp.write( '[Boot]\n' )

    for counter in range( 1, 10 ):
      for item in self._bootCache:
        ( value, values ) = self._bootCache[ item ]
        if re.search( self._boot_item_regex % counter, item ):
          try:
            value = boot[ counter - 1 ][0]
            value = self.class_map.keys()[ self.class_map.values().index( value ) ]

          except IndexError:
            value = 'Disabled'

          tmp.write( '%s=%s\n' % ( item, values.keys()[ values.values().index( value ) ] ) )

    for item in boot:
      if len( item ) > 1:
        name = self.class_name.keys()[ self.class_name.values().index( item[0] ) ]
        for item2 in self._bootCache:
          ( value, values ) = self._bootCache[ item2 ]
          if item2 == name:
            tmp.write( '%s=%s\n' % ( name, values.keys()[ values.values().index( item[1] ) ] ) )

    tmp.close()

    try:
      self._execute( [ '-c', 'ChangeBiosCfg', '--file', '/tmp/sum_set' ] )
      return True

    except Exception as e:
      print 'Exception While setting boot order "%s"' % e
      return False
