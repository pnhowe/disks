#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "disk.h"

int verbose;

static char short_opts[] = "vqh?i:mstcorglbfak";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "help", no_argument, NULL, 'h' },
  { "interval", required_argument, NULL, 'i' },
  { "monitor", no_argument, NULL, 'm' },
  { "status", no_argument, NULL, 's' },
  { "test-only", no_argument, NULL, 't' },
  { "captive", no_argument, NULL, 'c' },
  { "conveyance", no_argument, NULL, 'o' },
  { "conveyance-or-short", no_argument, NULL, 'r' },
  { "long-or-short", no_argument, NULL, 'g' },
  { "long", no_argument, NULL, 'l' },
  { "short", no_argument, NULL, 'b' },
  { "offline", no_argument, NULL, 'f' },
  { "abort", no_argument, NULL, 'a' },
  { "no-support-ok ", no_argument, NULL, 'k' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v,  --verbose              Verbose, more -v more stuff.(4 levles, starts on level 1)\n\
  -q,  --quiet                Less Verbose, more -q less stuff.\n\
  -h,-?, --help               Show this\n\
  -i,  --interval             Polling interval, in seconds ( default: 120 (2 min) ).\n\
  -m,  --monitor              Follow status of allready running selftest\n\
  -s,  --status               Show the current status and exit\n\
  -t,  --test-only            Start selftest only, do not follow\n\
  -c,  --captive              Do a Captive/Foreground Selftest (In Conjunction with Short/Long/Conveyance)\n\
  -o,  --conveyance           Do a Conveyance test (not for SCSI)\n\
  -r,  --conveyance-or-short  Do a Conveyance test if supported otherwise short\n\
  -g,  --long-or-short        Do a Long test if supported otherwise short\n\
  -l,  --long                 Do a Long/Extended Selftest (not for all SCSI)\n\
  -b,  --short                Do a Short/Brief Selftest (default)\n\
  -f,  --offline              Do a Off-Line Selftest (not for SCSI)\n\
      NOTE: this disables captive, and is not monitorable, thus implies --test-only\n\
            and running --status or --monitor will return immeditally as if it completed\n\
  -a,  --abort                Abort/Stop a running Selftest\n\
  -k,  --no-support-ok        If the drive does not support Selftest, exit with 0\n\
\n\
NOTE: A captive test will get intruppted if the kernel tries to talk to the disk, and so will a status or a monitor command \n\
      a ctl-c during an interactive selftest will not stop the test, run again with -a to stop \n\
      if more than one of -o, -r, -l, or -s is specified, the last one wins \n\
\n";

//TODO: add a ctl-c top stop and cancel

#define SELFTEST_CONVEYANCE_OR_SHORT 0xFFFE
#define SELFTEST_EXTENDED_OR_SHORT 0xFFFF

static int startSelfTest( struct device_handle *drive, int type, int captive )
{
  int rc;
  if( type == SELFTEST_CONVEYANCE_OR_SHORT )
  {
    if( verbose )
      printf( "Starting Conveyance...\n" );

    if( captive )
      rc = smartSelfTest( drive, SELFTEST_CONVEYANCE | SELFTEST_CAPTIVE );
    else
      rc = smartSelfTest( drive, SELFTEST_CONVEYANCE );

    if( ( rc == -1 ) && ( errno == EBADE ) ) // Conveyance didn't work, try short
    {
      if( verbose )
        printf( "Conveyance not supported, trying Short...\n" );

      if( captive )
        return smartSelfTest( drive, SELFTEST_SHORT | SELFTEST_CAPTIVE );
      else
        return smartSelfTest( drive, SELFTEST_SHORT );
    }
    else
      return rc;
  }
  else if( type == SELFTEST_EXTENDED_OR_SHORT )
  {
    if( verbose )
      printf( "Starting Extended...\n" );

    if( captive )
      rc = smartSelfTest( drive, SELFTEST_EXTENDED | SELFTEST_CAPTIVE );
    else
      rc = smartSelfTest( drive, SELFTEST_EXTENDED );

    if( ( rc == -1 ) && ( errno == EBADE ) ) // Conveyance didn't work, try short
    {
      if( verbose )
        printf( "Extended not supported, trying Short...\n" );

      if( captive )
        return smartSelfTest( drive, SELFTEST_SHORT | SELFTEST_CAPTIVE );
      else
        return smartSelfTest( drive, SELFTEST_SHORT );
    }
    else
      return rc;
  }
  else
  {
    if( verbose )
    {
      if( type == SELFTEST_SHORT )
        printf( "Starting Short...\n" );
      else if( type == SELFTEST_EXTENDED )
        printf( "Starting Extended...\n" );
      else if( type == SELFTEST_CONVEYANCE )
        printf( "Starting Conveyance...\n" );
      else
        printf( "Starting Type %i...\n", type );
    }

    if( captive )
      return smartSelfTest( drive, type | SELFTEST_CAPTIVE );
    else
      return smartSelfTest( drive, type );
  }
}

int main( int argc, char **argv )
{
  // paramaters
  char *device;
  int monitor = 0;
  int status = 0;
  int testonly = 0;
  int type = SELFTEST_SHORT;
  int captive = 0;
  int abort = 0;
  int no_support_ok = 0;
  int interval = 120;  // 2 mins

  // workVars
  time_t curTime;
  struct device_handle drive;
  struct drive_info *info;
  char c;
  int rc;

  verbose = 1;

  if( argc < 2 )
  {
    fprintf( stderr, usage_txt, argv[0] );
    exit( 1 );
  }

  while( ( c = getopt_long( argc, argv, short_opts, long_opts, NULL ) ) != -1 )
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


      case 'i':
        interval = atoll( optarg );
        if ( interval < 2 )
          interval = 2;
        break;

      case 'm':
        monitor = 1;
        break;

      case 's':
        status = 1;
        break;

      case 't':
        testonly = 1;
        break;

      case 'c':
        captive = 1;
        break;

      case 'b':
        type = SELFTEST_SHORT;
        break;

      case 'l':
        type = SELFTEST_EXTENDED;
        break;

      case 'o':
        type = SELFTEST_CONVEYANCE;
        break;

      case 'r':
        type = SELFTEST_CONVEYANCE_OR_SHORT;
        break;

      case 'g':
        type = SELFTEST_EXTENDED_OR_SHORT;
        break;

      case 'f':
        type = SELFTEST_OFFLINE;
        break;

      case 'a':
        abort = 1;
        break;

      case 'k':
        no_support_ok = 1;
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

  if( openDisk( device, &drive, DRIVER_TYPE_UNKNOWN, PROTOCOL_TYPE_UNKNOWN, IDENT ) )
  {
    fprintf( stderr, "Error opening device '%s', errno: %i.\n", device, errno );
    exit( 1 );
  }

  if( verbose )
    printf( "On Device '%s'\n", device );

  info = getDriveInfo( &drive );

  if( !info->supportsSelfTest )
  {
    if( no_support_ok )
    {
      printf( "WARNING: Selftest not supported\n" );
      exit( 0 );
    }
    else
    {
      printf( "Selftest not supported\n" );
      exit( 1 );
    }
  }

  if( abort )
  {
    if( verbose )
      printf( "Aborting Running Self Test...\n" );

    if( smartSelfTest( &drive, SELFTEST_ABORT ) )
    {
      printf( "Error Aborting, errno %i.\n", errno );
      goto err;
    }
    goto ok;
  }

  if( captive && ( type != SELFTEST_OFFLINE ) )
  {
    if( verbose )
      printf( "Starting Captive Self Test...\n" );

    if( startSelfTest( &drive, type, 1 ) )
    {
      printf( "Error Starting Captive selftest, errno %i.\n", errno );
      goto err;
    }
    goto ok;
  }

  if( !status && !monitor )
  {
    if( verbose )
      printf( "Starting Self Test...\n" );

    if( startSelfTest( &drive, type, 0 ) )
    {
      fprintf( stderr, "Error Starting selftest, errno %i.\n", errno );
      goto err;
    }
  }

  if( testonly || ( type == SELFTEST_OFFLINE ) ) // can't get the status of offline tests, no point in trying
  {
    goto ok;
  }

  if( verbose )
    printf( "\n\n\n" );

  while( 1 )
  {
    if( verbose )
      printf( "Testing.... Status refreshing every %i seconds.\n\n\n", interval );

    rc = smartSelfTestStatus( &drive );
    if( rc == -1 )
    {
      fprintf( stderr, "Error getting Self Test Status, errno %i.\n", errno );
      goto err;
    }

    if( ( rc & SELFTEST_STATUS_MASK ) == SELFTEST_COMPLETE )
    {
      printf( "Selftest Complete.\n" );
      goto ok;
    }
    else if( ( rc & SELFTEST_STATUS_MASK ) == SELFTEST_PER_COMPLETE )
    {
      time( &curTime );
      if( verbose == 1 )  // otherwise we messup the output of the other levels of verbosity
        printf( "\x1b[1A\x1b[1A" );
      if( verbose )
      {
        if( drive.protocol == PROTOCOL_TYPE_ATA )
          printf( "%s  %i%% Complete            \n", ctime( &curTime ), ( rc & SELFTEST_VALUE_MASK ) ); // ctime appends a \n to it's return.
        else if( drive.protocol == PROTOCOL_TYPE_SCSI )
          printf( "%s  Segment %i               \n", ctime( &curTime ), ( rc & SELFTEST_VALUE_MASK ) ); // ctime appends a \n to it's return.
        else
          printf( "%s  Unknown Progress         \n", ctime( &curTime ) );
      }
    }
    else if( ( ( rc & SELFTEST_STATUS_MASK ) == SELFTEST_HOST_ABBORTED ) || ( ( rc & SELFTEST_STATUS_MASK ) == SELFTEST_HOST_INTERRUPTED ) )
    {
      fprintf( stderr, "Selftest was interrupted or abborted.\n" );
      goto err;
    }
    else if( ( rc & SELFTEST_STATUS_MASK ) == SELFTEST_FAILED )
    {
      fprintf( stderr, "Selftest failed. Reason: " );
      if( drive.protocol == PROTOCOL_TYPE_ATA )
      {
        switch( rc & SELFTEST_VALUE_MASK ) // from table 64 "Self-test execution status values"
        {
          case 0x03:
            fprintf( stderr, "Unable to Complete the Self-Test Routine\n" );
            break;
          case 0x04:
            fprintf( stderr, "Unknown Test Element Failed\n" );
            break;
          case 0x05:
            fprintf( stderr, "Electrical Element Failed\n" );
            break;
          case 0x06:
            fprintf( stderr, "Servo and/or Seek Element Failed\n" );
            break;
          case 0x07:
            fprintf( stderr, "Read Element Failed\n" );
            break;
          case 0x08:
            fprintf( stderr, "Suspected Handeling Damage\n" );
            break;
          default:
            fprintf( stderr, "Unknown 0x%x\n", ( rc & SELFTEST_VALUE_MASK ) );
        }
      }
      else if( drive.protocol == PROTOCOL_TYPE_SCSI )
      {
        switch( rc & SELFTEST_VALUE_MASK & 0x0f ) // from Table 416 — SELF-TEST RESULTS field
        {
          case 0x03:
            fprintf( stderr, "Unknown Device Error\n" );
            break;
          case 0x04:
            fprintf( stderr, "Unknown Test Segment Failed\n" );
            break;
          case 0x05:
            fprintf( stderr, "First Segment Failed\n" );
            break;
          case 0x06:
            fprintf( stderr, "Second Segment Failed\n" );
            break;
          case 0x07:
            fprintf( stderr, "Segment #%i Failed\n", ( rc & 0x0ff0 >> 4 ) );
            break;
          default:
            fprintf( stderr, "Unknown 0x%x\n", ( rc & SELFTEST_VALUE_MASK ) );
        }
      }
      else
        fprintf( stderr, "Unknown 0x%x\n", ( rc & SELFTEST_VALUE_MASK ) );

      goto err;
    }
    else if( ( rc & SELFTEST_STATUS_MASK ) == SELFTEST_UNKNOWN )
    {
      fprintf( stderr, "Selftest Stoped for an Unknown reason. Reason: 0x%x\n", rc & SELFTEST_VALUE_MASK );
      goto err;
    }

    if( status )
      break;

    sleep( interval );
  }

ok:

  closeDisk( &drive );

  exit( 0 );

err:
  closeDisk( &drive );

  exit( 1 );
}
