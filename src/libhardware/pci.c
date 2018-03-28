// snippits and general procedure barrowed from pci-utils see proc.c in the pci-utils source
#include <stdio.h>
#include <errno.h>

#include "pci.h"

#define PCI_FUNC(devfn)		((devfn) & 0x07)
#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)

extern int verbose;

int getPCIList( struct pci_entry list[], const int list_size )
{
  char *filename = "/proc/bus/pci/devices";
  char buff[1024];
  int counter = 0;
  FILE *fp;
  unsigned int dfn, vend;


  fp = fopen( filename, "r" );
  if( !fp )
  {
    if( verbose )
      fprintf( stderr, "Error opening \"%s\", errno: %i\n", filename, errno );

    return -1;
  }

  while( fgets( buff, sizeof( buff ), fp ) )
  {
    if( sscanf( buff, "%x %x", &dfn, &vend ) != 2 )
      continue;

    if( counter < list_size )
    {
      list[counter].vendor_id = vend >> 16U;
      list[counter].device_id = vend & 0xffff;
      list[counter].bus = dfn >> 8U;
      list[counter].device = PCI_SLOT( dfn & 0xff );
      list[counter].function = PCI_FUNC( dfn & 0xff );
      counter++;
    }
  }

  return counter;
}
