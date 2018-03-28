#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "disk.h"

int verbose;
int batchMode;
int timerTrip;

static char short_opts[] = "vqh?ds:e:c:b";
static const struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v' },
  { "quiet", no_argument, NULL, 'q' },
  { "help", no_argument, NULL, 'h' },
  { "dis-cache", no_argument, NULL, 'd' },
  { "start", required_argument, NULL, 's' },
  { "end", required_argument, NULL, 'e' },
  { "count", required_argument, NULL, 'c' },
  { "batch", no_argument, NULL, 'b' },
  { NULL, 0, NULL, 0 }
};

static char *usage_txt =
"%s version " PROG_VERSION "\n\
Usage: [OPTIONS] source target\n\
Options:\n\
  -v,  --verbose   Verbose, more -v more stuff.(4 levles, starts on level 1)\n\
  -q,  --quiet     Less Verbose, more -q less stuff.\n\
  -h,-?, --help    Show this\n\
  -d, --dis-cache  Disable Write Cache on target drive ( Default is to enable it )\n\
  -s, --start      Start LBA ( Default:0 )\n\
  -e, --end        End LBA ( Default:0, 0=Entire Disk ) \n\
  -c, --count      Number of Sectors to copy at a time on first pass ( Default:64 )\n";

#define COPY( _SOURCE_, _TARGET_, _LBA_, _COUNT_, _BUFF_, _LIST_ )\
if( readLBA( _SOURCE_, 1, 0, _LBA_, _COUNT_, _BUFF_ ) ) \
{ \
  errcnt++; \
  add( _LIST_, _LBA_, _COUNT_ ); \
  continue; \
} \
if( writeLBA( _TARGET_, 1, 0, _LBA_, _COUNT_, _BUFF_ ) ) \
{ \
  fprintf( stderr, "Error Writing at %llu, errno: %i.\n", wrkLBA, errno ); \
  rc = 1; \
  break; \
}

void SIGALRMsignalHandler( __attribute__((unused)) int sig )
{
  timerTrip = 1;
}

typedef struct block
{
  __u64 LBA;
  __u16 count;
  struct block *next;
} block_t;

void add( block_t **head, __u64 LBA, __u16 count )
{
  block_t *tmp;
  block_t *new;

  if( verbose )
    printf( "*** Adding bad at %llu for %u ***\n", LBA, count );

  if( verbose == 1 )
    printf( "\n" );

  new = malloc( sizeof( block_t ) );
  new->LBA = LBA;
  new->count = count;
  new->next = NULL;

  if( *head == NULL )
  {
    *head = new;
    return;
  }

  tmp = *head;
  while( tmp->next != NULL )
    tmp = tmp->next;

  tmp->next = new;
}

void clean( block_t **head )
{
  block_t *tmp1, *tmp2;

  if( *head == NULL )
    return;

  tmp1 = ( *head )->next;
  free( *head );
  while( tmp1 != NULL )
  {
    tmp2 = tmp1;
    tmp1 = tmp1->next;
    free( tmp2 );
  }

  *head = NULL;
}

