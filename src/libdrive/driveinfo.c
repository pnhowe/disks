#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "disk.h"
#include "ata.h"
#include "scsi.h"

int verbose;

static char short_opts[] = "vqd:p:foh?";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "driver", required_argument, NULL, 'd' },
  { "protocol", required_argument, NULL, 'p' },
  { "on", no_argument, NULL, 'o' },
  { "off", no_argument, NULL, 'f' },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v,  --verbose   Verbose, more -v more stuff.(4 levles, starts on level 0)\n\
  -q,  --quiet     Less Verbose, more -q less stuff.\n\
  -d, --driver     Force a Driver to use ( SGIO, SAT, 3Ware, MegaDev, MegaSAS, IDE(forces -pATA) )\n\
  -p, --protocol   Force a Protocol to use ( ATA, SCSI )\n\
  -o  --on         Turn SMART On (enable)\n\
  -f  --off        Turn SMART Off (disable)\n\
  -h,-?, --help    Show this\n\
\n";

int main( int argc, char **argv )
{
  char *device;
  struct device_handle drive;
  enum driver_type driver;
  enum protocol_type protocol;
  struct drive_info *info;
  float tmpSize;
  int disableSmart = 0;
  int enableSmart = 0;
  __u64 tmpLBA;

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
        else if( strcmp( optarg, "3Ware" ) == 0 )
          driver = DRIVER_TYPE_3WARE;
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

      case 'o':
        enableSmart = 1;
        break;

      case 'f':
        disableSmart = 1;
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

  if( disableSmart )
  {
    if( disableSMART( &drive ) )
    {
      fprintf( stderr, "Error Disabling SMART, errno %i.\n", errno );
      goto err;
    }
    if( verbose )
      printf( "SMART Disabled.\n" );

    goto ok;
  }

  if( enableSmart )
  {
    if( enableSMART( &drive ) )
    {
      fprintf( stderr, "Error Enabled SMART, errno %i.\n", errno );
      goto err;
    }
    if( verbose )
      printf( "SMART Enabled.\n" );

    goto ok;
  }

  info = getDriveInfo( &drive );

  // If the output is changed, make sure to check harddrive/run in firmware-update to make sure it is unaffected

  if( info->driver == DRIVER_TYPE_IDE )
    printf( "Type:                 IDE\n" );
  else if( info->driver == DRIVER_TYPE_SGIO )
    printf( "Type:                 SGIO\n" );
  else if( info->driver == DRIVER_TYPE_SAT )
    printf( "Type:                 SAT\n" );
  else if( info->driver == DRIVER_TYPE_3WARE )
    printf( "Type:                 3Ware\n" );
  else if( info->driver == DRIVER_TYPE_MEGADEV )
    printf( "Type:                 MegaDev\n" );
  else if( info->driver == DRIVER_TYPE_MEGASAS )
    printf( "Type:                 MegaSAS\n" );
  else
    printf( "Type:                 UNKNOWN\n" );

  if( info->protocol == PROTOCOL_TYPE_ATA )
    printf( "Protocol:             ATA\n" );
  else if( info->protocol == PROTOCOL_TYPE_SCSI )
    printf( "Protocol:             SCSI\n" );
  else
    printf( "Protocol:             UNKNOWN\n" );

  if( info->protocol == PROTOCOL_TYPE_SCSI )
    printf( "Vendor:               %s\n", info->vendor_id );
  printf( "Model:                %s\n", info->model );
  printf( "Serial:               %s\n", info->serial );
  if( info->protocol == PROTOCOL_TYPE_ATA )
    printf( "Firmware Rev:         %s\n", info->firmware_rev );
  if( info->protocol == PROTOCOL_TYPE_SCSI )
    printf( "Version:              %s\n", info->version );

  if( info->protocol == PROTOCOL_TYPE_ATA )
    printf( "ATA Version:          %i, %s (0x%04x)\n", info->ATA_major_version, ata_version_desc( info->ATA_minor_version ), info->ATA_minor_version );

  if( info->protocol == PROTOCOL_TYPE_SCSI )
    printf( "SCSI Version:         %s (0x%04x)\n", scsi_descriptor_desc( info->SCSI_version ), info->SCSI_version );

  if( info->WWN )
    printf( "WWN:                  %016llx\n", info->WWN );

  printf( "LBA Count:            %llu\n", info->LBACount );
  printf( "Logical Sector Size:  %i\n", info->LogicalSectorSize );
  printf( "Physical Sector Size: %i\n", info->PhysicalSectorSize );
  tmpSize = ( info->LogicalSectorSize * info->LBACount );
  printf( "Total Disk Size:      %.3f (GiB)  %.3f (TiB)\n", ( tmpSize / ( 1024.0 * 1024.0 * 1024.0 ) ), ( tmpSize / ( 1024.0 * 1024.0 * 1024.0 * 1024.0 ) ) );
  printf( "Total Disk Size:      %.3f (GB)   %.3f (TB)\n", ( tmpSize / ( 1000.0 * 1000.0 * 1000.0 ) ), ( tmpSize / ( 1000.0 * 1000.0 * 1000.0 * 1000.0 ) ) );

  if( !info->isSSD && info->RPM )
    printf( "Nominal Rotation Spd: %u\n", info->RPM );

  if( info->isSSD )
    printf( "SSD:                  Yes\n" );
  else
    printf( "SSD:                  No\n" );

  if( info->supportsTrim )
  {
    printf( "Supports Trim/Unmap:  Yes\n" );
    if( info->protocol == PROTOCOL_TYPE_SCSI )
    {
      printf( "Max Unmap LBA Count:  %u\n", info->maxUnmapLBACount );
      printf( "Max Unmap Descr:      %u\n", info->maxUnmapDescriptorCount );
    }
  }
  else
    printf( "Supports Trim/Unmap:  No\n" );

  if( info->maxWriteSameLength )
    printf( "Max Write Same Length:  %llu\n", info->maxWriteSameLength );

  printf( "Features:\n" );
  if( info->SMARTSupport )
    printf( "  Supports SMART\n" );

  if( info->SMARTEnabled )
    printf( "  SMART Enabled\n" );

  if( info->supportsLBA )
    printf( "  Uses LBA Addressing\n" );

  if( info->bit48LBA )
    printf( "  Uses 48bit Addressing\n" );

  if( info->supportsSelfTest )
    printf( "  Supports Self Test\n" );

  if( info->supportsDMA )
    printf( "  Supports DMA\n" );

  if( info->supportsSETMAX )
  {
    tmpLBA = getMaxLBA( &drive );
    printf( "  Supports SETMAX\n" );
    if( tmpLBA > 0 )
      printf( "    MAX LBA: %llu\n", tmpLBA );
  }

  if( info->supportsSCT )
    printf( "  Supports SCT\n" );

  if( info->supportsWriteSame )
    printf( "  Supports WriteSame\n" );

  if( info->protocol == PROTOCOL_TYPE_SCSI )
  {
    printf( "  Log Sence Pages:\n" );
    if( info->hasLogPageErrorWrite )
      printf( "    Error Write\n" );
    if( info->hasLogPageErrorRead )
      printf( "    Error Read\n" );
    if( info->hasLogPageErrorVerify )
      printf( "    Error Verify\n" );
    if( info->hasLogPageErrorNonMedium )
      printf( "    Error Non-Medium\n" );
    if( info->hasLogPageTemperature )
      printf( "    Temprature\n" );
    if( info->hasLogPageStartStop )
      printf( "    Start-Stop\n" );
    if( info->hasLogPageSelfTest )
      printf( "    Self Test\n" );
    if( info->hasLogPageBackgroundScan )
      printf( "    Background Scan\n" );
    if( info->hasLogPageFactoryLog )
      printf( "    Factory Log\n" );
    if( info->hasLogPageSSD )
      printf( "    SSD\n" );
    if( info->hasLogPageInfoExcept )
      printf( "    Info Exception(SMART)\n" );
    printf( "  Vital Product Data Pages:\n" );
    if( info->hasVPDPageSerial )
      printf( "    Unit Serial Number\n" );
    if( info->hasVPDPageIdentification )
      printf( "    Device Identification\n" );
    if( info->hasVPDPageBlockLimits )
      printf( "    Block Limits\n" );
    if( info->hasVPDPageBlockDeviceCharacteristics )
      printf( "    Block Device Characteristics\n" );
  }

ok:

  closeDisk( &drive );

  exit( 0 );

err:
  closeDisk( &drive );

  exit( 1 );
}
