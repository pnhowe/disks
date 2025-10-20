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

struct vpd_entry
{
  char id[3]; // id is 2 bytes pull null for convenience
  char value[256]; // must be bigger that one byte's size (255) to fit the VPD-R and VPD-RW entries without bounds checking, and leave room for the \0
};

// returns -1 for error otherwise the number of enteries avaiable, note this may be bigger than list_size
// list_size -> number of entries allocated to list
int getPCIList( struct pci_entry list[], const int list_size );

// returns -1 for error otherwise the number of vpd entries for that pci device, note this may be bigger than list_size
// list_size -> number of entries allocated to list
int getVPDInfo( struct pci_entry *entry, struct vpd_entry list[], const int list_size );


#endif
