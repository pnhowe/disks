/* barrowed from smartctl os_linux.cpp and os_linux.h */
/*  the Copyright notice from smartctl */
/*
 *  os_linux.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-11 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2003-11 Doug Gilbert <dgilbert@interlog.com>
 * Copyright (C) 2008    Hank Wu <hank@areca.com.tw>
 * Copyright (C) 2008    Oliver Bock <brevilo@users.sourceforge.net>
 * Copyright (C) 2008-11 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2008    Jordan Hargrave <jordan_hargrave@dell.com>
 *
 *  Parts of this file are derived from code that was
 *
 *  Written By: Adam Radford <linux@3ware.com>
 *  Modifications By: Joel Jacobson <linux@3ware.com>
 *                   Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *                    Brad Strand <linux@3ware.com>
 *
 *  Copyright (C) 1999-2003 3ware Inc.
 *
 *  Kernel compatablity By:     Andre Hedrick <andre@suse.com>
 *  Non-Copyright (C) 2000      Andre Hedrick <andre@suse.com>
 *
 * Other ars of this file are derived from code that was
 *
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2000 Andre Hedrick <andre@linux-ide.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

/* Adapeted by Peter Howe pnhowe@gmail.com */
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ide.h"
#include "scsi.h" // for dump_bytes
#include "ata.h" // for ATA_16

#include <linux/hdreg.h>

extern int verbose;

//http://www.jukie.net/bart/blog/ata-via-scsi

// http://www.mjmwired.net/kernel/Documentation/ioctl/hdio.txt

