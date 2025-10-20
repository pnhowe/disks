#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dmi.h"
#include "pci.h"

int verbose = 0;

void set_verbose( const int value )
{
  verbose = value;
}

int get_dmi_info( struct dmi_entry *dmi_list, int *count, char *errStr )
{
  int rc;

  rc = getDMIList( dmi_list, *count );
  if( rc == -1 )
  {
    sprintf( errStr, "Error Getting DMIInfo, errno %i.\n", errno );
    return -1;
  }

  *count = rc;

  return 0;
}

int get_pci_info( struct pci_entry *pci_list, int *count, char *errStr )
{
  int rc;

  rc = getPCIList( pci_list, *count );
  if( rc == -1 )
  {
    sprintf( errStr, "Error Getting PCIInfo, errno %i.\n", errno );
    return -1;
  }

  *count = rc;

  return 0;
}

int get_vpd_info( struct pci_entry *entry, struct vpd_entry *vpd_list, int *count, char *errStr )
{
  int rc;

  rc = getVPDInfo( entry, vpd_list, *count );
  if( rc == -1 )
  {
    sprintf( errStr, "Error Getting VPDInfo, errno %i.\n", errno );
    return -1;
  }

  *count = rc;

  return 0;
}