int doCopy( struct device_handle *source, struct device_handle *target, __u16 count, __u64 start, __u64 end )
{
  int rc = 0;
  int i;
  __u8 *wrkBuff;
  __u64 wrkLBA;
  time_t startTime;
  time_t curTime;
  float perComplete;
  int secRemaining;
  block_t *missed = NULL;
  block_t *failed = NULL;
  block_t *iter;
  int errcnt = 0;
  int wrkcnt = 0;
  int wrkcnt2 = 0;

  time( &startTime );

  setTimeout( 1 );

  // output before doing anything to let everything know that we are at least starting the copy
  if( batchMode )
  {
    printf( "Copy: %.2f %u %.2f %.3f   \n", 0.0, 0, 0.0, 0.0 );
    fflush( stdout );
  }
  if( verbose == 1 )  // otherwise we messup the output of the other levels of verbosity
    printf( "\x1b[1A\x1b[1A" );
  if( verbose )
    printf( "%s  %.3f%% Complete, %u skipped, %.3f Mb/s, %.3f hours remaining.   \n", ctime( &curTime ), 0.0, 0, 0.0, 0.0 ); // ctime appends a \n to it's return.

  wrkBuff = memalign( sysconf( _SC_PAGESIZE ), count * 512 );
  timerTrip = 0;
  alarm( 5 );
  for( wrkLBA = start; ( wrkLBA + count ) < end; wrkLBA += count )
  {
    COPY( source, target, wrkLBA, count, wrkBuff, &missed )

    time( &curTime );
    if( timerTrip ) // output before doing anything to let everything know that we are at least starting the copy
    {
      timerTrip = 0;
      alarm( 5 );
      perComplete = ( (float) ( wrkLBA - start ) / (float) ( end - start ) );
      secRemaining = ( (float) ( curTime - startTime ) / perComplete ) * ( 1.0 - perComplete );
      if( batchMode )
      {
        printf( "Copy: %.2f %u %.2f %.3f   \n", perComplete * 100, errcnt, ( ( (float) ( wrkLBA - start ) * 512.0 ) / ( (float) ( curTime - startTime ) ) ) / 1052672, (float) secRemaining / 3600.0 );
        fflush( stdout );
      }
      if( verbose == 1 )  // otherwise we messup the output of the other levels of verbosity
        printf( "\x1b[1A\x1b[1A" );
      if( verbose )
        printf( "%s  %.3f%% Complete, %u skipped, %.3f Mb/s, %.3f hours remaining.   \n", ctime( &curTime ), perComplete * 100, errcnt, ( ( (float) ( wrkLBA - start ) * 512.0 ) / ( (float) ( curTime - startTime ) ) ) / 1052672, (float) secRemaining / 3600.0 ); // ctime appends a \n to it's return.
    }
  }

  if( rc != 0 )
  {
    free( wrkBuff );
    clean( &missed );
    return rc;
  }

  free( wrkBuff );

  if( verbose )
    printf( "Copying remaining sectors... %llu \n", wrkLBA );
  wrkLBA -= count;
  count = end - wrkLBA;
  if( verbose )
    printf( "%llu  for %u to %llu \n", wrkLBA, count, end );

  wrkBuff = memalign( sysconf( _SC_PAGESIZE ), 512 * count );
  for( i = 0; i < 1; i++ ) // the COPY Macro needs to be in a loop
  {
    COPY( source, target, wrkLBA, count, wrkBuff, &missed )
  }

  if( rc != 0 )
  {
    free( wrkBuff );
    clean( &missed );
    return rc;
  }

  if( missed == NULL )
  {
    free( wrkBuff );
    if( verbose )
      printf( "Pass one had no problems\n" );
    return 0;
  }

  count = 1;
  setTimeout( 5 );

  free( wrkBuff );
  wrkBuff = memalign( sysconf( _SC_PAGESIZE ), 512 * count );

  iter = missed;
  wrkcnt = errcnt;
  errcnt = 0;
  wrkcnt2 = 0;
  if( verbose )
    printf( "\n\n\n" );

  while( iter != NULL )
  {
    wrkcnt2++;
    if( batchMode )
    {
      printf( "Retry: %u %u %u\n", wrkcnt2, wrkcnt, errcnt );
      fflush( stdout );
    }
    if( verbose )
    {
      time( &curTime );
      if( verbose == 1 )  // otherwise we messup the output of the other levels of verbosity
        printf( "\x1b[1A\x1b[1A" );
      printf( "%s  Retrying %u Block at LBA %llu ( %u of %u ), %u skipped blocks      \n", ctime( &curTime ), iter->count, iter->LBA, wrkcnt2, wrkcnt, errcnt );
    }
    for( wrkLBA = iter->LBA; wrkLBA < ( iter->LBA + iter->count ); wrkLBA++ )
    {
      COPY( source, target, wrkLBA, count, wrkBuff, &failed )
    }
    iter = iter->next;
  }

  if( rc != 0 )
  {
    free( wrkBuff );
    clean( &missed );
    clean( &failed );
    return rc;
  }

  free( wrkBuff );

  clean( &missed );

  if( batchMode )
  {
    wrkcnt = 0;
    iter = failed;
    while( iter != NULL )
    {
      wrkcnt++;
      iter = iter->next;
    }
    printf( "Failed: %u\n", wrkcnt );
    fflush( stdout );
  }
  else if( verbose )
  {
    wrkcnt = 0;
    iter = failed;
    printf( "LBAs that didn't copy:\n" );
    while( iter != NULL )
    {
      printf( "  %llu\n", iter->LBA );
      wrkcnt++;
      iter = iter->next;
    }
    printf( "Total: %u\n", wrkcnt );
  }

  clean( &failed );

  return 0;
}