int ide_cmd( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, __attribute__((unused)) const unsigned int timeout )
{
  int rc;
  int type;

  if( ( drive->protocol != PROTOCOL_TYPE_ATA ) || ( cdb_len != 16 ) || ( cdb[0] != ATA_16 ) ) // this driver can only do ATA
  {
    errno = EINVAL;
    return -1;
  }

  __u8 buff[BUFFER_LENGTH];

  if( ( data_len != 0 ) && ( data_len != 512 ) )
  {
    if( verbose >= 2 )
      fprintf( stderr, "Buffer size must be 0 or 512.\n" );

    return -1;
  }

  memset( buff, 0, BUFFER_LENGTH );

    // for DRIVE_TASK
    // NOT DOCUMENTED in /usr/src/linux/include/linux/hdreg.h. You
    // have to read the IDE driver source code.  Sigh.
    // buff[0]: ATA COMMAND CODE REGISTER
    // buff[1]: ATA FEATURES REGISTER
    // buff[2]: ATA SECTOR_COUNT
    // buff[3]: ATA SECTOR NUMBER
    // buff[4]: ATA CYL LO REGISTER
    // buff[5]: ATA CYL HI REGISTER
    // buff[6]: ATA DEVICE HEAD

  // for DRIVE_CMD
  // See struct hd_drive_cmd_hdr in hdreg.h.  Before calling ioctl()
  // buff[0]: ATA COMMAND CODE REGISTER
  // buff[1]: ATA SECTOR NUMBER REGISTER
  // buff[2]: ATA FEATURES REGISTER
  // buff[3]: ATA SECTOR COUNT REGISTER

  // for DRIVE_CMD for SMART
  // See struct hd_drive_cmd_hdr in hdreg.h.  Before calling ioctl()
  // buff[0]: ATA_OP_SMART
  // buff[1]: LBA LOW REGISTER -- is the sub-smart cmd
  // buff[2]: ATA FEATURES REGISTER -- ie smart cmd
  // buff[3]: ATA SECTOR COUNT REGISTER

  // Note that on return:
  // buff[2] contains the ATA SECTOR COUNT REGISTER

  type = ATA_CMD;
  switch( cdb[14] )
  {
    case ATA_OP_IDENTIFY:  //IDENTIFY  NOTE: more is done to this later
      buff[0] = ATA_OP_IDENTIFY;
      buff[3] = 1;
      break;

    case ATA_OP_READ_NATIVE_MAX:
      type = ATA_TASK;
      buff[0] = cdb[14];  // cdb[14]: (ata) command
      buff[1] = cdb[4];   // cdb[4]: features (7:0)
      buff[2] = cdb[6];   // cdb[6]: sector_count (7:0)
      buff[3] = cdb[8];   // cdb[8]: lba_low (7:0)
      buff[4] = cdb[10];  // cdb[10]: lba_mid (7:0)
      buff[5] = cdb[12];  // cdb[12]: lba_high (7:0)
      buff[6] = cdb[13];  // cdb[13]: device
      break;

    case ATA_OP_SMART:
      if( ( rw == RW_READ ) || ( rw == RW_WRITE ) )
      {
        buff[0] = cdb[14];  // cdb[14]: (ata) command
        buff[1] = cdb[8];   // cdb[8]: lba_low (7:0)
        buff[2] = cdb[4];   // cdb[4]: features (7:0)
        buff[3] = cdb[6];   // cdb[6]: sector_count (7:0)
      }
      else
      {
        type = ATA_TASK;
        buff[0] = cdb[14];  // cdb[14]: (ata) command
        buff[1] = cdb[4];   // cdb[4]: features (7:0)
        buff[2] = cdb[6];   // cdb[6]: sector_count (7:0)
        buff[3] = cdb[8];   // cdb[8]: lba_low (7:0)
        buff[4] = cdb[10];  // cdb[10]: lba_mid (7:0)
        buff[5] = cdb[12];  // cdb[12]: lba_high (7:0)
        buff[6] = cdb[13];  // cdb[13]: device
      }
      break;

/*
  case ENABLE:
    buff[2]=ATA_SMART_ENABLE;
    buff[1]=1;
    break;
  case DISABLE:
    buff[2]=ATA_SMART_DISABLE;
    buff[1]=1;
    break;
  case AUTO_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete
    // select == 241 for enable but no data transfer.  Use TASK ioctl.
    buff[1]=ATA_SMART_AUTO_OFFLINE;
    buff[2]=select;
    break;
  case AUTOSAVE:
    // select == 248 for enable but no data transfer.  Use TASK ioctl.
    buff[1]=ATA_SMART_AUTOSAVE;
    buff[2]=select;
    break;
  case IMMEDIATE_OFFLINE:
    buff[2]=ATA_SMART_IMMEDIATE_OFFLINE;
    buff[1]=select;
    break;
    */
  default:
    errno = EINVAL;
    return -1;
  }

  // This command uses the HDIO_DRIVE_TASKFILE ioctl(). This is the
  // only ioctl() that can be used to WRITE data to the disk.
  
  /*
  if( command == SMART_OP_WRITE_LOG )
  {
    unsigned char task[ sizeof( ide_task_request_t ) + 512 ];
    ide_task_request_t *reqtask = ( ide_task_request_t * ) task;
    task_struct_t      *taskfile = (task_struct_t *) reqtask->io_ports;
    int retval;

    memset(task, 0, sizeof( task ) );

    taskfile->data           = 0;
    taskfile->feature        = ATA_SMART_WRITE_LOG_SECTOR;
    taskfile->sector_count   = 1;
    taskfile->sector_number  = select;
    taskfile->low_cylinder   = 0x4f;
    taskfile->high_cylinder  = 0xc2;
    taskfile->device_head    = 0;
    taskfile->command        = ATA_SMART_CMD;

    reqtask->data_phase      = TASKFILE_OUT;
    reqtask->req_cmd         = IDE_DRIVE_TASK_OUT;
    reqtask->out_size        = 512;
    reqtask->in_size         = 0;

    // copy user data into the task request structure
    memcpy( task + sizeof( ide_task_request_t ), data, 512 );

    if( ( retval = ioctl( fd, HDIO_DRIVE_TASKFILE, task ) ) )
    {
      if (retval==-EINVAL)
        pout("Kernel lacks HDIO_DRIVE_TASKFILE support; compile kernel with CONFIG_IDE_TASKFILE_IO set\n");
      return -1;
    }
    return 0;
  }
  */

  if( type == ATA_TASK ) // No Data
  {
    if( verbose >= 3 )
      fprintf( stderr, "IDE: HDIO_DRIVE_TASK\n" );

    if( verbose >= 3 )
      fprintf( stderr, "IDE Task Out: cmd=0x%02x    feat=0x%02x sec_count=0x%02x sec_num=0x%02x cyl_low=0x%02x cyl_hi=0x%02x head=0x%02x\n", buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6] );

    rc = ioctl( drive->fd, HDIO_DRIVE_TASK, buff );

    if( rc )
    {
      if( verbose >= 2 )
        fprintf( stderr, "ioctl Failed, rc: %i errno: %i\n", rc, errno );

      if( rc == -EINVAL )
      {
        if( verbose >= 3 )
        {
          fprintf( stderr, "IDE: Error SMART Status command via HDIO_DRIVE_TASK failed" );
          fprintf( stderr, "Rebuild older linux 2.2 kernels with HDIO_DRIVE_TASK support added\n" );
        }
      }
      else
        fprintf( stderr, "Error SMART Status command failed\n" );

      return -1;
    }

    if( verbose >= 3 )
      fprintf( stderr, "IDE Task  In: status=0x%02x error=0x%02x sec_count=0x%02x sec_num=0x%02x cyl_low=0x%02x cyl_hi=0x%02x head=0x%02x\n", buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6] );

    if( buff[1] )
    {
      if( verbose >= 2 )
         fprintf( stderr, "IDE Error: 0x%02x\n", buff[1] );
      errno = EIO;
      return -1;
    }

    memset( cdb, 0, cdb_len );

    cdb[5] = buff[2];   // cdb[5]: sector_count (7:0)
    cdb[7] = buff[3];   // cdb[7]: lba_low (7:0)
    cdb[9] = buff[4];   // cdb[9]: lba_mid (7:0)
    cdb[11] = buff[5];  // cdb[11]: lba_high (7:0)
    cdb[12] = buff[6];  // cdb[12]: device

    cdb[0] = 0x09; // so ata.c knows there is a CDB
    cdb[1] = 0x0c;

    return 0;
  }

  if( cdb[14] == ATA_OP_IDENTIFY )
  {
    unsigned short deviceid[256];

    if( verbose >= 3 )
      fprintf( stderr, "IDE: HDIO_GET_IDENTITY\n" );

    if ( !ioctl( drive->fd, HDIO_GET_IDENTITY, deviceid ) && ( deviceid[0] & 0x8000 ) )
    {
      if( verbose >= 3 )
        fprintf( stderr, "IDE: IDENTIFY changed to IDENTIFY_PACKET\n" );

      buff[0] = ATA_IDENTIFY_PACKET_DEVICE;
    }
  }

  if( verbose >= 3 )
      fprintf( stderr, "IDE: HDIO_DRIVE_CMD\n" );

