#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <errno.h>

#include "device.h"
#include "enclosure.h"
#include "scsi.h"

int verbose;

static char short_opts[] = "vqd:p:h?";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v, --verbose    Verbose, more -v more stuff.(4 levles, starts on level 0)\n\
  -q, --quiet      Less Verbose, more -q less stuff.\n\
  -h,-?, --help    Show this\n\
\n";

int main( int argc, char **argv )
{
  char *device;
  struct device_handle enclosure;
  struct enclosure_info *info;
  int c;

  verbose = 0;

  while( ( c = getopt_long( argc, argv, short_opts, long_opts, NULL  ) ) != -1 )
  {
    switch( c )
    {
      case 'v':
        verbose++;
        break;

      case 'q':
        if( verbose > 0 )
          verbose--;
        break;

      case '?':
      case 'h':
      default:
        fprintf( stderr, usage_txt, argv[0] );
        exit( 1 );
    }
  }

  if( optind >= argc )
  {
    fprintf( stderr, usage_txt, argv[0] );
    exit( 1 );
  }

  device = argv[ optind ];

  if( getuid() != 0 )
  {
    fprintf( stderr, "Must be root to run this.\n" );
    exit( 1 );
  }

  if( openEnclosure( device, &enclosure ) )
  {
    fprintf( stderr, "Error opening device '%s', errno: %i.\n", device, errno );
    exit( 1 );
  }

  info = getEnclosureInfo( &enclosure );

  printf( "Vendor:               %s\n", info->vendor_id );
  printf( "Model:                %s\n", info->model );
  printf( "Serial:               %s\n", info->serial );
  printf( "Version:              %s\n", info->version );
  printf( "SCSI Version:         %s (0x%04x)\n", scsi_descriptor_desc( info->SCSI_version ), info->SCSI_version );

  if( info->WWN )
    printf( "WWN:                  %016llx\n", info->WWN );

  printf( "Features:\n" );
  printf( "  Diagnostic Pages:\n" );
  if( info->hasDiagnosticPageConfiguration )
      printf( "    Configuration\n" );
  if( info->hasDiagnosticPageControlStatus )
      printf( "    Enclosure Control/Status\n" );
  if( info->hasDiagnosticPageHelpText )
      printf( "    Help Text\n" );
  if( info->hasDiagnosticPageString )
      printf( "    String In/Out\n" );
  if( info->hasDiagnosticPageThreshold )
      printf( "    Threshold In/Out\n" );
  if( info->hasDiagnosticPageElementDescriptor )
      printf( "    Element Descriptior\n" );
  if( info->hasDiagnosticPageShortStatus )
      printf( "    Short Enclosure Status\n" );
  if( info->hasDiagnosticPageBusy )
      printf( "    Busy\n" );
  if( info->hasDiagnosticPageAdditionalElement )
      printf( "    Additional Element\n" );
  if( info->hasDiagnosticPageSubHelpText )
      printf( "    SubEnclosure Help Text\n" );
  if( info->hasDiagnosticPageSubString )
      printf( "    SubEnclosure Help Text In/Out\n" );
  if( info->hasDiagnosticPageSupportedDiagnostigPages )
      printf( "    Supported Diagnostic Pages\n" );
  if( info->hasDiagnosticPageDownloadMicroCode )
      printf( "    Download Microcontrol Control/Status\n" );
  if( info->hasDiagnosticPageSubNickname )
      printf( "    SubEnclosure Nickname\n" );

  closeEnclosure( &enclosure );

  exit( 0 );
}
