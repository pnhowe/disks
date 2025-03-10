""" Hardware utilities """

from ctypes import cdll, POINTER, pointer, Structure, c_int, c_char_p, create_string_buffer

from libhardware.libhardware_h import dmi_entry, pci_entry, vpd_entry

__VERSION__ = "0.6.0"

libhardware = cdll.LoadLibrary( 'libhardware.so.' + __VERSION__.split( '.' )[0] )

ENTRY_LIST_SIZE = 1024


class dmiEntryList( Structure ):
  _fields_ = [ ( 'list', dmi_entry * ENTRY_LIST_SIZE ) ]


class pciEntryList( Structure ):
  _fields_ = [ ( 'list', pci_entry * ENTRY_LIST_SIZE ) ]


class vpdEntryList( Structure ):
  _fields_ = [ ( 'list', vpd_entry * ENTRY_LIST_SIZE ) ]


_set_verbose_ident = libhardware.set_verbose
_set_verbose_ident.argtypes = [ c_int ]
_set_verbose_ident.restype = None

_get_dmi_info_ident = libhardware.get_dmi_info
_get_dmi_info_ident.argtypes = [ POINTER( dmiEntryList ), POINTER( c_int ), c_char_p ]
_get_dmi_info_ident.restype = c_int

_get_pci_info_ident = libhardware.get_pci_info
_get_pci_info_ident.argtypes = [ POINTER( pciEntryList ), POINTER( c_int ), c_char_p ]
_get_pci_info_ident.restype = c_int

_get_vpd_info_ident = libhardware.get_vpd_info
_get_vpd_info_ident.argtypes = [ POINTER( vpd_entry ), POINTER( vpdEntryList ), POINTER( c_int ), c_char_p ]
_get_vpd_info_ident.restype = c_int


def setVerbose( verbose ):
  _set_verbose_ident( verbose )


def dmiInfo():
  counter = c_int( 0 )
  entry_list = dmiEntryList()
  errstr = create_string_buffer( 100 )
  counter.value = ENTRY_LIST_SIZE

  if _get_dmi_info_ident( pointer( entry_list ), pointer( counter ), errstr ):
    raise Exception( 'Error getting DMI Info "{0}"'.format( errstr.value.strip() ) )

  tmp = {}

  for entry in entry_list.list:
    if not entry.type:
      continue

    if entry.group not in tmp:
      tmp[ entry.group ] = { 'type': str( entry.type, 'utf-8' ).strip(), 'names': [], 'values': [] }
    tmp[ entry.group ][ 'names' ].append( str( entry.name, 'utf-8' ).strip() )
    tmp[ entry.group ][ 'values' ].append( str( entry.value, 'utf-8' ).strip() )

  results = {}
  for group in tmp:
    if tmp[ group ][ 'type' ] not in results:
      results[ tmp[ group ][ 'type' ] ] = []
    results[ tmp[ group ][ 'type' ] ].append( dict( zip( tmp[ group ][ 'names' ], tmp[ group ][ 'values' ] ) ) )

  return results


def pciInfo():
  pci_counter = c_int( 0 )
  vpd_counter = c_int( 0 )
  pci_entry_list = pciEntryList()
  vpd_entry_list = pciEntryList()
  errstr = create_string_buffer( 100 )
  pci_counter.value = ENTRY_LIST_SIZE

  if _get_pci_info_ident( pointer( pci_entry_list ), pointer( pci_counter ), errstr ):
    raise Exception( 'Error getting PCI Info "{0}"'.format( errstr.value.strip() ) )

  results = {}

  for pci_entry in pci_entry_list.list:
    if not pci_entry.vendor_id:
      continue

    vpd_list = []

    vpd_counter.value = ENTRY_LIST_SIZE

    if _get_vpd_info_ident( pointer( pci_entry ), pointer( vpd_entry_list ), pointer( vpd_counter ), errstr ):
      raise Exception( 'Error getting VPD Info "{0}"'.format( errstr.value.strip() ) )
    
    for vpd_entry in vpd_entry_list.list:
      if not vpd_entry.id:
        continue

      vpd_list.append( { 'id': vpd_entry.id.stip(), 'value': vpd_entry.value.strip() } )


    results[ '{0:04x}:{1:02x}:{2:02x}.{3:02x}'.format( pci_entry.domain, pci_entry.bus, pci_entry.device, pci_entry.function ) ] = { 'vendor': pci_entry.vendor_id, 'device': pci_entry.device_id, 'vpd': vpd_list }

  return results
