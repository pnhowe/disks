#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "dmi.h"
#include "pci.h"

#define DMI_LIST_SIZE 1024
#define PCI_LIST_SIZE 1024
#define VPD_LIST_SIZE 200

int verbose;

static char short_opts[] = "vhdp?";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v, --verbose    Verbose, more -v more stuff ( 4 levels )\n\
  -d, --dmi        show dmi info\n\
  -p, --pci        show pci info\n\
  -h,-?, --help    Show this\n\
\n";

int main( int argc, char **argv )
{
  int i, j;
  int rc, rc2;
  struct dmi_entry dmi_list[DMI_LIST_SIZE];
  struct pci_entry pci_list[PCI_LIST_SIZE];
  struct vpd_entry vpd_list[VPD_LIST_SIZE];
  int c;
  int doDMI = 0;
  int doPCI = 0;

  verbose = 0;

  while( ( c = getopt_long( argc, argv, short_opts, long_opts, NULL  ) ) != -1 )
  {
    switch( c )
    {
      case 'v':
        verbose++;
        break;

      case 'd':
        doDMI = 1;
        break;

      case 'p':
        doPCI = 1;
        break;

      case '?':
      case 'h':
      default:
        fprintf( stderr, usage_txt, argv[0] );
        exit( 1 );
    }
  }

  if( getuid() != 0 )
  {
    fprintf( stderr, "Must be root to run this.\n" );
    exit( 1 );
  }

  if( doDMI )
  {
    printf( "DMI Information...\n" );
    memset( dmi_list, 0, sizeof( dmi_list ) );

    rc = getDMIList( dmi_list, DMI_LIST_SIZE );
    if( rc == -1 )
    {
      fprintf( stderr, "Error Getting DMIInfo, errno %i.\n", errno );
      exit( 1 );
    }

    if( rc >= DMI_LIST_SIZE )
    {
      fprintf( stderr, "Error Getting DMIInfo, got %i entries.\n", rc );
      exit( 1 );
    }

    printf( "Have %i DMI Entries\n", rc );

    for( i = 0; i < rc; i++ )
      printf( "%4i: %-20s\t%-20s\%s\n", dmi_list[i].group, dmi_list[i].type, dmi_list[i].name, dmi_list[i].value );
  }

  if( doPCI )
  {
    printf( "PCI Information...\n" );
    memset( pci_list, 0, sizeof( pci_list ) );

    rc = getPCIList( pci_list, PCI_LIST_SIZE );
    if( rc == -1 )
    {
      fprintf( stderr, "Error Getting PCIInfo, errno %i.\n", errno );
      exit( 1 );
    }

    if( rc >= PCI_LIST_SIZE )
    {
      fprintf( stderr, "Error Getting PCIInfo, got %i entries.\n", rc );
      exit( 1 );
    }

    printf( "Have %i PCI Entries\n", rc );

    for( i = 0; i < rc; i++ )
    {
      printf( "%04x:%02x:%02x.%02x  %04x %04x\n", pci_list[i].domain, pci_list[i].bus, pci_list[i].device, pci_list[i].function, pci_list[i].vendor_id, pci_list[i].device_id );

      memset( vpd_list, 0, sizeof( vpd_list ) );
      rc2 = getVPDInfo( &pci_list[i], vpd_list, VPD_LIST_SIZE );
      if( rc2 == -1 )
      {
        fprintf( stderr, "Error Getting VPDInfo, errno %i.\n", errno );
        exit( 1 );
      }
  
      if( rc2 >= VPD_LIST_SIZE )
      {
        fprintf( stderr, "Error Getting VPDInfo, got %i entries.\n", rc2 );
        exit( 1 );
      }

      for( j = 0; j < rc2; j++ )
        printf( "  %s: %s\n", vpd_list[j].id, vpd_list[j].value );
    }
  }

  exit( 0 );
}