/*  Not sure if this works
  if( ( verbose >= 4 ) && ( rw == RW_WRITE ) )
    dump_bytes( "IDE Command: send", data, data_len );

  if( rw == RW_WRITE )
    memcpy( buff + HDIO_DRIVE_CMD_OFFSET, data, data_len );
*/
  if( verbose >= 3 )
    fprintf( stderr, "IDE Out: cmd=0x%02x    sector=0x%02x features=0x%02x count=0x%02x\n", buff[0], buff[1], buff[2], buff[3] );

  rc = ioctl( drive->fd, HDIO_DRIVE_CMD, buff );

  if( verbose >= 3 )
    fprintf( stderr, "IDE  In: status=0x%02x error=0x%02x                  count=0x%02x\n", buff[0], buff[1], buff[3] );

  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "ioctl Failed, rc: %i errno: %i\n", rc, errno );

    return -1;
  }

/*
  // CHECK POWER MODE command returns information in the Sector Count
  // register (buff[3]).  Copy to return data buffer.
  if( command==CHECK_POWER_MODE )
    buff[HDIO_DRIVE_CMD_OFFSET] = buff[2];
*/

  if( rw == RW_READ )
    memcpy( data, buff + HDIO_DRIVE_CMD_OFFSET, data_len );

  if( ( verbose >= 4 ) && ( rw == RW_READ ) )
    dump_bytes( "IDE Command: received", data, data_len );

  memset( cdb, 0, cdb_len );

  cdb[5] = buff[2];   // cdb[5]: sector_count (7:0)

  cdb[0] = 0x09; // so ata.c knows there is a CDB
  cdb[1] = 0x0c;

  return 0;
}

void ide_close( struct device_handle *drive )
{
  close( drive->fd );
  drive->fd = -1;
}

void ide_info_mask( struct drive_info *info )
{
  info->bit48LBA = 0;
  info->supportsDMA = 0;
}

int ide_open( const char *device, struct device_handle *drive )
{
  struct stat statbuf;

  if( stat( device, &statbuf ) )
  {
    if( verbose )
      fprintf( stderr, "Unable to stat file '%s', errno: %i\n", device, errno );
    return -1;
  }
  
  if( !S_ISBLK( statbuf.st_mode ) )
  {
    if( verbose )
      fprintf( stderr, "'%s' Is not a block device\n", device );
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

  drive->driver_cmd = &ide_cmd;
  drive->close = &ide_close;
  drive->driver = DRIVER_TYPE_IDE;
  drive->info_mask = &ide_info_mask;

  return 0;
}

int ide_isvalidname( const char *device )
{
  if( !strncmp( device, "/dev/hd", 7 ) )
    return 1;

  return 0;
}

char *ide_validnames( void )
{
  return "'/dev/hda'";
}
