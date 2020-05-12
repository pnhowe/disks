#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <errno.h>

#include "disk.h"

int verbose;

static char short_opts[] = "vqd:p:h?";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "driver", required_argument, NULL, 'd' },
  { "protocol", required_argument, NULL, 'p' },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v, --verbose    Verbose, more -v more stuff.(4 levles, starts on level 0)\n\
  -q, --quiet      Less Verbose, more -q less stuff.\n\
  -d, --driver     Force a Driver to use ( SGIO, SAT, MegaDev, MegaSAS, IDE(forces -pATA) )\n\
  -p, --protocol   Force a Protocol to use ( ATA, SCSI )\n\
  -h,-?, --help    Show this\n\
\n";


int main( int argc, char **argv )
{
  char *device;
  struct device_handle drive;
  int rc;
  int i;
  enum driver_type driver;
  enum protocol_type protocol;
  struct smart_attribs attribs;
  struct drive_info *info;

  int c;

  verbose = 0;

  driver = DRIVER_TYPE_UNKNOWN;
  protocol = PROTOCOL_TYPE_UNKNOWN;

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

      case 'd':
        if( strcmp( optarg, "IDE" ) == 0 )
          driver = DRIVER_TYPE_IDE;
        else if( strcmp( optarg, "SGIO" ) == 0 )
          driver = DRIVER_TYPE_SGIO;
        else if( strcmp( optarg, "SAT" ) == 0 )
          driver = DRIVER_TYPE_SAT;
        else if( strcmp( optarg, "MegaDev" ) == 0 )
          driver = DRIVER_TYPE_MEGADEV;
        else if( strcmp( optarg, "MegaSAS" ) == 0 )
          driver = DRIVER_TYPE_MEGASAS;
        else
        {
          fprintf( stderr, "Unknwon Driver '%s'\n", optarg );
          fprintf( stderr, usage_txt, argv[0] );
          exit( 1 );
        }

        break;

      case 'p':
        if( strcmp( optarg, "ATA" ) == 0 )
          protocol = PROTOCOL_TYPE_ATA;
        else if( strcmp( optarg, "SCSI" ) == 0 )
          protocol = PROTOCOL_TYPE_SCSI;
        else
        {
          fprintf( stderr, "Unknwon Protocol '%s'\n", optarg );
          fprintf( stderr, usage_txt, argv[0] );
          exit( 1 );
        }

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

  if( openDisk( device, &drive, driver, protocol, IDENT ) )
  {
    fprintf( stderr, "Error opening device '%s', errno: %i.\n", device, errno );
    exit( 1 );
  }

  info = getDriveInfo( &drive );

  printf( "Model:                %s\n", info->model );
  printf( "Serial:               %s\n", info->serial );
  if( info->protocol == PROTOCOL_TYPE_ATA )
    printf( "Firmware Rev:         %s\n", info->firmware_rev );
  if( info->protocol == PROTOCOL_TYPE_SCSI )
    printf( "Version:              %s\n", info->version );

  if( !info->SMARTSupport || !info->SMARTEnabled )
  {
    fprintf( stderr, "Error Device Dosen't support SMART or SMART is not enabled.\n" );
    exit( 1 );
  }

  rc = smartStatus( &drive );

  if( rc == -1 )
  {
    fprintf( stderr, "Error getting status, errno %i.\n", errno );
    goto err;
  }
  else
  {
    if( rc & STATUS_DRIVE_FAULT )
      printf( "SMART Drive Fault\n" );

    if( rc & STATUS_THRESHOLD_EXCEEDED )
      printf( "SMART Threshold Exceeded\n" );
  }

  if( smartAttributes( &drive, &attribs ) )
  {
    fprintf( stderr, "Error getting atrribs, errno %i.\n", errno );
    goto err;
  }

  printf( "SMART Attribs\n" );
  if( attribs.protocol == PROTOCOL_TYPE_ATA )
  {
    printf( "ID\tValue\tMax\tThresh\tRaw Value\n" );
    for( i = 0; i < attribs.count; i++ )
    {
      if( attribs.data.ata[i].threshold )
        printf( "%3i\t%3i\t%3i\t%3i\t%lli\n", attribs.data.ata[i].id, attribs.data.ata[i].current, attribs.data.ata[i].max, attribs.data.ata[i].threshold, attribs.data.ata[i].raw );
      else
        printf( "%3i\t%3i\t%3i\t - \t%lli\n", attribs.data.ata[i].id, attribs.data.ata[i].current, attribs.data.ata[i].max, attribs.data.ata[i].raw );
    }
  }
  else if( attribs.protocol == PROTOCOL_TYPE_SCSI )
  {
    printf( "Page\tParameter\tValue\n" );
    for( i = 0; i < attribs.count; i++ )
      printf( "%3i\t%3i\t%lli\n", attribs.data.scsi[i].page_code, attribs.data.scsi[i].parm_code, attribs.data.scsi[i].value );
  }

  rc = smartLogCount( &drive );
  if( rc == -1 )
  {
    fprintf( stderr, "Error getting log count, errno %i.\n", errno );
    goto err;
  }
  else
    printf( "Errors in logged:%i\n", rc );

  closeDisk( &drive );

  exit( 0 );
err:
  closeDisk( &drive );

  exit( 1 );

}
