#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <sys/epoll.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
//#include <sys/types.h>

int cont;

// follows the same rules as a tty from the cmd line
//#define DEBUG_TTY "/tmp/ttydebug"
//#define DEBUG_TTY "/dev/tty4"

#ifdef DEBUG_TTY
FILE *debug_fd;
#endif

#define DEBUG_COL_WIDTH 16

void sig_handle( int __attribute__((unused)) signo )
{
#ifdef DEBUG_TTY
  fprintf( debug_fd, "---- stop signal recieved\r\n" );
#endif
  cont = 0;
}

int check_create_tty( char *filename )
{
  int fd;
  struct stat sb;

  if( strncmp( filename, "-", 1 ) == 0 )
    return 1;

  fd = 0;

  if( stat( filename, &sb ) )
  {
    if( errno == ENOENT )
    {
      fd = open( filename, O_RDWR | O_CREAT, 0644 );
      if( fd == -1 )
      {
        fprintf( stderr, "Error creating '%s', errno: %i\n", filename, errno );
        return 0;
      }
    }
    else
    {
      fprintf( stderr, "Error statting '%s', errno: %i\n", filename, errno );
      return 0;
    }
  }
  else
  {
    if( ( ( sb.st_mode & S_IFMT ) != S_IFREG ) && ( ( ( sb.st_mode & S_IFMT ) != S_IFCHR ) ) )
    {
      fprintf( stderr, "Error '%s', is not a Char Device nor a Regular file.\n", filename );
      return 0;
    }

    fd = open( filename, O_RDWR );
    if( fd == -1 )
    {
      fprintf( stderr, "Error opening '%s', errno: %i\n", filename, errno );
      return 0;
    }
  }

  if( fd )
    close( fd );

  return 1;
}

int open_tty( char *filename )
{
  int flags;
  int fd;
  struct termios ttyinfo;

  if( strncmp( filename, "-", 1 ) == 0 )
    return 1;

  fd = open( filename, O_RDWR );
  if( fd == -1 )
  {
    fprintf( stderr, "Error opening '%s', errno: %i\n", filename, errno );
    return -1;
  }

  if( isatty( fd ) )
  {
    if( tcgetattr( fd, &ttyinfo ) )
    {
      fprintf( stderr, "Error getting tc attr, errno: %i\n", errno );
      return -1;
    }
    ttyinfo.c_iflag &= ~( BRKINT | ICRNL | ISTRIP );
    ttyinfo.c_oflag &= ~( OPOST );
    ttyinfo.c_lflag &= ~( ECHO | ICANON | IEXTEN | ISIG );

    ttyinfo.c_cc[VMIN] = 0; ttyinfo.c_cc[VTIME] = 0;
    if( tcsetattr( fd, TCSAFLUSH, &ttyinfo ) )
    {
      fprintf( stderr, "Error setting tc attr, errno: %i\n", errno );
      return -1;
    }
  }
  else
  {
    flags = fcntl( fd, F_GETFL, 0 );
    if (flags == -1)
    {
      fprintf( stderr, "Error getting Flags, errno: %i\n", errno );
      return -1;
    }

    flags |= O_NONBLOCK;
    if( fcntl( fd, F_SETFL, flags ) )
    {
      fprintf( stderr, "Error setting Flags, errno: %i\n", errno );
      return -1;
    }
  }

  return fd;
}

int daemonize( void )
{
  int rc;
  int __attribute__((unused)) fd;

  rc = fork();
  if( rc < 0 )
  {
    fprintf( stderr, "Failed first fork, errno: %i\n", errno );
    return -1;
  }
  else if( rc > 0 )
  {
    exit( 0 );
  }

  if( setsid() < 0 )
  {
    fprintf( stderr, "setsid failed, errno: %i\n", errno );
    return -1;
  }

  signal( SIGCHLD, SIG_IGN );
  signal( SIGHUP, SIG_IGN );

  rc = fork();
  if( rc < 0 )
  {
    fprintf( stderr, "Failed second fork, errno: %i\n", errno );
    return -1;
  }
  else if( rc > 0 )
  {
    exit( 0 );
  }

  if( chdir( "/" ) )
  {
    fprintf( stderr, "Failed to change dir, errno: %i\n", errno );
    return -1;
  }

  close( STDIN_FILENO );
  close( STDOUT_FILENO );
  close( STDERR_FILENO );
  fd = open( "/dev/null", O_RDWR );
  fd = dup( 0 );
  fd = dup( 0 );
  return 0;
}

