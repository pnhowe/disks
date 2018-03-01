/*
  muncher
  adapted from thrash  http://www.csc.liv.ac.uk/~greg/thrash/
*/
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>

#include "device.h"
#include "disk.h"

#define VERSION "0.1"

static char short_opts[] = "vqh?rwbs:e:c:a:p:f:idut:";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "help", no_argument, NULL, 'h' },
  { "read", no_argument, NULL, 'r' },
  { "write", no_argument, NULL, 'w' },
  { "both", no_argument, NULL, 'b' },
  { "start", required_argument, NULL, 's' },
  { "end", required_argument, NULL, 'e' },
  { "count", required_argument, NULL, 'c' },
  { "random", required_argument, NULL, 'a' },
  { "sweep", required_argument, NULL, 'p' },
  { "full", required_argument, NULL, 'f' },
  { "ignore", no_argument, NULL, 'i' },
  { "DMA", no_argument, NULL, 'd' },
  { "FUA", no_argument, NULL, 'u' },
  { "set-cache", required_argument, NULL, 't' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " VERSION "\n\
Usage: [OPTIONS] device\n\
Options:\n\
  -v,  --verbose      Verbose, more -v more stuff.(4 levles, starts on level 1)\n\
  -q,  --quiet        Less Verbose, more -q less stuff.\n\
  -h,-?,--help        Show this\n\
      Read/Write, note: if multiple are passed, the last one takes priority\n\
  -r, --read          Do Read Test only (default)\n\
  -w, --write         Do Write Test only (DESCTRUCTIVE, will remove data on device\n\
  -b, --both          Do Read and Write Test(DESCTRUCTIVE, will remove data on device\n\
                         The Start/End LBAs are considered valid, ie inclusive.\n\
  -s, --start         Starting LBA (default = 0)\n\
  -e, --end           Ending LBA (0 = max LBA) (default = 0)\n\
\n\
  -c, --count         Number of rounds of the test (default = 1)\n\
  -a  --random        Random Seeks, 0 disables (default = 10000)\n\
  -p  --sweep         Sweep IDOD Steps, 0 disables (default = 1000)\n\
  -f  --full          Full IDOD Swings, 0 disables (default = 100), Note, disabling the Sweep disables Full IDOD\n\
\n\
  -i, --ignore        Don't stop the test when errors are returned\n\
\n\
  -d, --DMA           Use DMA instead of PIO (ATA)\n\
  -u, --FUA           Use Enable FUA (SCSI)\n\
  -t, --set-cache=(on/off) Tell the drive to enable or disable its write cache (Ignored for SCSI for now)\n\
\n\
NOTE: DMA and FUA might cause problems with your driver/HBA, try a quick (ie. -a5 -p0 -f0) run with and without\n\
-d and -u to see what is safe for your setup (WARNING: this may lockup your machine.  It is prefered to have\n\
-d and -u if your setup will support it. DMA is faster and less overhead on the CPU, and FUA will make the drive\n\
more honest.  That being said, -t off should also have the same effect as -u, but not all drives/drivers/HBAs are\n\
created equal.\n\
\n\
";

//TODO: and -r for retry count, incase the drive hickups how many times to try the op

#define DO_READ( _LBA_ ) rCount++; \
rc = readLBA( &drive, DMA, FUA, _LBA_, 1, readBuff ); \
if( rc ) \
{ \
  if( ignoreErrors ) \
    printf( "Warning, error with read operation at LBA %llu, errno: %i, ignored.\n", _LBA_, errno ); \
  else \
  { \
    fprintf( stderr, "Error with read operation at LBA %llu, errno: %i, bailling.\n", _LBA_, errno ); \
    ec = 1; \
    goto out; \
  } \
}

#define DO_WRITE( _LBA_ ) wCount++; \
rc = writeLBA( &drive, DMA, FUA, _LBA_, 1, writeBuff ); \
if( rc ) \
{ \
  if( ignoreErrors ) \
    printf( "Warning, error with write operation at LBA %llu, errno: %i, ignored.\n", _LBA_, errno ); \
  else \
  { \
    fprintf( stderr, "Error with write operation at LBA %llu, errno: %i, bailling.\n", _LBA_, errno ); \
    ec = 1; \
    goto out; \
  } \
}

// shared with commands.c and sgio.c
int verbose;

// silly globals
int ODIDStep;
__u64 startLBA;
__u64 endLBA;

unsigned int rCount;
unsigned int wCount;

// Rand work vars
int randBits, LBABits, LBARounds, LBALastBits;
unsigned int LBALastMask, ODIDMask, ODIDMid;

void initRand()
{
  time_t tmpTime;

  time( &tmpTime );
  srand( tmpTime );

  randBits = ceil( log10( RAND_MAX ) / log10( 2 ) );
  LBABits = ceil( log10( endLBA ) / log10( 2 ) ); // TODO: be smarter with the start/end don't need to rand from 0 to end
  LBARounds = floor( LBABits / randBits );
  LBALastBits = LBABits % randBits;
  LBALastMask = INT_MAX >> ( randBits - LBALastBits );

  int tmpNum = ( ( (int)floor( log10( ODIDStep ) / log10( 2 ) ) ) >> 1 ); // div by 2 so we can go +- the center with one number
  if( tmpNum >= randBits )
    ODIDMask = INT_MAX;  // do we really want another loop? if the step is that big, are we wanting to be that granular?
  else
    ODIDMask = INT_MAX >> ( randBits - tmpNum );

  ODIDMid = ODIDMask >> 1;
}

/*inline*/ __u64 getRandLBA()
{
  int r;
  __u64 wrkNum;

again:
  wrkNum = 0;
  for( r = 0; r < LBARounds; r++ )
  {
    wrkNum <<= randBits;
    wrkNum += rand();
  }
  wrkNum <<= LBALastBits;
  wrkNum += rand() & LBALastMask;

  if( ( wrkNum > endLBA ) || ( wrkNum < startLBA ) )
    goto again;

  return wrkNum;
}

/*inline*/ __u64 getOffsetLBA( __u64 target )
{
  __u64 wrkNum;

  wrkNum = target + ( rand() & ODIDMask );
  if( wrkNum - ODIDMid < startLBA )
    return startLBA;

  wrkNum -= ODIDMid;

  if( wrkNum > endLBA )
    return endLBA;

  return wrkNum;
}

inline int flipCoin()
{
  return rand() & 0x01;
}

int main( int argc, char **argv )
{
  // paramaters
  int readTest = 1;
  int writeTest = 0;
  int ignoreErrors = 0;
  unsigned int roundCount = 1;
  char *device;
  int DMA = 0;
  int FUA = 0;
  int setcache = -1;
  int ec = 2;

  unsigned int randSeekCount = 10000;
  unsigned int ODIDCount     =  1000;
  unsigned int FullODIDCount =   100; // disabled if ODIDCount = 0

  // workVars
  unsigned int buffSize;
  __u8 *readBuff;
  __u8 *writeBuff;
  char c;
  unsigned int iopsPerRound;
  unsigned int i;
  unsigned int round;
  time_t startTime;
  time_t endTime;
  struct device_handle drive;
  struct drive_info *info;
  __u64 totalLBACount;
  __u64 LBA;
  int rc;

  startLBA = 0;
  endLBA = 0;

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

      case 'r':
        readTest = 1;
        writeTest = 0;
        break;

      case 'w':
        readTest = 0;
        writeTest = 1;
        break;

      case 'b':
        readTest = 1;
        writeTest = 1;
        break;

      case 's':
        startLBA = atoll( optarg );
        break;

      case 'e':
        endLBA = atoll( optarg );
        break;

      case 'c':
        roundCount = atoi( optarg );
        break;

      case 'a':
        randSeekCount = atoi( optarg );
        break;

      case 'p':
        ODIDCount = atoi( optarg );
        break;

      case 'f':
        FullODIDCount = atoi( optarg );
        break;

      case 'i':
        ignoreErrors = 1;
        break;

      case 'd':
        DMA = 1;
        break;

      case 'u':
        FUA = 1;
        break;

      case 't':
        if( strcasecmp( "on", optarg ) == 0 )
          setcache = 1;
        else if( strcasecmp( "off", optarg ) == 0 )
          setcache = 0;
        else
        {
          fprintf( stderr, "-t/--set-cache must be set to \"on\" or \"off\"\n" );
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

  if( !readTest && !writeTest )
  {
    fprintf( stderr, "Must have Read and/or Write test selected\n" );
    exit( 1 );
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

  info = getDriveInfo( &drive );

  if( !info->supportsLBA )
  {
    fprintf( stderr, "Device '%s' Dosen't support LBAs.\n", device );
    goto err;
  }

  if( verbose )
  {
    // print what type of test we are running
    printf( "On Device '%s'\n", device );
    if( readTest && writeTest )
      printf( "Doing both Read and Write test\n" );
    else if( writeTest )
      printf( "Doing Write only test\n" );
    else if( readTest )
      printf( "Doing Read only test\n" );
    else
    {
      fprintf( stderr, "Eh?\n" );
      goto err;
    }
  }

  buffSize = info->LogicalSectorSize; // one logical sector please
  if( buffSize == 0 )
  {
    fprintf( stderr, "Error getting the Logical Sector Size for '%s'\n", device );
    goto err;
  }

  if( verbose )
    printf( "Logical Sector Size is %i\n", buffSize );

  totalLBACount = info->LBACount;

  if( totalLBACount == 0 )
  {
    fprintf( stderr, "Error getting the LBACount for '%s'\n", device );
    goto err;
  }

  if( ( setcache == 0 ) && ( drive.protocol == PROTOCOL_TYPE_ATA ) )
  {
    if( disableWriteCache( &drive ) )
    {
      fprintf( stderr, "Error Disabeling Write Cache, errno %i.\n", errno );
      goto err;
    }
    if( verbose )
      printf( "Write Cache Disabled\n" );
  }
  else if( ( setcache == 1 ) && ( drive.protocol == PROTOCOL_TYPE_ATA ) )
  {
    if( enableWriteCache( &drive ) )
    {
      fprintf( stderr, "Error Enabeling Write Cache, errno %i.\n", errno );
      goto err;
    }
    if( verbose )
      printf( "Write Cache Enabled\n" );
  }

  if( verbose )
  {
    if( DMA && FUA  )
      printf( "Using DMA with FUA\n" );
    else if( DMA )
      printf( "Using DMA\n" );
    else if( FUA )
      printf( "Using PIO with FUA\n" );
    else
      printf( "Using PIO\n" );
  }

  if( endLBA == 0 )
    endLBA = totalLBACount;
  else if( endLBA > totalLBACount )
    endLBA = totalLBACount;

  if( ODIDCount != 0 )
  {
    ODIDStep = totalLBACount / ODIDCount;
    if( ODIDStep < 30 )
      ODIDStep = 30;
  }
  else
  {
    ODIDStep = 0;
    FullODIDCount = 0;
  }

  iopsPerRound = randSeekCount + ( ODIDCount * 4 ) + ( FullODIDCount * 2 );

  if( verbose )
    printf( "Going to do apx %llu IO ops\n", (__u64) iopsPerRound * (__u64) roundCount );

  if( readTest && writeTest )
  {
    randSeekCount /= 3;
    ODIDStep *= 3;
    FullODIDCount /= 3;
    ODIDCount /= 3;
  }

  initRand(); // startLBA, endLBA, ODIDStep must be set first

  __u64 wrkLBA;
  __u64 wrkLBA2;

  // from here down, must set ec and goto out to clean up
  readBuff = memalign( sysconf( _SC_PAGESIZE ), buffSize );
  writeBuff = memalign( sysconf( _SC_PAGESIZE ), buffSize );

  memset( readBuff, 0, buffSize );
  memset( writeBuff, 0, buffSize );

  rCount = 0;
  wCount = 0;

  time( &startTime );

  for( round = 0; round < roundCount; round++ )
  {
    if( verbose )
      printf( "Starting round %i of %i...\n", ( round + 1 ), roundCount );

    if( writeTest )
    {
      switch( rand() & 0x07 )
      {
        case( 0 ):
          for( i = 0; i < buffSize; i++ )
            writeBuff[i] = i;
          break;
        case( 1 ):
          memset( writeBuff, 0x00, sizeof( writeBuff ) );
          break;
        case( 2 ):
          memset( writeBuff, 0x01, sizeof( writeBuff ) );
          break;
        case( 3 ):
          memset( writeBuff, 0xFF, sizeof( writeBuff ) );
          break;
        case( 4 ):
          memset( writeBuff, 0xAA, sizeof( writeBuff ) );
          break;
        case( 5 ):
          memset( writeBuff, 0x80, sizeof( writeBuff ) );
          break;
         // 6, 7 -> same as last round
      }
    }

    if( readTest )
    {
      if( verbose )
        printf( "  Doing %i Random Reads...\n", randSeekCount );
      for( i = 0; i < randSeekCount; i++ )
      {
        LBA = getRandLBA();
        DO_READ( LBA )
      }
    }
    if( writeTest )
    {
      if( verbose )
        printf( "  Doing %i Random Writes...\n", randSeekCount );
      for( i = 0; i < randSeekCount; i++ )
      {
        LBA = getRandLBA();
        DO_WRITE( LBA )
      }
    }

    if( readTest )
    {
      if( ODIDCount != 0 )
      {
        if( verbose )
          printf( "  Doing %i-%i-%i ID-OD Reads...\n", ODIDCount, FullODIDCount, ODIDCount );

        // ID to Full
        wrkLBA = ODIDStep;
        for( wrkLBA2 = ODIDStep; wrkLBA2 < ( endLBA - ODIDStep ); wrkLBA2 += ODIDStep )
        {
          DO_READ( getOffsetLBA( wrkLBA ) )
          DO_READ( getOffsetLBA( wrkLBA2 ) )
        }

        // Full
        wrkLBA2 = totalLBACount - ODIDStep;
        for( i = 0; i < FullODIDCount; i++ )
        {
          DO_READ( getOffsetLBA( wrkLBA ) )
          DO_READ( getOffsetLBA( wrkLBA2 ) )
        }

        // FUll to ID
        for( wrkLBA = ODIDStep; wrkLBA < ( endLBA - ODIDStep ); wrkLBA += ODIDStep )
        {
          DO_READ( getOffsetLBA( wrkLBA ) )
          DO_READ( getOffsetLBA( wrkLBA2 ) )
        }
      }
    }

    if( writeTest )
    {
      if( ODIDCount != 0 )
      {
        if( verbose )
          printf( "  Doing %i-%i-%i ID-OD Writes...\n", ODIDCount, FullODIDCount, ODIDCount );

        // ID to Full
        wrkLBA = ODIDStep;
        for( wrkLBA2 = ODIDStep; wrkLBA2 < ( endLBA - ODIDStep ); wrkLBA2 += ODIDStep )
        {
          DO_WRITE( getOffsetLBA( wrkLBA ) )
          DO_WRITE( getOffsetLBA( wrkLBA2 ) )
        }

        // Full
        wrkLBA2 = endLBA - ODIDStep;
        for( i = 0; i < FullODIDCount; i++ )
        {
          DO_WRITE( getOffsetLBA( wrkLBA ) )
          DO_WRITE( getOffsetLBA( wrkLBA2 ) )
        }

        // FUll to ID
        for( wrkLBA = ODIDStep; wrkLBA < ( endLBA - ODIDStep ); wrkLBA += ODIDStep )
        {
          DO_WRITE( getOffsetLBA( wrkLBA ) )
          DO_WRITE( getOffsetLBA( wrkLBA2 ) )
        }
      }
    }

    if( readTest && writeTest )
    {
      if( verbose )
        printf( "  Doing %i Random Mixed Reads and Writes...\n", randSeekCount );

      for( i = 0; i < randSeekCount; i++ )
      {
        if( flipCoin() )
        {
          LBA = getRandLBA();
          DO_READ( LBA )
        }
        else
        {
          LBA = getRandLBA();
          DO_WRITE( LBA )
        }
      }

      if( ODIDCount != 0 )
      {
        if( verbose )
          printf( "  Doing %i-%i-%i ID-OD Mixed Reads and Writes...\n", ODIDCount, FullODIDCount, ODIDCount );

        // ID to Full
        wrkLBA = ODIDStep;
        for( wrkLBA2 = ODIDStep; wrkLBA2 < ( endLBA - ODIDStep ); wrkLBA2 += ODIDStep )
        {
          if( flipCoin() )
          {
            DO_READ( getOffsetLBA( wrkLBA ) )
            DO_READ( getOffsetLBA( wrkLBA2 ) )
          }
          else
          {
            DO_WRITE( getOffsetLBA( wrkLBA ) )
            DO_WRITE( getOffsetLBA( wrkLBA2 ) )
          }
        }

        // Full
        wrkLBA2 = endLBA - ODIDStep;
        for( i = 0; i < FullODIDCount; i++ )
        {
          if( flipCoin() )
          {
            DO_READ( getOffsetLBA( wrkLBA ) )
            DO_READ( getOffsetLBA( wrkLBA2 ) )
          }
          else
          {
            DO_WRITE( getOffsetLBA( wrkLBA ) )
            DO_WRITE( getOffsetLBA( wrkLBA2 ) )
          }
        }

        // FUll to ID
        for( wrkLBA = ODIDStep; wrkLBA < ( endLBA - ODIDStep ); wrkLBA += ODIDStep )
        {
          if( flipCoin() )
          {
            DO_READ( getOffsetLBA( wrkLBA ) )
            DO_READ( getOffsetLBA( wrkLBA2 ) )
          }
          else
          {
            DO_WRITE( getOffsetLBA( wrkLBA ) )
            DO_WRITE( getOffsetLBA( wrkLBA2 ) )
          }
        }
      }
    }
  }
  ec = 0;
out:
  free( readBuff );
  free( writeBuff );

  closeDisk( &drive );

  time( &endTime );

  if( verbose )
  {
    printf( "Did %u reads and %u writes\ntotal IO ops:%u in %li seconds\n", rCount, wCount, ( rCount + wCount ), ( endTime - startTime ) );
    printf( "IO ops/s: %f\n", ( (float) iopsPerRound * (float) roundCount ) / (float) ( endTime - startTime ) );
  }

  exit( ec );


err:
  closeDisk( &drive );

  exit( 1 );
}
