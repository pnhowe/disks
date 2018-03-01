/* derived from sgio.c in hdparm - by Mark Lord (C) 2007 -- freely distributable */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "cdb.h"
#include "sgio.h"

#include "scsi.h" // for scsi code util functions

#include <linux/hdreg.h>

extern int verbose;

#ifndef SG_IO
#error SG_IO is required!! problems with scsi/sg.h ?
#endif

// http://www.tldp.org/HOWTO/SCSI-Generic-HOWTO/index.html
int sgio_cmd( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, const unsigned int timeout )
{
  int rc;
  unsigned char sb[32];
  struct sg_io_hdr io_hdr;

  memset( &sb,     0, sizeof( sb ) );
  memset( &io_hdr, 0, sizeof( struct sg_io_hdr) );

  io_hdr.interface_id	= 'S';
  io_hdr.cmd_len = cdb_len;
  io_hdr.mx_sb_len	= sizeof( sb );
  io_hdr.dxfer_direction	= data ? ( ( rw == RW_WRITE ) ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV) : SG_DXFER_NONE;
  io_hdr.dxfer_len	= data ? data_len : 0;
  io_hdr.dxferp  = data;
  io_hdr.cmdp    = cdb;
  io_hdr.sbp     = sb;
  io_hdr.pack_id	= 0;
  io_hdr.timeout = timeout * 1000; /* msecs */

  if( ( verbose >= 4 ) && ( rw == RW_WRITE ) )
    dump_bytes( "SGIO: send", data, data_len );

  rc = ioctl( drive->fd, SG_IO, &io_hdr );
  if( verbose >= 3 )
    fprintf( stderr, "SGIO: ioctl returnd: %i, errno: %i\n", rc, errno );

  if( rc == -1 )
  {
    if( verbose >= 2 )
      fprintf( stderr, "SGIO: ioctl Failed, errno: %i\n", errno );
    return -1;
  }

  // see: http://tldp.org/HOWTO/SCSI-Generic-HOWTO/x364.html
  // see: http://oss.sgi.com/LDP/HOWTO/SCSI-Programming-HOWTO-21.html
  if( verbose >= 3 )
    fprintf( stderr, "SGIO: len=%u info=0x%x, status=0x%x, host_status=0x%x, driver_status=0x%x\n", io_hdr.cmd_len, io_hdr.info, io_hdr.status, io_hdr.host_status, io_hdr.driver_status );

  if( io_hdr.info | SG_INFO_CHECK ) // TODO: this dosen't make any sense this will always be true, should an INFO_CHECK allways be an error?
  {
    if( verbose >= 3 )
      fprintf( stderr, "SGIO: Info Check\n" );

    if( io_hdr.status && ( io_hdr.status != SG_CHECK_CONDITION ) )
    {
      if( verbose >= 2 )
        fprintf( stderr, "SGIO: bad status: 0x%x\n", io_hdr.status );
      errno = EBADE;
      return -1;
    }

    if( io_hdr.host_status )
    {
      if( verbose >= 2 )
        fprintf( stderr, "SGIO: bad host status: 0x%x\n", io_hdr.host_status );

      if( ( io_hdr.host_status == SG_ERR_DID_NO_CONNECT ) || ( io_hdr.host_status == SG_ERR_DID_BUS_BUSY ) || ( io_hdr.host_status == SG_ERR_DID_TIME_OUT ) )
      {
        if( verbose >= 2 )
          fprintf( stderr, "SGIO: host timeout\n");
        errno = ETIMEDOUT;
        return -1;
      }

      if( io_hdr.host_status == SG_ERR_DID_RESET )
      {
        if( verbose >= 2 )
          fprintf( stderr, "SGIO: bus/device reset\n");
        errno = ECONNRESET;
        return -1;
      }

      errno = EBADE;
      return -1;
    }

    if( io_hdr.driver_status )
    {
      if( io_hdr.driver_status == SG_ERR_DRIVER_TIMEOUT )
      {
        if( verbose >= 2 )
          fprintf( stderr, "SGIO: driver timeout\n" );
        errno = ECONNRESET;
        return -1;
      }
      else if( io_hdr.driver_status != SG_ERR_DRIVER_SENSE )
      {
        if( verbose >= 2 )
          fprintf( stderr, "SGIO: bad driver status: 0x%x\n", io_hdr.driver_status );
        errno = EBADE;
        return -1;
      }
    }
  }

  if( verbose >= 4 )
    dump_bytes( "SGIO: sb[]", sb, sizeof( sb ) );

  if( verbose >= 3 )
    print_sense( "SGIO", sb );

  if( ( verbose >= 4 ) && ( rw == RW_READ ) )
    dump_bytes( "SGIO: received", data, data_len );

  // sense buffer : http://oss.sgi.com/LDP/HOWTO/SCSI-Programming-HOWTO-10.html
  // 0x72 -> Descriptior length = 14 (0x0e)      NOTE: 0x72/73  is descriotor format, 70/71 is fixed format
  // 0x09 -> ATA Descriptior length = 12 (0x0c)  NOTE: 0x09 -> ATA Return ... see 2.4 of SCSI Spec
  if( ( ( sb[0] & 0x7f ) == 0x72 ) || ( sb[7] == 0x0e ) )
    memcpy( cdb, sb + 8, sb[7] );
  else
    memset( cdb, 0, cdb_len );

  if( ( ( sb[0] == 0x70 ) || ( sb[0] == 0x71 ) ) && ( ( ( sb[2] & 0x0f ) != 0x00 ) && ( ( sb[2] & 0x0f ) != 0x01 ) ) )
  {
    errno = EBADE;
    return -1;
  }

  return 0;
}

void sgio_close( struct device_handle *drive )
{
  close( drive->fd );
  drive->fd = -1;
}

void sgio_info_mask( __attribute__((unused)) struct drive_info *info )
{

}

int sgio_open( const char *device, struct device_handle *drive )
{
  struct stat statbuf;

  if( stat( device, &statbuf ) )
  {
    if( verbose )
      fprintf( stderr, "Unable to stat file '%s', errno: %i\n", device, errno );
    return -1;
  }

  if( device[6] == 'd' && !S_ISBLK( statbuf.st_mode ) )
  {
    if( verbose )
      fprintf( stderr, "'%s' Is not a block device\n", device );
    errno = EBADF;
    return -1;
  }
  else if( device[6] == 'g' && !S_ISCHR( statbuf.st_mode ) )
  {
    if( verbose )
      fprintf( stderr, "'%s' Is not a char device\n", device );
    errno = EBADF;
    return -1;
  }

  drive->fd = open( device, O_RDONLY | O_NONBLOCK );

  if( drive->fd == -1 )
  {
    if( verbose )
      fprintf( stderr, "Error opening device '%s', errno: %i\n", device, errno );
    return -1;
  }

  drive->driver_cmd = &sgio_cmd;
  drive->close = &sgio_close;
  drive->driver = DRIVER_TYPE_SGIO;
  drive->info_mask = &sgio_info_mask;

  return 0;
}

int sgio_isvalidname( const char *device )
{
  if( !strncmp( device, "/dev/sg", 7 ) )
    return 1;

  if( !strncmp( device, "/dev/sd", 7 ) )
    return 1;

  return 0;
}

char *sgio_validnames( void )
{
  return "'/dev/sda', '/dev/sg0'";
}
