#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "disk.h"

int verbose;

static char short_opts[] = "vqd:p:lwtiash?";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "driver", required_argument, NULL, 'd' },
  { "protocol", required_argument, NULL, 'p' },
  { "sleep", no_argument, NULL, 'l' },
  { "wake", no_argument, NULL, 'w' },
  { "standby", no_argument, NULL, 't' },
  { "idle", no_argument, NULL, 'i' },
  { "active", no_argument, NULL, 'a' },
  { "state", no_argument, NULL, 's' },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v,  --verbose   Verbose, more -v more stuff.(4 levles, starts on level 1)\n\
  -q,  --quiet     Less Verbose, more -q less stuff.\n\
  -d, --driver     Force a Driver to use ( SGIO, SAT, MegaDev, MegaSAS, IDE(forces -pATA) )\n\
  -p, --protocol   Force a Protocol to use ( ATA, SCSI, NVME )\n\
  -l  --sleep      Tell Drive to Sleep (Not for SCSI)\n\
  -w  --wake       Tell Drive to Wake Up from Sleep (Not for SCSI)\n\
  -t  --standby    Tell Drive to Standby\n\
  -i  --idle       Tell Drive to Idle\n\
  -a  --active     Tell Drive to go Active from Idle/Standby\n\
  -s  --state      Get Current Power State\n\
  -h,-?, --help    Show this\n\
NOTE: If more than one sleep/standby/reset is specified, the last one is used.\n\
NOTE2: Generaly a drive can go from Standby to Sleep without doing Active.\n\
       Wake is probably requred to get out of sleep, techincally the drive goes.\n\
       Standby on its way to wake up.\n\
";

#define MODE_SLEEP   0x01
#define MODE_WAKE    0x02
#define MODE_STANDBY 0x03
#define MODE_IDLE    0x04
#define MODE_ACTIVE  0x10

#define MODE_STATUS  0xff

#define MAX_READ_ATTEMPTS 5

static int doSuspedToActive( struct device_handle *drive )
{
  int i;
  unsigned char *buff;
  buff = malloc( sizeof( char ) * drive->drive_info.LogicalSectorSize );

  for( i = 0; i < MAX_READ_ATTEMPTS; i++ )
  {
    if( verbose >= 2 )
      fprintf( stderr, "Read Attempt %i of %i\n", i, MAX_READ_ATTEMPTS );

    if( readLBA( drive, 0, 0, 1, 1, buff ) )
    {
      if( errno == ECONNRESET )  // Internal SATA ports tend to do this
      {
        sleep( 5 );
        continue;
      }

      if( errno == ETIMEDOUT ) // HBAs tend to do this
      {
        sleep( 5 );
        continue;
      }

      if( errno == ENODEV )
      {
        if( verbose >= 2 )
          fprintf( stderr, "The Device Disapeared on us, kernel probably re-enumerated it somewhere else.\n" );
        free( buff );
        return -1;
      }

      free( buff );
      return -1;
    }

    free( buff );
    return 0;
  }

  free( buff );

  if( verbose >= 2 )
    fprintf( stderr, "To Many Read Attempts\n" );

  return -1;
}

int main( int argc, char **argv )
{
  char *device;
  struct device_handle drive;
  enum driver_type driver;
  enum protocol_type protocol;
  int mode = 0;
  int state;

  int c;

  verbose = 1;

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
        else if( strcmp( optarg, "NVME" ) == 0 )
          driver = DRIVER_TYPE_NVME;
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
        else if( strcmp( optarg, "NVME" ) == 0 )
          protocol = PROTOCOL_TYPE_NVME;
        else
        {
          fprintf( stderr, "Unknwon Protocol '%s'\n", optarg );
          fprintf( stderr, usage_txt, argv[0] );
          exit( 1 );
        }

        break;

      case 'l':
        mode = MODE_SLEEP;
        break;
      case 'w':
        mode = MODE_WAKE;
        break;
      case 't':
        mode = MODE_STANDBY;
        break;
      case 'i':
        mode = MODE_IDLE;
        break;
      case 'a':
        mode = MODE_ACTIVE;
        break;

      case 's':
        mode = MODE_STATUS;
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

  if( openDisk( device, &drive, driver, protocol, NO_IDENT ) )
  {
    fprintf( stderr, "Error opening device '%s', errno: %i.\n", device, errno );
    exit( 1 );
  }

  if( ( ( mode == MODE_SLEEP ) || ( mode == MODE_WAKE ) ) && ( drive.protocol == PROTOCOL_TYPE_SCSI ) ) // TODO: NVME?
  {
    fprintf( stderr, "Sleep/Wake not supported for SCSI\n" );
    exit( 1 );
  }

  if( mode == MODE_SLEEP )
  {
    if( sleepState( &drive ) )
    {
      fprintf( stderr, "Error Putting disk to Sleep, errno %i.\n", errno );
      exit( 1 );
    }

    exit( 0 );
  }

  if( mode == MODE_WAKE )
  {
    if( doReset( &drive ) )
    {
      if( errno != ECONNRESET )
      {
        fprintf( stderr, "Error Resetting Disk, errno %i.\n", errno );
        exit( 1 );
      }
    }

    if( doSuspedToActive( &drive ) )
    {
      fprintf( stderr, "Error Putting disk to Active, errno %i.\n", errno );
      exit( 1 );
    }

    exit( 0 );
  }

  if( mode == MODE_STANDBY )
  {
    if( standbyState( &drive ) )
    {
      fprintf( stderr, "Error Putting disk to Standby, errno %i.\n", errno );
      exit( 1 );
    }

    exit( 0 );
  }

  if( mode == MODE_IDLE )
  {
    if( idleState( &drive ) )
    {
      fprintf( stderr, "Error Putting disk to Idle, errno %i.\n", errno );
      exit( 1 );
    }

    exit( 0 );
  }

  if( mode == MODE_ACTIVE )
  {
    if( doSuspedToActive( &drive ) )
    {
      fprintf( stderr, "Error Putting disk to Active, errno %i.\n", errno );
      exit( 1 );
    }

    exit( 0 );
  }

  if( mode == MODE_STATUS )
  {
    state = getPowerMode( &drive );
    if( state == -1 )
    {
      fprintf( stderr, "Error Getting Power Mode.\n" );
      exit( 1 );
    }

    switch( state )
    {
      case POWER_ACTIVE:
        printf( "Active\n" );
        break;
      case POWER_SLEEP:
        printf( "Sleep\n" );
        break;
      case POWER_STANDBY:
        printf( "Standby\n" );
        break;
      case POWER_IDLE:
        printf( "Idle\n" );
        break;
      case POWER_UNKNOWN:
        printf( "Unknown\n" );
        break;
      default:
        printf( "Invalid State\n" );
    }

    exit( 0 );
  }

  exit( 0 );
}
