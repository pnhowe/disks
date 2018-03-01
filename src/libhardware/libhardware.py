""" Hardware utilities """

from ctypes import cdll, POINTER, pointer, Structure, c_int, c_char_p, create_string_buffer

from libhardware_h import dmi_entry, pci_entry

__VERSION__ = "0.4.0"

libhardware = cdll.LoadLibrary( 'libhardware.so.' + __VERSION__.split( '.' )[0] )

ENTRY_LIST_SIZE = 1024

class dmiEntryList( Structure ):
  _fields_ = [ ( 'list', dmi_entry * ENTRY_LIST_SIZE ) ]

class pciEntryList( Structure ):
  _fields_ = [ ( 'list', pci_entry * ENTRY_LIST_SIZE ) ]

_set_verbose_ident = libhardware.set_verbose
_set_verbose_ident.argtypes = [ c_int ]
_set_verbose_ident.restype = None

_get_dmi_info_ident = libhardware.get_dmi_info
_get_dmi_info_ident.argtypes = [ POINTER( dmiEntryList ), POINTER( c_int ), c_char_p ]
_get_dmi_info_ident.restype = c_int

_get_pci_info_ident = libhardware.get_pci_info
_get_pci_info_ident.argtypes = [ POINTER( pciEntryList ), POINTER( c_int ), c_char_p ]
_get_pci_info_ident.restype = c_int

def setVerbose( verbose ):
  _set_verbose_ident( verbose )

def dmiInfo():
  counter = c_int( 0 )
  entry_list = dmiEntryList()
  errstr = create_string_buffer( 100 )
  counter.value = ENTRY_LIST_SIZE

  if _get_dmi_info_ident( pointer( entry_list ), pointer( counter ), errstr ):
    raise Exception( 'Error getting DMI Info "%s"' % errstr.value.strip() )

  tmp = {}

  for entry in entry_list.list:
    if not entry.type:
      continue
    if entry.group not in tmp:
      tmp[entry.group] = { 'type': entry.type, 'names': [], 'values': [] }
    tmp[entry.group]['names'].append( entry.name.strip() )
    tmp[entry.group]['values'].append( entry.value.strip() )

  results = {}
  for group in tmp:
    if tmp[group]['type'] not in results:
      results[ tmp[group]['type'] ] = []
    results[ tmp[group]['type'] ].append( dict( zip( tmp[group]['names'], tmp[group]['values'] ) ) )

  return results

def pciInfo():
  counter = c_int( 0 )
  entry_list = pciEntryList()
  errstr = create_string_buffer( 100 )
  counter.value = ENTRY_LIST_SIZE

  if _get_pci_info_ident( pointer( entry_list ), pointer( counter ), errstr ):
    raise Exception( 'Error getting PCI Info "%s"' % errstr.value.strip() )

  results = {}

  for entry in entry_list.list:
    if not entry.vendor_id:
      continue
    results[ '%04x:%02x:%02x.%02x' % ( entry.domain, entry.bus, entry.device, entry.function ) ] = { 'vendor': entry.vendor_id, 'device': entry.device_id }

  return results
