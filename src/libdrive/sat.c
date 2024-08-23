/* parts taken from  scsiata.cpp of smartmontools and sgio.c */
/* modified by Peter Howe pnhowe@gmail.com */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <scsi/scsi.h>
#include <sys/ioctl.h>
#include <scsi/scsi_ioctl.h>

#include "cdb.h"
#include "sat.h"

#include "scsi.h" // for scsi code util functions

extern int verbose;

#ifndef SCSI_IOCTL_SEND_COMMAND
#error SCSI_IOCTL_SEND_COMMAND is required!! problems with scsi/scsi_ioctl.h ?
#endif

int sat_cmd( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, __attribute__((unused)) const unsigned int timeout )
{
  int rc;
  struct linux_ioctl_send_command cmd;

  if( data_len > MAX_DXFER_LEN )
  {
    errno = EINVAL;
    return -1;
  }

  memset( &cmd, 0, sizeof( struct linux_ioctl_send_command ) );

  memcpy( cmd.cdb, cdb, cdb_len );

  if( ( rw == RW_READ ) && data )
  {
    cmd.inbufsize = data_len;
    cmd.outbufsize = 0;
  }
  else if( ( rw == RW_WRITE ) && data )
  {
    memcpy( cmd.data, data, data_len );
    cmd.inbufsize = 0;
    cmd.outbufsize = data_len;
  }
  else
  {
    cmd.inbufsize = 0;
    cmd.outbufsize = 0;
  }

  if( verbose >= 4 )
    fprintf( stderr, "SAT: buff size: in: %i, out: %i\n", cmd.inbufsize, cmd.outbufsize );

  if( ( verbose >= 4 ) && ( rw == RW_WRITE ) )
    dump_bytes( "SAT: send", cmd.data, sizeof( cmd.data ) );

  rc = ioctl( drive->fd, SCSI_IOCTL_SEND_COMMAND, &cmd );

  fprintf( stderr, "SAT: ioctl returnd: %i, errno: %i\n", rc, errno ); // temporary

  /*
  rc = 0 -> succeeds
  rc < 0 -> unix error
  rc > 0 -> compacted scsi error code, lowest byte is SCSI status ( see drivers/scsi/scsi.h )
  */

  if( rc < 0 )
  {
    if( verbose >= 2 )
      fprintf( stderr, "SAT: ioctl Failed, rc: %i, errno: %i\n", rc, errno );
    return -1;
  }

  if( verbose >= 4 )
    dump_bytes( "SAT: sense", cmd.cdb, sizeof( cmd.cdb ) );

  if( ( verbose >= 3 ) && ( cmd.cdb[0] == 0x70 || cmd.cdb[0] == 0x71 ) )
    fprintf( stderr, "SAT: sence key=0x%02x asc=0x%02x ascq=0x%02x\n", cmd.cdb[2] & 0x0f, cmd.cdb[12], cmd.cdb[13] ); // see http://oss.sgi.com/LDP/HOWTO/SCSI-Programming-HOWTO-22.html#sec-sensecodes

  if( verbose >= 2 )
    fprintf( stderr, "SAT: scsi error, rc: %i scsi_status=0x%x\n", rc, ( rc & STATUS_MASK ) );

  if( ( verbose >= 4 ) && ( rw == RW_READ ) )
    dump_bytes( "SAT: received", cmd.data, sizeof( cmd.data ) );

  memcpy( data, cmd.data, data_len );

/*
  tf->error     = cmd.cdb[ 1];
  tf->hob.nsect = cmd.cdb[ 2];
  tf->lob.nsect = cmd.cdb[ 3];
  tf->hob.lbal  = cmd.cdb[ 4];
  tf->lob.lbal  = cmd.cdb[ 5];
  tf->hob.lbam  = cmd.cdb[ 6];
  tf->lob.lbam  = cmd.cdb[ 7];
  tf->hob.lbah  = cmd.cdb[ 8];
  tf->lob.lbah  = cmd.cdb[ 9];
  tf->dev       = cmd.cdb[10];
  tf->status    = cmd.cdb[11];
  tf->hob.feat  = 0;


  if( verbose >= 3 )
    fprintf( stderr, "SAT: stat=0x%02x err=0x%02x nsect=0x%02x lbal=0x%02x lbam=0x%02x lbah=0x%02x dev=0x%02x\n",
        tf->status, tf->error, tf->lob.nsect, tf->lob.lbal, tf->lob.lbam, tf->lob.lbah, tf->dev );

  if( tf->status & ( ATA_STAT_ERR | ATA_STAT_DRQ | ATA_STAT_FAULT ) )
  {
    if( verbose >= 3 )
      fprintf( stderr, "SAT: I/O error, ata_op=0x%02x ata_status=0x%02x ata_error=0x%02x\n", tf->command, tf->status, tf->error );
    errno = EIO;
    return -1;
  }
  */

  return 0;
}

void sat_close( struct device_handle *drive )
{
  close( drive->fd );
  drive->fd = -1;
}

void sat_info_mask( __attribute__((unused)) struct drive_info *info )
{

}

int sat_open( const char *device, struct device_handle *drive )
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

  drive->driver_cmd = &sat_cmd;
  drive->close = &sat_close;
  drive->driver = DRIVER_TYPE_SAT;
  drive->info_mask = &sat_info_mask;

  return 0;
}

int sat_isvalidname( const char *device )
{
  if( !strncmp( device, "/dev/sg", 7 ) )
    return 1;

  if( !strncmp( device, "/dev/sd", 7 ) )
    return 1;

  return 0;
}

char *sat_validnames( void )
{
  return "'/dev/sda', '/dev/sg0'";
}
