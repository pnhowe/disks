""" Enclosure utilities """

from ctypes import pointer, c_ubyte, create_string_buffer
import os
import re
import glob
from libdrive.libenclosure_h import struct_enclosure_info, struct_subenc_descriptors, get_enclosure_info, get_subenc_descriptors, set_subenc_descriptors, set_verbose

from libdrive.libdrive import getSASEnclosureList


value_array = ( c_ubyte * 4 )


def setVerbose( verbose ):
  set_verbose( verbose )


class Enclosure( object ):
  def __init__( self, name ):
    self._info = {}
    self._descriptors = None
    self.name = name
    self.pcipath = None
    self.scsi_generic = None

    tmp_list = glob.glob( '/sys/class/enclosure/{0}/device'.format( name ) )  # figure out how to do this for disks that don't have a block device
    if tmp_list:
      self.pcipath = re.sub( '^/sys/devices', '', os.path.realpath( tmp_list[0] ) )

    tmp_list = glob.glob( '/sys/class/enclosure/{0}/device/scsi_generic/sg*'.format( name ) )
    if tmp_list:
      self.scsi_generic = '/dev/{0}'.format( tmp_list[0].split( '/' )[ -1 ] )

    self.devpath = re.sub( '^/sys', '', os.path.realpath( '/sys/class/enclosure/{0}'.format( name ) ) )

  def _loadInfo( self ):
    errstr = create_string_buffer( 100 )
    tmp = struct_enclosure_info()

    if get_enclosure_info( self.scsi_generic, pointer( tmp ), errstr ):
      raise Exception( 'Error getting enclosure info "{0}"'.format( errstr.value.strip() ) )

    self._info = { 'vendor': tmp.vendor_id.strip(), 'version': tmp.version.strip() }

    for item in ( 'serial', 'model' ):
      self._info[ item ] = getattr( tmp, item ).decode().strip()
    for item in ( 'WWN', ):
      self._info[ item ] = getattr( tmp, item )

  def _loadDescriptors( self ):
    errstr = create_string_buffer( 100 )
    tmp = struct_subenc_descriptors()

    if get_subenc_descriptors( self.scsi_generic, pointer( tmp ), errstr ):
      raise Exception( 'Error getting enclosure info "{0}"'.format( errstr.value.strip() ) )

    self._descriptors = tmp

  @property
  def reporting_info( self ):
    return { 'serial': self.serial, 'model': self.model, 'name': self.name, 'location': self.location, 'vendor': self.vendor, 'version': self.version }

  @property
  def serial( self ):
    if not self._info:
      self._loadInfo()
    return self._info['serial']

  @property
  def model( self ):
    if not self._info:
      self._loadInfo()
    return self._info['model']

  @property
  def vendor( self ):
    if not self._info:
      self._loadInfo()
    return self._info[ 'vendor' ]

  @property
  def version( self ):
    if not self._info:
      self._loadInfo()
    return self._info[ 'version' ]

  @property
  def WWN( self ):
    if not self._info:
      self._loadInfo()

    return self._info[ 'WWN' ]

  def getSubEncDescriptors( self ):
    if not self._descriptors:
      self._loadDescriptors()

    descriptor_list = []
    for i in range( 0, self._descriptors.count ):
      descriptor = self._descriptors.descriptors[ i ]
      descriptor_list.append( {
                                'sub_enclosure': int( descriptor.sub_enclosure ),
                                'element_index': int( descriptor.element_index ),
                                'element_type': int( descriptor.element_type ),
                                'subelement_index': int( descriptor.subelement_index ),
                                'help_text': descriptor.help_text.strip(),
                                'value_offset': int( descriptor.value_offset ),
                                'value': bytearray( descriptor.value )
                               } )

    return { 'generation': self._descriptors.generation, 'count': self._descriptors.count, 'page_size': self._descriptors.page_size, 'descriptors': descriptor_list }

  def setSubEncDescriptors( self, generation, page_size, descriptor_list ):  # descriptor_list = list of ( value_offset, value )
    index = 0
    tmp = struct_subenc_descriptors()
    tmp.generation = generation
    tmp.page_size = page_size
    for ( value_offset, value ) in descriptor_list:
      if not isinstance( value, bytearray ) or len( value ) != 4:
        raise Exception( 'Value is not a bytearray of 4 bytes' )
      tmp.descriptors[ index ].value_offset = value_offset
      tmp.descriptors[ index ].value = value_array.from_buffer_copy( value )

      index += 1
      if index >= 250:  # enclosure.h:#define DESCRIPTOR_MAX_COUNT 250
        raise Exception( 'To many descriptors in descriptor_list' )

    errstr = create_string_buffer( 100 )

    tmp.count = index

    if set_subenc_descriptors( self.scsi_generic, pointer( tmp ), errstr ):
      raise Exception( 'Error setting enclosure info "{0}"'.format( errstr.value.strip() ) )

  def __hash__( self ):
    return self.name.__hash__()

  def __str__( self ):
    return 'Enclosure: {0} {1}'.format( self.name, self.scsi_generic )

  # def __cmp__( self, other ):
  #   if other is None:
  #     return 1
  #   return cmp( self.name, other.name )


# Enclosure Manager
class EnclosureManager( object ):
  def __init__( self ):
    super( EnclosureManager, self ).__init__()
    self._enclosure_list = []
    self.rescan()

  @property
  def enclosure_list( self ):
    tmp = list( self._enclosure_list )  # make a copy
    tmp.sort()
    return tmp

  @property
  def scsi_map( self ):
    tmp = {}
    for enclosure in self.enclosure_list:
      tmp[ enclosure.scsi_generic ] = enclosure
    return tmp

  @property
  def devpath_map( self ):
    tmp = {}
    for enclosure in self.enclosure_list:
      tmp[ enclosure.devpath ] = enclosure
    return tmp

  def rescan( self ):
    self._enclosure_list = []

    enclosure_list = getSASEnclosureList()

    for enclosure in enclosure_list:
      self._enclosure_list.append( Enclosure( enclosure ) )
