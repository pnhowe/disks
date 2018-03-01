import re
import os
import glob

from handler import Handler

STORCLI64_CMD = '/sbin/storcli64'

class MegaRAIDTarget( object ):
  def __init__( self, id, model ):
    self.id = id
    self.model = model
    self.package_version = None
    self.pci_address = None

  def __str__( self ):
    return 'MegaRAIDTarget id: %s' % self.id

  def __repr__( self ):
    return 'MegaRAIDTarget id: %s model: %s addr: %s version: %s' % ( self.id, self.model, self.pci_address, self.package_version )


class MegaRAID( Handler ):
  def __init__( self, *args, **kwargs ):
    super( MegaRAID, self ).__init__( *args, **kwargs )

  def getTargets( self ):
    ( rc, line_list ) = self._execute( 'megaraid_get_targets', STORCLI64_CMD + ' show' )
    if rc != 0:
      print 'Error getting megaraid controller list'
      return None

    c_list = []

    for line in line_list:
      tmp = re.match( '\s*([0-9]+)\s+([0-9a-zA-Z\-]+)(\s+\S+){12}', line )
      if tmp:
        c_list.append( MegaRAIDTarget( tmp.group( 1 ), tmp.group( 2 ) ) )

    result = []

    for c in c_list:
      ( rc, line_list ) = self._execute( 'megaraid_detail_%s' % c.id, STORCLI64_CMD + ' /c%s show' % c.id )
      if rc != 0:
        print 'Error getting detail on controller "%s"' % c.id
        return None

      for line in line_list:
        tmp = re.match( '\s*FW Package Build\s*=\s*([0-9\.\-]*)', line )
        if tmp:
          c.package_version = tmp.group( 1 )

        for line in line_list:
          tmp = re.match( '\s*PCI Address\s*=\s*([0-9][0-9]:[0-9][0-9]:[0-9][0-9]:[0-9][0-9])', line )
          if tmp:
            c.pci_address = '00%s.0' % tmp.group( 1 )[ 0:8 ]

      result.append( ( c, c.model, c.package_version ) )

    return result

  def updateTarget( self, target, filename ):
    self._prep_wrk_dir()

    ( rc, line_list ) = self._execute( 'megaraid_download_%s' % target.pci_address, STORCLI64_CMD + ' /c%s download file=%s' % ( target.id, filename ) )
    if rc != 0:
      print 'Error Downloading "%s" to "%s"' % ( filename, target.id )
      return False

    return True
