import re

from ..handler import Handler

STORCLI64_CMD = '/sbin/storcli64'

PRIORITY = 60
NAME = 'Mega RAID'


class MegaRAIDTarget( object ):
  def __init__( self, id, model ):
    self.id = id
    self.model = model
    self.package_version = None
    self.pci_address = None

  def __str__( self ):
    return f'MegaRAIDTarget id: "{self.id}"'

  def __repr__( self ):
    return f'MegaRAIDTarget id: "{self.id}" model: "{self.model}" addr: "{self.pci_address}" version: "{self.package_version}"'


class MegaRAID( Handler ):
  def __init__( self ):
    super().__init__()

  def getTargets( self ):
    ( rc, line_list ) = self._execute( 'megaraid_get_targets', STORCLI64_CMD + ' show' )
    if rc != 0:
      raise Exception( 'Error getting megaraid controller list' )

    c_list = []

    for line in line_list:
      tmp = re.match( r'\s*([0-9]+)\s+([0-9a-zA-Z\-]+)(\s+\S+){12}', line )
      if tmp:
        c_list.append( MegaRAIDTarget( tmp.group( 1 ), tmp.group( 2 ) ) )

    result = []

    for c in c_list:
      ( rc, line_list ) = self._execute( 'megaraid_detail_{c.id}', STORCLI64_CMD + ' /c{c.id} show' )
      if rc != 0:
        raise Exception( f'Error getting detail on controller "{c.id}"' )

      for line in line_list:
        tmp = re.match( r'\s*FW Package Build\s*=\s*([0-9\.\-]*)', line )
        if tmp:
          c.package_version = tmp.group( 1 )

        for line in line_list:
          tmp = re.match( r'\s*PCI Address\s*=\s*([0-9][0-9]:[0-9][0-9]:[0-9][0-9]:[0-9][0-9])', line )
          if tmp:
            c.pci_address = f'00{tmp.group( 1 )[ 0:8 ]}.0'

      result.append( ( c, c.model, c.package_version ) )

    return result

  def updateTarget( self, target, filename ):
    self._prep_wrk_dir()

    ( rc, line_list ) = self._execute( f'megaraid_download_{target.pci_address}', STORCLI64_CMD + ' /c{target.id} download file={filename}' )
    if rc != 0:
      print( f'Error Downloading "{filename}" to "target.id"' )
      return False

    return True