int main( int argc, char *argv[] )
{
  char *source_name;
  int *ttys = NULL;
  char **tty_names;
  int tty_count;
  int i, j;
  int byte_count;
  int epoll_count;
  int efd;
  struct epoll_event event;
  struct epoll_event *events = NULL;
  char wrk_buff[512];
  struct stat sb;
#ifdef DEBUG_TTY
  int k, l;
#endif

  if( argc < 2 )
  {
    fprintf( stderr, "Usage: %s [-d] <desired shared name> <first tty> [<second tty>...]\n", argv[0] );
    fprintf( stderr, " -d -> daemonize\n" );
    fprintf( stderr, " a tty may be '-' which will output to stdout, read only, can't be used with -d\n" );
    exit( 1 );
  }

  if( strncmp( argv[1],  "-d", 2 ) == 0 )
  {
    tty_names = &argv[2];
    tty_count = argc - 2;
  }
  else
  {
    tty_names = &argv[1];
    tty_count = argc - 1;
  }

  for( i = 1; i < tty_count; i++ )
  {
    if( !check_create_tty( tty_names[i] ) )
    {
      fprintf( stderr, "Error checking '%s'\n", tty_names[i] );
      exit( 1 );
    }
  }

  if( strncmp( argv[1],  "-d", 2 ) == 0 )
  {
    if( daemonize() )
    {
      fprintf( stderr, "Failed to Daemonize\n" );
      exit( 1 );
    }
  }

#ifdef DEBUG_TTY
  debug_fd = fdopen( open_tty( DEBUG_TTY ), "w" );
#endif

  cont = 1;

  signal( SIGTERM, sig_handle );
  signal( SIGINT, sig_handle );

  if( access( tty_names[0], F_OK ) == 0 )
  {
    fprintf( stderr, "File '%s' allready exists\n", tty_names[0] );
    exit( 1 );
  }

  ttys = (int*) malloc( sizeof( int ) * tty_count );
  if( ttys == NULL )
  {
    fprintf( stderr, "Failed to allocate meory for ttys, errno: %i\n", errno );
    goto error;
  }

  ttys[0] = open_tty( "/dev/ptmx" );
  if( ttys[0] == -1 )
  {
    fprintf( stderr, "Error opening /dev/ptmx, errno: %i\n", errno );
    exit( 1 );
  }

  source_name = (char*) ptsname( ttys[0] );
  if( !source_name )
  {
    fprintf( stderr, "Error getting pts name, source: %i, errno: %i\n", ttys[0], errno );
    exit( 1 );
  }

  if( grantpt( ttys[0] ) )
  {
    fprintf( stderr, "Error with grantpt, source: %i, errno: %i\n", ttys[0], errno );
    exit( 1 );
  }
  if( unlockpt( ttys[0] ) )
  {
    fprintf( stderr, "Error with grantpt, source: %i, errno: %i\n", ttys[0], errno );
    exit( 1 );
  }

  if( symlink( source_name, tty_names[0] ) )
  {
    fprintf( stderr, "Error creating symlink from '%s' to '%s', errno: %i\n", source_name, tty_names[0], errno );
    exit( 1 );
  }
  if( chmod( source_name, 00777 ) )
  {
    fprintf( stderr, "Error chmod on '%s', errno: %i\n", source_name, errno );
    goto error;
  }

#ifdef DEBUG_TTY
  fprintf( debug_fd, "pts: %s linked to %s, fd: %i\r\n", source_name, tty_names[0], ttys[0] );
#endif

  efd = epoll_create( 1 );
  if( efd == -1 )
  {
    fprintf( stderr, "epoll_create failed, errno: %i\n", errno );
    goto error;
  }

  event.data.u32 = 0;
  event.events = EPOLLIN;
  if( epoll_ctl( efd, EPOLL_CTL_ADD, ttys[0], &event ) )
  {
    fprintf( stderr, "failed to add source to epoll, errno: %i\n", errno );
    goto error;
  }

  for( i = 1; i < tty_count; i++ )
  {
    ttys[i] = open_tty( tty_names[i] );
    if( ttys[i] == -1 )
    {
      fprintf( stderr, "Error opening '%s'\n", tty_names[i] );
      goto error;
    }
#ifdef DEBUG_TTY
    fprintf( debug_fd, "target: %s opend as fd: %i\r\n", tty_names[i], ttys[i] );
#endif

    if( fstat( ttys[i], &sb ) )
    {
      fprintf( stderr, "Error statting fd %i target '%s', errno: %i\n", ttys[i], tty_names[i], errno );
      return -1;
    }
    if( ( sb.st_mode & S_IFMT ) == S_IFCHR )
    {
      event.data.u32 = i;
      event.events = EPOLLIN;
      if( epoll_ctl( efd, EPOLL_CTL_ADD, ttys[i], &event ) )
      {
        fprintf( stderr, "failed to add '%s' to epoll, errno: %i\n", tty_names[i], errno );
        goto error;
      }
#ifdef DEBUG_TTY
    fprintf( debug_fd, "target: %s event added\r\n", tty_names[i] );
#endif
    }
  }

  events = calloc( tty_count, sizeof( event ) );

#ifdef DEBUG_TTY
    fprintf( debug_fd, "Waiting for input...\r\n" );
#endif

  while( cont )
  {
    epoll_count = epoll_wait( efd, events, tty_count, -1 );
    if( epoll_count < 0 )
    {
      fprintf( stderr, "Error with epoll, errno: %i\n", errno );
      goto error;
    }
    if( epoll_count == 0 )
      continue;

#ifdef DEBUG_TTY
    fprintf( debug_fd, "epoll_count: %i\r\n", epoll_count );
#endif

    for( i = 0; i < epoll_count; i++ )
    {
      if( ( events[i].events & EPOLLERR ) || ( events[i].events & EPOLLHUP ) )
      {
#ifdef DEBUG_TTY
        fprintf( debug_fd, "error with data: %i fd: %i name: %s\r\n", events[i].data.u32, ttys[ events[i].data.u32 ], tty_names[ events[i].data.u32 ] );
#endif
        if( events[i].data.u32 == 0 )
          fprintf( stderr, "Error polling source\n" );
        else
          fprintf( stderr, "Error polling '%s'\n", tty_names[ events[i].data.u32 ] );
        cont = 0;
        continue;
      }

      if( events[i].events & EPOLLIN )
      {
        byte_count = read( ttys[events[i].data.u32], wrk_buff, sizeof( wrk_buff ) );
        if( byte_count < 1 )
          continue;
#ifdef DEBUG_TTY
        fprintf( debug_fd, " Read %i from %i\r\n", byte_count, ttys[events[i].data.u32] );
        for( k = 0; k < byte_count; k += DEBUG_COL_WIDTH )
        {
          fprintf( debug_fd, "%i:\t", k );
          for( l = 0; ( l < DEBUG_COL_WIDTH ) && ( k + l < byte_count ); l++ )
          {
            if( ( wrk_buff[k + l] < 32 ) || ( wrk_buff[k + l] > 126 ) )
              fprintf( debug_fd, "." );
            else
              fprintf( debug_fd, "%c", wrk_buff[k + l] );
          }

          for( ; l < DEBUG_COL_WIDTH; l++ )
            fprintf( debug_fd, " " );
          fprintf( debug_fd, " - " );

          for( l = 0; ( l < DEBUG_COL_WIDTH ) && ( k + l < byte_count ); l++ )
            fprintf( debug_fd, " %02x", wrk_buff[k + l] );
          fprintf( debug_fd, "\r\n" );
        }
#endif
        if( events[i].data.u32 == 0 )
        {
          for( j = 1; j < tty_count; j++ )
          {
            if( write( ttys[j], wrk_buff, byte_count ) != byte_count )
            {
#ifdef DEBUG_TTY
              fprintf( debug_fd, "Error writting all data to '%s'\r\n", tty_names[j] );
#endif
              fprintf( stderr, "Error writting all data to '%s'\n", tty_names[j] );
              cont = 0;
              continue;
            }
#ifdef DEBUG_TTY
            fprintf( debug_fd, " Wrote %i to %i\r\n", byte_count, ttys[j] );
#endif
          }
        }
        else
        {
          if( write( ttys[0], wrk_buff, byte_count ) != byte_count )
          {
#ifdef DEBUG_TTY
              fprintf( debug_fd, "Error writting all data to source\r\n" );
#endif
              fprintf( stderr, "Error writting all data to source\n" );
              cont = 0;
              continue;
          }
#ifdef DEBUG_TTY
          fprintf( debug_fd, " Wrote %i to %i\r\n", byte_count, ttys[0] );
#endif
        }
#ifdef DEBUG_TTY
        fflush( debug_fd );
#endif
      }
    }
  }

#ifdef DEBUG_TTY
  fprintf( debug_fd, "cleaning up...\r\n" );
#endif

  unlink( tty_names[0] );
  free( ttys );
  free( events );

#ifdef DEBUG_TTY
  fprintf( debug_fd, "done!\r\n" );
#endif

#ifdef DEBUG_TTY
  fclose( debug_fd );
#endif
  exit( 0 );

error:
#ifdef DEBUG_TTY
  if( debug_fd )
    fprintf( debug_fd, "error!\r\n" );
#endif
  if( ttys != NULL )
    free( ttys );
  if( events != NULL )
    free( events );

  unlink( tty_names[0] );

#ifdef DEBUG_TTY
  if( debug_fd )
    fclose( debug_fd );
#endif
  exit( 1 );
}
