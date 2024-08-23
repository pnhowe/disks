#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "device.h"
#include "disk.h"

int verbose;

static char short_opts[] = "vqh?i:nsct:o:k:l:rCz";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "help", no_argument, NULL, 'h' },
  { "interval", required_argument, NULL, 'i' },
  { "no-wait", no_argument, NULL, 'n' },
  { "status", no_argument, NULL, 's' },
  { "cancel", no_argument, NULL, 'c' },
  { "start", required_argument, NULL, 't' },
  { "count", required_argument, NULL, 'o' },
  { "chunk-size", required_argument, NULL, 'k' },
  { "value", required_argument, NULL, 'l' },
  { "trim", no_argument, NULL, 'r' },
  { "check", no_argument, NULL, 'C' },
  { "trim-or-zero", no_argument, NULL, 'z' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v,  --verbose         Verbose, more -v more stuff.( 4 levels, starts on level 1 )\n\
  -q,  --quiet           Less Verbose, more -q less stuff.\n\
  -h,-?, --help          Show this\n\
  -i,  --interval        Polling interval, in seconds ( default: 30 ) ( ATA Only ).\n\
  -n,  --no-wait         Do not wait for Wipe to complete.\n\
                          Status can be checked with -s.\n\
  -s,  --status          Display Status of a wipe allready in progress ( ATA Only )\n\
  -c,  --cancel          Cancel wipe of drive in progress ( ATA Only )\n\
  -t,  --start           Starting LBA ( default: 0 )\n\
  -o,  --count           Number of LBAs ( default: 0 -> means end of disk )\n\
                          NOTE: ATA max count is 48 bit where SCSI is 24 bit\n\
  -k,  --chunk-size      Wipe Chunk Size, 0 will do one request for the entire wiping\n\
                          Area.  This Is usefull on SCSI where the request blocks,\n\
                          to enable status updates.  Also for times when a RAID\n\
                          controller has it's own timeout. May also help in times\n\
                          where you want to have more controll in cancelling the\n\
                          wipe. In LBAs. ( SCSI/Trim Only ) ( default: 0 )\n\
  -l,  --value           32bit value to write ( default: 0 )\n\
  -r,  --trim            Trim/UNmap instead of writing value,\n\
                          use -C to check for support ( interval ignored )\n\
  -C,  --check           Check to see if the disk is compaitble\n\
  -z,  --trim-or-zero    If trim is support it will trim, otherwise value 0\n\
                          use -C to see what will be done and if it's compatable\n\
                          ( value ignored, and interval if trim happens )\n\
\n\
NOTE: Some operations on the drive such as checking the smart status will cancel\n\
  the wipe of the drive.  Status will show the last wiped sector if that happens.\n\
NOTE 2: Wiping is done by the drive internally, killing this process doen't guarentee\n\
  stopping of the wipe process.  If you want it stopped do a -c to make sure.\n\
NOTE 3: For SCSI Disks the command to wipe blocks until it completes.  Meaning status\n\
  and status dosen't work, also killing the process has an undetermined affect on the\n\
  wiping command in progress.\n\
NOTE 4: For Trim, command will block until it completes, same as SCSI.\n\
\n";

int wipeContinue;

void signalHandler( int sig )
{
  wipeContinue = 0;
  printf( "Recieved Signal %i.  Canceling... please wait.\n" , sig );
}

#define GENERAL_ERROR        -1

#define STATUS_RUNNING       0
#define STATUS_DONE          1
#define STATUS_INTERRUPTED   2
#define STATUS_STOPPED_OTHER 3


int doStatus( struct device_handle *drive, struct drive_info *info )
{
  long long int curLBA;

  if( drive->protocol != PROTOCOL_TYPE_ATA )
  {
    fprintf( stderr, "Not supported for this drive." );
    return GENERAL_ERROR;
  }

  curLBA = wirteSameStatus( drive );  // TODO: also check for other smart ops, like the long test, make a STATUS_BUSY for things like that
  if( curLBA == -1 )
  {
    fprintf( stderr, "Error getting WriteSame Status. rc: %lli, errno: %i\n", curLBA, errno );
    return GENERAL_ERROR;
  }

  if( curLBA & WRITESAME_STATUS_DONE_STATUS )
  {
    curLBA &= ~WRITESAME_STATUS_DONE_STATUS;
    if( curLBA == 0x000 )
    {
      if( verbose )
        printf( "Wipe is completed.\n" );
      return STATUS_DONE;
    }
    else if( curLBA == 0x0008 )
    {
      if( verbose )
        printf( "Wipe has been interrupted.\n" );
      return STATUS_INTERRUPTED;
    }
    else
    {
      if( verbose )
        printf( "Wipe is not running... Reason 0x%llx.\n", curLBA );
      return STATUS_STOPPED_OTHER;
    }
  }

  if( curLBA & WRITESAME_STATUS_LBA )
  {
    if( verbose )
      printf( "At LBA %lli (%f%%)\n", curLBA, ( (float) curLBA / (float) info->LBACount ) * 100.0 );
    return STATUS_RUNNING;
  }

  fprintf( stderr, "Error getting status %lli (0x%llx).\n", curLBA, curLBA );
  return GENERAL_ERROR;
}


int doCancel( struct device_handle *drive, struct drive_info *info )
{
  int rc;

  rc = doStatus( drive, info );
  if( rc == GENERAL_ERROR )
    return GENERAL_ERROR;

  if( rc != STATUS_RUNNING ) // Not running, we are good bail
    return 0;

  rc = smartStatus( drive ); // Generally any SMART Command should cancel the Wipe
  if( rc == -1 )
  {
    fprintf( stderr, "Error Canceling.\n" );
    return GENERAL_ERROR;
  }

  rc = doStatus( drive, info );
  if( rc == GENERAL_ERROR )
    return GENERAL_ERROR;

  return 0;
}

int doTrimWipe( struct device_handle *drive, struct drive_info *info, long long int start, long long int count, long long int chunkSize )
{
  int rc;
  time_t startTime;
  time_t curTime;
  float minRemaining;
  long long int curLBA;
  long long int endingLBA;

  if( chunkSize )
  {
    time( &startTime );
    if( count == 0 )
      endingLBA = info->LBACount;
    else
      endingLBA = start + count;

    for( curLBA = start; curLBA < endingLBA; curLBA += chunkSize ) // TODO: Do some math, make sure this works out
    {
      if( ( curLBA + chunkSize ) > endingLBA )
        chunkSize = endingLBA - curLBA;

      time( &curTime );
      minRemaining = ( ( curTime - startTime ) / ( (float) curLBA / (float) endingLBA ) * ( 1 - ( (float) curLBA / (float) endingLBA ) ) ) / 60;
      if( verbose == 1 )  // otherwise we messup the output of the other levels of verbosity
        printf( "\x1b[1A\x1b[1A" );
      if( verbose )
        printf( "%s  %f%% Complete, %.2f minutes remaining.            \n", ctime( &curTime ), ( (float) curLBA / (float) endingLBA ) * 100.0, minRemaining ); // ctime appends a \n to it's return.

      rc = trim( drive, curLBA, chunkSize );
      if( rc )
      {
        fprintf( stderr, "Error asking drive to trim. rc: %i, errno %i.\n", rc, errno );
        return GENERAL_ERROR;
      }
    }
  }
  else
  {
    if( verbose )
    {
      time_t curTime;
      time( &curTime );
      printf( "Trim, will block till complete, no status polling.\n" );
      printf( "%s  ??? Complete, ??? minutes remaining.       \n", ctime( &curTime ) ); // ctime appends a \n to it's return.
    }
    rc = trim( drive, start, count );
    if( rc )
    {
      fprintf( stderr, "Error asking drive to trim. rc: %i, errno %i.\n", rc, errno );
      return GENERAL_ERROR;
    }
  }

  return 0;
}



int doATAWipe( struct device_handle *drive, struct drive_info *info, long long int start, long long int count, unsigned long int value, int interval )
{
  int rc;
  long long int curLBA;
  long long int endingLBA;
  time_t startTime;
  time_t curTime;
  float minRemaining;

  wipeContinue = 1;
  signal( SIGINT, signalHandler );
  signal( SIGHUP, signalHandler );
  signal( SIGALRM, signalHandler );
  signal( SIGTERM, signalHandler );

  rc = doStatus( drive, info );
  if( rc == GENERAL_ERROR )
    return GENERAL_ERROR;

  if( rc == STATUS_RUNNING )
  {
    fprintf( stderr, "Drive allready in preforming a wipe, cancel the old one first.\n" );
    return -1;
  }

  rc = writeSame( drive, start, count, value );
  if( rc )
  {
    fprintf( stderr, "Error asking drive to wipe (ATA). rc: %i, errno %i.\n", rc, errno );
    return GENERAL_ERROR;
  }

  if( interval )
  {
    time( &startTime );
    if( verbose )
    {
      printf( "Wiping.... Status refreshing every %i seconds.\n\n\n", interval );
      sleep( 1 ); // short runup to get a first guess of time remaining
    }

    if( count == 0 )
      endingLBA = info->LBACount;
    else
      endingLBA = start + count;

    while( wipeContinue )
    {
      curLBA = wirteSameStatus( drive );
      if( curLBA == -1 )
      {
        fprintf( stderr, "Error getting WriteSame Status. rc: %lli, errno: %i.\n", curLBA, errno );
        return GENERAL_ERROR;
      }
      else if( !( curLBA & WRITESAME_STATUS_LBA ) )
        break;
      curLBA &= ~WRITESAME_STATUS_LBA;
      time( &curTime );
      minRemaining = ( ( curTime - startTime ) / ( (float) curLBA / (float) endingLBA ) * ( 1 - ( (float) curLBA / (float) endingLBA ) ) ) / 60;
      if( verbose == 1 )  // otherwise we messup the output of the other levels of verbosity
        printf( "\x1b[1A\x1b[1A" );
      if( verbose )
        printf( "%s  %f%% Complete, %.2f minutes remaining.            \n", ctime( &curTime ), ( (float) curLBA / (float) endingLBA ) * 100.0, minRemaining ); // ctime appends a \n to it's return.
      sleep( interval );
    }

    if( !wipeContinue )
    {
      if( verbose )
        printf( "Canceling...\n" );
      if( doCancel( drive, info ) == GENERAL_ERROR )
        return GENERAL_ERROR;
    }

    rc = doStatus( drive, info );
    if( rc == GENERAL_ERROR )
      return GENERAL_ERROR;
  }

  return 0;
}


int doSCSIWipe( struct device_handle *drive, struct drive_info *info, long long int start, long long int count, unsigned long int value, long long int chunkSize )
{
  int rc;
  int oldTimeout;
  time_t startTime;
  time_t curTime;
  float minRemaining;
  long long unsigned curLBA;
  long long unsigned endingLBA;

  if( chunkSize )
  {
    time( &startTime );
    if( count == 0 )
      endingLBA = info->LBACount;
    else
      endingLBA = start + count;

    for( curLBA = start; curLBA < endingLBA; curLBA += chunkSize ) // TODO: Do some math, make sure this works out
    {
      time( &curTime );
      minRemaining = ( ( curTime - startTime ) / ( (float) curLBA / (float) endingLBA ) * ( 1 - ( (float) curLBA / (float) endingLBA ) ) ) / 60;
      if( verbose == 1 )  // otherwise we messup the output of the other levels of verbosity
        printf( "\x1b[1A\x1b[1A" );

      if( verbose )
        printf( "%s  %f%% Complete, %.2f minutes remaining.            \n", ctime( &curTime ), ( (float) curLBA / (float) endingLBA ) * 100.0, minRemaining ); // ctime appends a \n to it's return.

      oldTimeout = setTimeout( 43200 ); // 12 hours

      if( ( curLBA + chunkSize ) > endingLBA )
      {  // Some disks seem to want to have chunkSize for all reqests, even if it's going to run off the end, other disks you have to end right at the end of the disk
        rc = writeSame( drive, curLBA, ( endingLBA - curLBA ), value );
        if( rc )
        {
          if( endingLBA == info->LBACount ) // the stop point is the disk, we are OK to try to run past the end
            rc = writeSame( drive, curLBA, chunkSize, value );

          if( rc )
          {
            fprintf( stderr, "Error asking drive to wipe (SCSI Chunked End). rc: %i, errno %i.\n", rc, errno );
            return GENERAL_ERROR;
          }
        }
      }
      else
      {
        rc = writeSame( drive, curLBA, chunkSize, value );
        if( rc )
        {
          fprintf( stderr, "Error asking drive to wipe (SCSI Chunked). rc: %i, errno %i.\n", rc, errno );
          return GENERAL_ERROR;
        }
      }
      setTimeout( oldTimeout );
    }
  }
  else
  {
    if( verbose )
    {
      time_t curTime;
      time( &curTime );
      printf( "SCSI Disk, will block till complete, no status polling.\n" );
      printf( "%s  ??? Complete, ??? minutes remaining (up to 12 hours).       \n", ctime( &curTime ) ); // ctime appends a \n to it's return.
    }
    oldTimeout = setTimeout( 43200 ); // 12 hours
    rc = writeSame( drive, start, count, value );
    setTimeout( oldTimeout );
    if( rc )
    {
      fprintf( stderr, "Error asking drive to wipe (SCSI). rc: %i, errno %i.\n", rc, errno );
      return GENERAL_ERROR;
    }
  }

  return 0;
}


int main( int argc, char **argv )
{
  // paramaters
  int waitForCompletion = 1;
  int statusOnly = 0;
  int cancelOnly = 0;
  int doCheck = 0;
  int doTrim = 0;
  int doTrimOrZero = 0;
  int interval = 30;
  char *device;

  // workVars
  int rc;
  struct device_handle drive;
  struct drive_info *info;
  char c;
  long long int start = 0;
  long long int count = 0;
  long long int chunkSize = 0;
  unsigned long int value = 0;

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

      case 'n':
        waitForCompletion = 0;
        break;

      case 's':
        statusOnly = 1;
        break;

      case 'c':
        cancelOnly = 1;
        break;

      case 't':
        start = atoll( optarg );
        if ( start < 0 )
          start = 0;
        break;

      case 'o':
        count = atoll( optarg );
        break;

      case 'l':
        value = atoll( optarg );
        break;

      case 'k':
        chunkSize = atoll( optarg );
        break;

      case 'r':
        doTrim = 1;
        break;

      case 'C':
        doCheck = 1;
        break;

      case 'z':
        doTrimOrZero = 1;
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

  if( statusOnly || cancelOnly )
  {
    if( openDisk( device, &drive, DRIVER_TYPE_UNKNOWN, PROTOCOL_TYPE_UNKNOWN, NO_IDENT ) )
    {
      fprintf( stderr, "Error opening device '%s', errno: %i.\n", device, errno );
      exit( 1 );
    }
  }
  else
  {
    if( openDisk( device, &drive, DRIVER_TYPE_UNKNOWN, PROTOCOL_TYPE_UNKNOWN, IDENT ) )
    {
      fprintf( stderr, "Error opening device '%s', errno: %i.\n", device, errno );
      exit( 1 );
    }
  }

  if( verbose )
    printf( "On Device '%s'\n", device );

  info = getDriveInfo( &drive );

  if( doTrimOrZero ) // TODO: support Sanitize
  {
    if( info->supportsTrim )
    {
      if( verbose )
        fprintf( stderr, "Using Trim.\n" );
      doTrim = 1;
    }
    else
    {
      if( verbose )
        fprintf( stderr, "Using WriteSame with 0.\n" );
      value = 0;
    }
  }

  if( doCheck )
  {
    if( doTrim )
    {
      if( !info->supportsTrim )
      {
        if( verbose )
          fprintf( stderr, "Drive is NOT Supported.\n" );
        exit( 1 );
      }
      else
      {
        if( verbose )
          fprintf( stderr, "Drive is Supported.\n" );
        exit( 0 );
      }
    }
    else
    {
      if( !info->supportsWriteSame )
      {
        if( verbose )
          fprintf( stderr, "Drive is NOT Supported.\n" );
        exit( 1 );
      }
      else
      {
        if( verbose )
          fprintf( stderr, "Drive is Supported.\n" );
        exit( 0 );
      }
    }
  }

  if( !doTrim )
  {
    if( statusOnly || cancelOnly )  // We have to fake it so commands.c will run our command, getting the info to set these would cancel the wipe
    {
      info->supportsWriteSame = 1;
      info->supportsSCT = 1;
    }
    else
    {
      if( !info->supportsWriteSame && !info->supportsSCT )
      {
        fprintf( stderr, "Drive does not support WriteSame nor SCT.\n" );
        goto err;
      }
    }
  }

  if( doTrim )
  {
    if( ( start * info->LogicalSectorSize ) % info->PhysicalSectorSize )
    {
      if( verbose )
        fprintf( stderr, "start is not a multiple of physical size ( ie: ( start * Logical size ) %% Physical Size != 0 ).\n" );
      exit( 1 );
    }

    if( ( count * info->LogicalSectorSize ) % info->PhysicalSectorSize )
    {
      if( verbose )
        fprintf( stderr, "count is not a multiple of physical size ( ie: ( count * Logical size ) %% Physical Size != 0 ).\n" );
      exit( 1 );
    }

    if( ( chunkSize * info->LogicalSectorSize ) % info->PhysicalSectorSize )
    {
      if( verbose )
        fprintf( stderr, "chunk-size is not a multiple of physical size ( ie: ( chunk-size * Logical size ) %% Physical Size != 0 ).\n" );
      exit( 1 );
    }
  }

  if( cancelOnly )
  {
    if( doTrim )
    {
      fprintf( stderr, "Cancel is not supported for Trim, bailing.\n" );
      rc = 1;
    }
    else if( drive.protocol != PROTOCOL_TYPE_ATA )
    {
      fprintf( stderr, "Cancel only supported for ATA, bailing.\n" );
      rc = 1;
    }
    else
    {
      if( doCancel( &drive, info ) == GENERAL_ERROR )
        rc = 1;
      else
        rc = 0;
    }
  }
  else if( statusOnly )
  {
    if( doTrim )
    {
      fprintf( stderr, "Status is not supported for Trim, bailing.\n" );
      rc = 1;
    }
    else if( drive.protocol != PROTOCOL_TYPE_ATA )
    {
      fprintf( stderr, "Status only supported for ATA, bailing.\n" );
      rc = 1;
    }
    else
    {
      if( doStatus( &drive, info ) == GENERAL_ERROR )
        rc = 1;
      else
        rc = 0;
    }
  }
  else if( doTrim )
  {
    if( !waitForCompletion )
    {
      fprintf( stderr, "Not waiting for Completion isn't supported for Trim, bailing.\n" );
      rc = 1;
    }
    else
    {
      if( doTrimWipe( &drive, info, start, count, chunkSize ) == GENERAL_ERROR )
        rc = 1;
      else
        rc = 0;
    }
  }
  else if( drive.protocol == PROTOCOL_TYPE_ATA )
  {
    if( !waitForCompletion )
      interval = 0;

    if( chunkSize )
    {
      fprintf( stderr, "WARNING!! Chunk Size not Supported for ATA, ignoring chunksize.\n" );
      //fprintf( stderr, "Chunk Size not Supported for ATA, bailing.\n" ); // would rather bale, but until dtest detects best paramaters, going to have to ignore it
      //rc = 1;
    }
    //else
    //{
      if( doATAWipe( &drive, info, start, count, value, interval ) == GENERAL_ERROR )
        rc = 1;
      else
        rc = 0;
    //}
  }
  else if( drive.protocol == PROTOCOL_TYPE_SCSI )
  {
    if( !waitForCompletion )
    {
      fprintf( stderr, "Not waiting for Completion isn't supported for SCSI, bailing.\n" );
      rc = 1;
    }
    else
    {
      if( doSCSIWipe( &drive, info, start, count, value, chunkSize ) == GENERAL_ERROR )
        rc = 1;
      else
        rc = 0;
    }
  }
  else
  {
    fprintf( stderr, "Hm... this is awkward....\n" );
    rc = 1;
  }

  closeDisk( &drive );

  if( verbose )
    printf( "Done.\n" );

  exit( rc );


err:
  closeDisk( &drive );

  exit( 1 );
}
