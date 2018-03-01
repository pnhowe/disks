import re
import glob
import os
from handler import Handler

MPT3SAS_CMD = '/sbin/sas3flash'

class Mpt3SASTarget( object ):
  def __init__( self, id, chip, version, pci_address ):
    self.id = id
    self.chip = chip
    self.pci_address = pci_address
    self.version = version
    self.sas_address = None
    self.type = None

  def __str__( self ):
    return 'Mpt3SASTarget id: %s' % self.id

  def __repr__( self ):
    return 'Mpt3SASTarget id: %s chip: %s addr: %s version: %s saddr: %s type: %s' % ( self.id, self.chip, self.pci_address, self.version, self.sas_address, self.type )


class Mpt3SAS( Handler ):
  def __init__( self, *args, **kwargs ):
    super( Mpt3SAS, self ).__init__( *args, **kwargs )

  def getTargets( self ):
    ( rc, line_list ) = self._execute( 'mpt3sas_get_targets', MPT3SAS_CMD + ' -listall' )
    if rc != 0:
      print 'Error getting mpt3sas controller list'
      return None

    c_list = []

    for line in line_list:
      tmp = re.match( '\s*([0-9]+)\s*(SAS[0-9]*)[^ ]*\s*([0-9\.]*)\s*[0-9\.]*\s*[0-9\.]*\s*([0-9:]*)', line )
      if tmp:
        c_list.append( Mpt3SASTarget( tmp.group( 1 ), tmp.group( 2 ), tmp.group( 3 ), '00%s.0' % tmp.group( 4 )[ 0:8 ] ) )

    result = []

    for c in c_list:
      ( rc, line_list ) = self._execute( 'mpt3sas_detail_%s' % c.pci_address, MPT3SAS_CMD + ' -c %s -list' % c.id )
      if rc != 0:
        print 'Error getting detail on controller "%s"' % c.id
        return None

      for line in line_list:
        tmp = re.match( '\s*Firmware Product ID\s*:[0-9x\s]*\(([A-Z]*)\)', line )
        if tmp:
          c.type = tmp.group( 1 )

        tmp = re.match( '\s*SAS Address\s*:\s*([0-9a-z\-]*)', line )
        if tmp:
          c.sas_address = tmp.group( 1 ).replace( '-', '' )

      result.append( ( c, c.chip, '%s-%s' % ( c.version, c.type ) ) )

    return result

  def updateTarget( self, target, filename ):
    self._prep_wrk_dir()

    pci_info = self._get_pci_info( target.pci_address )

    ( rc, line_list ) = self._execute( 'mpt3sas_extract_%s' % target.pci_address, '/bin/tar -xvzf "%s" -C %s' % ( filename, self.WORK_DIR ) )
    if rc != 0:
      print 'Error extracting "%s"' % filename
      return False

    firmware_file = None
    filename_list = []
    chip = re.sub( '[^0-9]', '', target.chip )
    for filename in glob.glob( '%s/*' % self.WORK_DIR ):
      filename_list.append( filename )
      if os.path.basename( filename ).startswith( chip ):
        firmware_file = os.path.basename( filename )

    if firmware_file is None:
      print 'Unable to determing firmware file'
      return False

    if 'mptsas3.rom' not in [ os.path.basename( i ) for i in filename_list ]:
      print 'mptsas3.rom file not found'
      return False

    if 'mpt3x64.rom' not in [ os.path.basename( i ) for i in filename_list ]:
      print 'mpt3x64.rom file not found'
      return False

    cmd = """
echo "Before..."
sas3flsh -c %s -list

echo "Clearing flash..."
sas3flsh -c %s -o -e 7

echo "Loading new firmware..."
sas3flsh -c %s -f %s
sas3flsh -c %s -b mptsas3.rom
sas3flsh -c %s -b mpt3x64.rom

echo "Setting SAS Address...."
sas3flsh -c %s -o -sasadd %s

echo "Results..."
sas3flsh -c %s -list
""" % ( target.id, target.id, target.id, firmware_file, target.id, target.id, target.id, target.sas_address, target.id )

    filename_list.append( '/resources/sas3flsh.exe' )

    pci = {
            'vendor': pci_info[ 'vendor_id' ],
            'product': pci_info[ 'product_id' ],
            'driver': pci_info[ 'driver' ],
            'address': target.pci_address
          }
    line_list = self._execute_quemu( 'mpt3sas_update_%s' % target.pci_address, cmd, filename_list, pci )
    if line_list is None:
      return False

    return True
