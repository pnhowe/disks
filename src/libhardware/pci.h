#ifndef _PCI_H
#define _PCI_H

struct pci_entry
{
  int vendor_id;
  int device_id;
  int domain;
  unsigned char bus;
  unsigned char device;
  unsigned char function;
};

// returns -1 for error other wise the number of enteries avaiable, note this may be bigger than count
// list_size -> number of entries allocated to list
int getPCIList( struct pci_entry list[], const int list_size );

#endif