int main( int argc, char **argv )
{
  // paramaters
  char *source;
  char *target;
  int disable_cache;

  // workVars
  struct device_handle drive_source;
  struct device_handle drive_target;
  struct drive_info *info_source;
  struct drive_info *info_target;
  __u64 totalLBACount;
  __u64 wrkLBA;
  __u64 start = 0;
  __u64 end = 0;
  __u16 count = 64;
  char c;

  verbose = 1;
  batchMode = 0;

  printf( "WARNING!!!! this code has not yet been updated to deal with non 512 size Logical Sectors\n" );
  // TODO: update for non 512 byte sized logical sectors, some fancy coding might need to be done to copy from one sized logical sector to the other


  if( argc < 3 )
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

      case 'd':
        disable_cache = 1;
        break;

      case 's':
        start = atoll( optarg );
        break;
      case 'e':
        end = atoll( optarg );
        break;
      case 'c':
        count = atol( optarg );
        break;

      case 'b':
        batchMode = 1;
        break;

      case '?':
      case 'h':
      default:
        fprintf( stderr, usage_txt, argv[0] );
        exit( 1 );
    }
  }

  if( ( optind + 1 ) >= argc )
  {
    fprintf( stderr, usage_txt, argv[0] );
    exit( 1 );
  }

  source = argv[ optind ];
  target = argv[ optind + 1 ];

  if( batchMode )
    verbose = 0;

  if( getuid() != 0 )
  {
    fprintf( stderr, "Must be root to run this.\n" );
    exit( 1 );
  }

  if( openDisk( source, &drive_source, DRIVER_TYPE_UNKNOWN, PROTOCOL_TYPE_UNKNOWN, IDENT ) )
  {
    fprintf( stderr, "Error opening device '%s', errno: %i.\n", source, errno );
    exit( 1 );
  }
  if( openDisk( target, &drive_target, DRIVER_TYPE_UNKNOWN, PROTOCOL_TYPE_UNKNOWN, IDENT ) )
  {
    fprintf( stderr, "Error opening device '%s', errno: %i.\n", target, errno );
    exit( 1 );
  }

  info_source = getDriveInfo( &drive_source );
  info_target = getDriveInfo( &drive_target );

  if( !info_source->supportsLBA  )
  {
    fprintf( stderr, "Device '%s' Dosen't support LBAs.\n", source );
    exit( 1 );
  }

  if( !info_target->supportsLBA )
  {
    fprintf( stderr, "Device '%s' Dosen't support LBAs.\n", target );
    exit( 1 );
  }

  if( verbose )
    printf( "Copying '%s' to '%s'\n", source, target );

  if( info_source->bit48LBA != info_target->bit48LBA )
  {
    fprintf( stderr, "Error: One Disk uses Extended (48-bit) commands and the other does not (28-bit).\n" );
    goto err;
  }

  if( info_source->protocol != info_target->protocol )
  {
    fprintf( stderr, "Error: The Disks are not the same protocol.\n" );
    goto err;
  }

  totalLBACount = info_source->LBACount;
  if( !totalLBACount )
  {
    fprintf( stderr, "Error getting LBACount from source\n" );
    goto err;
  }

  wrkLBA = info_target->LBACount;
  if( !wrkLBA )
  {
    fprintf( stderr, "Error getting LBACount from target\n" );
    goto err;
  }

  if( totalLBACount != wrkLBA )
  {
    totalLBACount = (( totalLBACount < wrkLBA ) ? totalLBACount : wrkLBA );
    printf( "WARNING: drives not sames size, going with the smallest of %2fG\n", ( ( (double) totalLBACount * 512.0 ) / 1073741824.0 ) ); //#TODO: should be caculated on size not LBA, and use the drive->enclosure LBA size
  }

  if( end != 0 )
    totalLBACount = (( totalLBACount < end ) ? totalLBACount : end );

  if( disable_cache )
  {
    if( disableWriteCache( &drive_target ) )
    {
      fprintf( stderr, "Error Disabeling Write Cache, errno %i.\n", errno );
      goto err;
    }
  }
  else
  {
    if( enableWriteCache( &drive_target ) )
    {
      fprintf( stderr, "Error Enabeling Write Cache, errno %i.\n", errno );
      goto err;
    }
  }

  if( !batchMode )
    printf( "Copying from %s to %s from %llu to %llu in chunks of %i LBAs\n\n\n", source, target, start, totalLBACount, count );

  signal( SIGALRM, SIGALRMsignalHandler );

  if( doCopy( &drive_source, &drive_target, count, start, totalLBACount ) )
  {
    fprintf( stderr, "Error Doing Copy\n" );
    goto err;
  }

  closeDisk( &drive_source );
  closeDisk( &drive_target );

  exit( 0 );

err:
  closeDisk( &drive_source );
  closeDisk( &drive_target );

  exit( 1 );
}
