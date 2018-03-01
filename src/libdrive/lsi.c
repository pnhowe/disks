#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <stddef.h>
#include <sys/sysmacros.h>

#include "cdb.h"
#include "lsi.h"

#include "ata.h" // for ATA commands
#include "scsi.h" // for scsi code util functions

extern int verbose;

int lsi_megasas_cmd( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, const unsigned int timeout )
{
  struct megasas_pthru_frame    *pthru;
  struct megasas_iocpacket      uio;
  int rc;

  if( data && ( data_len > 512 ) )
  {
    if( verbose >= 2 )
    {
      errno = EINVAL;
      fprintf( stderr, "MEGASAS: can't handle buffers larger than 512\n" );  // might not be true, some investigaion is needed
    }
    return -1;
  }

  if( ( verbose >= 4 ) && ( rw == RW_WRITE ) )
    dump_bytes( "MEGASAS: send", data, data_len );

  memset( &uio, 0, sizeof( uio ) );
  pthru = &uio.frame.pthru;
  pthru->cmd = MFI_CMD_PD_SCSI_IO;
  pthru->cmd_status = MFI_CMD_STATUS_POLL_MODE; // 0xff
  pthru->scsi_status = 0x0;
  pthru->target_id = drive->port;
  pthru->lun = 0;
  pthru->cdb_len = cdb_len;
  pthru->timeout = timeout;
  pthru->flags = MFI_FRAME_DIR_READ;
  if( data_len > 0 )
  {
    pthru->sge_count = 1;
    pthru->data_xfer_len = data_len;
    pthru->sgl.sge32[0].phys_addr = (intptr_t)data;
    pthru->sgl.sge32[0].length = (__u32)data_len;
  }
  memcpy( pthru->cdb, cdb, cdb_len );

  uio.host_no = drive->hba;
  if( data_len > 0 )
  {
    uio.sge_count = 1;
    uio.sgl_off = offsetof( struct megasas_pthru_frame, sgl );
    uio.sgl[0].iov_base = data;
    uio.sgl[0].iov_len = data_len;
  }

 //do a dump of the return pthru... see if cmd_status is comming back some where else on esx
 //also install esx on twin01 or 02 to do the ahci harddrive enumeration

  if( verbose >= 4 )
  {
    dump_bytes( "MEGASAS: passthru out", (__u8 *) pthru, sizeof( struct megasas_pthru_frame ) );
    dump_bytes( "MEGASAS: uio out", (__u8 *) &uio, sizeof( struct megasas_iocpacket ) );
  }

  rc = 0;
  errno = 0;
  rc = ioctl( drive->fd, MEGASAS_IOC_FIRMWARE, &uio );
  if( rc ) // code orgionally had || pthru->cmd_status, but in ESXi seems to be == 0xff even if nothing is wrong, so we moved the things we cared about out (seems the ESX drive dosen't set this to anything
  {
    if( errno == ENODEV )
    {
      if( verbose )
        fprintf( stderr, "MEGASAS: Host/HBA %d does not exist.\n", drive->hba );
      errno = EIO;
      return -1;
    }

    if( verbose >= 3 )
      fprintf( stderr, "MEGASAS: Unknown Error. errno: %d, cmd_status: 0x%x, scsi_status: 0x%x\n", errno, pthru->cmd_status, pthru->scsi_status );
    errno = EIO;
    return -1;
  }

  if( verbose >= 4 )
  {
    dump_bytes( "MEGASAS: passthru in", (__u8 *) pthru, sizeof( struct megasas_pthru_frame ) );
    dump_bytes( "MEGASAS: uio in", (__u8 *) &uio, sizeof( struct megasas_iocpacket ) );
  }

  if( pthru->cmd_status == MFI_STAT_DEVICE_NOT_FOUND )
  {
    if( verbose )
      fprintf( stderr, "MEGASAS: Device %d does not exist.\n", drive->port );
    errno = EIO;
    return -1;
  }
  else if( pthru->cmd_status == MFI_STAT_SCSI_IO_FAILED ) // not sure if this is the right response... did it to get drivepower working, should really dig into this to see what should be done
  {
    if( verbose )
      fprintf( stderr, "MEGASAS: IO Failed.\n" );
    errno = ECONNRESET;
    return -1;
  }
  else if( pthru->cmd_status == MFI_STAT_SCSI_DONE_WITH_ERROR ) // not sure if this is the right response... did it to get drivepower working, should really dig into this to see what should be done
  {
    if( verbose )
      fprintf( stderr, "MEGASAS: Sucess with error.\n" );
  }

  if( ( verbose >= 4 ) && ( rw == RW_READ ) )
    dump_bytes( "MEGASAS: received", data, data_len );

  return 0;
}

int lsi_megadev_cmd( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, const unsigned int timeout )
{
  struct uioctl_t uio;
  int rc;

  if( data && ( data_len > 512 ) )
  {
    if( verbose >= 2 )
    {
      errno = EINVAL;
      fprintf( stderr, "MEGADEV: can't handle buffers larger than 512\n" );
    }
    return -1;
  }

  if( drive->port == 7 ) // the controller is at port 7, probably shoudn't be sending it stuff
  {
    errno = EINVAL;
    return -1;
  }

  if( ( verbose >= 4 ) && ( rw == RW_WRITE ) )
    dump_bytes( "MEGADEV: send", data, data_len );

  memset(&uio, 0, sizeof(uio));
  uio.inlen  = data_len;
  uio.outlen = data_len;

  uio.ui.fcs.opcode = 0x80;             // M_RD_IOCTL_CMD
  uio.ui.fcs.adapno = MKADAP(drive->hba);

  uio.data.pointer = (__u8 *)data;

  uio.mbox.cmd = MEGA_MBOXCMD_PASSTHRU;
  uio.mbox.xferaddr = (intptr_t)&uio.pthru;

  uio.pthru.ars     = 1;
  uio.pthru.timeout = timeout;
  uio.pthru.channel = 0;
  uio.pthru.target  = drive->port;
  uio.pthru.cdblen  = cdb_len;
  uio.pthru.reqsenselen  = MAX_REQ_SENSE_LEN;
  uio.pthru.dataxferaddr = (intptr_t)data;
  uio.pthru.dataxferlen  = data_len;
  memcpy(uio.pthru.cdb, cdb, cdb_len);

  rc=ioctl(drive->fd, MEGAIOCCMD, &uio);
  if( uio.pthru.scsistatus || rc != 0 )
  {
    fprintf( stderr, "MEGADEV: result: errno: %d scsistatus: %d\n", errno, uio.pthru.scsistatus );
    errno = EIO;
    return -1;
  }

  if( ( verbose >= 4 ) && ( rw == RW_READ ) )
    dump_bytes( "MEGADEV: received", data, data_len );

  return 0;
}

int lsi_3ware_cmd( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, __attribute__((unused)) const unsigned int timeout )
{
  int rc;

  if( ( drive->protocol != PROTOCOL_TYPE_ATA ) || ( cdb_len != 16 ) || ( cdb[0] != ATA_16 ) ) // this driver can only do ATA
  {
    errno = EINVAL;
    return -1;
  }

  if( data && ( data_len > 512 ) )
  {
    if( verbose >= 2 )
    {
      errno = EINVAL;
      fprintf( stderr, "3WARE: can't handle buffers larger than 512\n" );
    }
    return -1;
  }

  TW_Passthru *passthru=NULL;
  char ioctl_buffer[TW_IOCTL_BUFFER_SIZE];
  TW_Ioctl_Buf_Apache *tw_ioctl_apache=NULL;

  tw_ioctl_apache                               = (TW_Ioctl_Buf_Apache *)ioctl_buffer;
  tw_ioctl_apache->driver_command.control_code  = TW_IOCTL_FIRMWARE_PASS_THROUGH;
  tw_ioctl_apache->driver_command.buffer_length = 512; /* payload size */
  passthru                                      = (TW_Passthru *)&(tw_ioctl_apache->firmware_command.command.oldcommand);

  // Same for (almost) all commands - but some reset below
  passthru->byte0.opcode  = TW_OP_ATA_PASSTHRU;
  passthru->request_id    = 0xFF;
  passthru->unit          = drive->port;
  passthru->status        = 0;
  passthru->flags         = 0x1;

  passthru->features     = cdb[4];
  passthru->sector_count = cdb[6];
  passthru->sector_num   = cdb[8];
  passthru->cylinder_lo  = cdb[10];
  passthru->cylinder_hi  = cdb[12];
  passthru->drive_head   = cdb[13];
  passthru->command      = cdb[14];

  if( ( verbose >= 4 ) && ( rw == RW_WRITE ) )
    dump_bytes( "3WARE: send", data, data_len );

  // 48 bit regs... where?

  // Is this a command that reads or returns 512 bytes?
  // passthru->param values are:
  // 0x0 - non data command without TFR write check,
  // 0x8 - non data command with TFR write check,
  // 0xD - data command that returns data to host from device
  // 0xF - data command that writes data from host to device
  // passthru->size values are 0x5 for non-data and 0x07 for data

  if( ( rw == RW_WRITE ) && data )
  {
    memcpy( tw_ioctl_apache->data_buffer, data, data_len );
    passthru->byte0.sgloff = 0x5;
    passthru->size         = 0x8;  // this should be 7 for 32 bit kernels
    passthru->param        = 0xF;  // PIO data write
  }
  else if( ( rw == RW_READ ) && data )
  {
    passthru->byte0.sgloff = 0x5;
    passthru->size         = 0x8;  // this should be 7 for 32 bit kernels
    passthru->param        = 0xD;
  }
  else
  {
    // Non data command -- but doesn't use large sector
    // count register values.
    passthru->byte0.sgloff = 0x0;
    passthru->size         = 0x5;
    passthru->param        = 0x8;
    passthru->sector_count = 0x0;
  }

  //if( verbose >= 3 )
  //  dump_bytes( "3WARE: passthru out", passthru, sizeof( TW_Passthru ));

  rc = ioctl( drive->fd, TW_IOCTL_FIRMWARE_PASS_THROUGH, tw_ioctl_apache );
  if( verbose >= 3 )
    fprintf( stderr, "3WARE: ioctl returnd: %i, errno: %i\n", rc, errno );

  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "3WARE: ioctl Failed, errno: %i\n", errno );
    return -1;
  }

  //if( verbose >= 3 )
  //  dump_bytes( "3WARE: passthru in", passthru, sizeof( TW_Passthru ));

  // See if the ATA command failed.  Now that we have returned from
  // the ioctl() call, if passthru is valid, then:
  // - passthru->status contains the 3ware controller STATUS  -> I think http://oss.sgi.com/LDP/HOWTO/SCSI-Programming-HOWTO-21.html#ss21.4
  // - passthru->command contains the ATA STATUS register
  // - passthru->features contains the ATA ERROR register
  //
  // Check bits 0 (error bit) and 5 (device fault) of the ATA STATUS
  // If bit 0 (error bit) is set, then ATA ERROR register is valid.
  // While we *might* decode the ATA ERROR register, at the moment it
  // doesn't make much sense: we don't care in detail why the error
  // happened.

  // page 70
  // I'm guessing this is reset, got it shen status==0x03 and command = 0xd0 when doing a reset (wakeup)
  if( passthru->status == 0x03 )
  {
    if( verbose >= 2 )
      fprintf( stderr, "3WARE:   driver timeout\n");
    errno = ETIMEDOUT;
    return -1;
  }

  if( passthru->status || ( passthru->command & 0x21 ) )
  {
    if( verbose >= 2 )
      fprintf( stderr, "3WARE: bad passthru, Controller Status: 0x%x, ATA Status: 0x%x, ATA Error: 0x%x\n", passthru->status, passthru->command, passthru->features );
    errno = EIO;
    return -1;
  }

  if( ( verbose >= 4 ) && ( rw == RW_READ ) )
    dump_bytes( "3WARE: received", data, data_len );

  // If this is a read data command, copy data to output buffer
  if( ( rw == RW_READ ) && data )
    memcpy( data, tw_ioctl_apache->data_buffer, data_len );

  // Return register values
  //cdb[4] = passthru->features;    error
  cdb[6] = passthru->sector_count;
  cdb[8] = passthru->sector_num;
  cdb[10] = passthru->cylinder_lo;
  cdb[12] = passthru->cylinder_hi;
  cdb[13] = passthru->drive_head;
  // cdb[14] = passthru->command;    status

  // 48 bit regs... where?

  // look for nonexistent devices/ports
  //if( cdb[14] == ATA_OP_IDENTIFY && !nonempty( (unsigned char *) data, data_len ) )
  if( cdb[14] == ATA_OP_IDENTIFY &&  ((unsigned char *) data)[60] == 0 && ((unsigned char *) data)[61] == 0 ) // cheating by looking at LBA28, should probably find something better later
  {
    //set_err(ENODEV, "No drive on port %d", port);
    errno = ENODEV;
    return -1;
  }

  return 0;
}
/*
// Hm... interesting...
  if (iop->cmnd[0] == SAT_ATA_PASSTHROUGH_12 || iop->cmnd[0] == SAT_ATA_PASSTHROUGH_16) {
    // Controller does not return ATA output registers in SAT sense data
    if (iop->cmnd[2] & (1 << 5)) // chk_cond
      return set_err(ENOSYS, "ATA return descriptor not supported by controller firmware");
  }
  // SMART WRITE LOG SECTOR causing media errors
  if ((iop->cmnd[0] == SAT_ATA_PASSTHROUGH_16 && iop->cmnd[14] == ATA_SMART_CMD
        && iop->cmnd[3]==0 && iop->cmnd[4] == ATA_SMART_WRITE_LOG_SECTOR) ||
      (iop->cmnd[0] == SAT_ATA_PASSTHROUGH_12 && iop->cmnd[9] == ATA_SMART_CMD &&
        iop->cmnd[3] == ATA_SMART_WRITE_LOG_SECTOR))
    return set_err(ENOSYS, "SMART WRITE LOG SECTOR command is not supported by controller firmware");

*/


void lsi_close( struct device_handle *drive )
{
  close( drive->fd );
  drive->fd = -1;
}

void lsi_info_mask( struct drive_info *info )
{
  info->bit48LBA = 0;
}

int check_megasas_loaded( void )
{
  #include <dirent.h>

  DIR *d;
  struct dirent *dir;

  d = opendir( "/sys/bus/pci/drivers/megaraid_sas/" );
  if( !d )
    return 0;

  while( ( dir = readdir( d ) ) != NULL )
  {
    if( dir->d_type == DT_DIR )
    {
      closedir( d );
      return 1;
    }
  }
  closedir( d );
  return 0;
}

int check_megasas( void )
{
  // This is a bit of a hack, but it's late and this will work untill it's time to do some serious testing and work
  // Basicall we are doing close to what libdrive.py does to detect megaraid_sas driver by looking for a directory in
  // /sys/bus/pci/drivers/megaraid_sas/ if they both exist then make sure the iocel_node exists, if not make it and return
  struct stat statbuf;
  int major;
  int fd;
  char *pos;
  char buff[4096];
  memset( buff, 0, sizeof( buff ) );

  if( !check_megasas_loaded() )
    return 0;

  if( !stat( "/dev/megaraid_sas_ioctl_node", &statbuf ) )
  {
    if( !S_ISCHR( statbuf.st_mode ) ) // Um, not being a char is a problem
    {
      if( verbose )
        fprintf( stderr, "megaraid_sas_ioctl_node Allready exists but isn't a char device.\n" );
      errno = EBADE;
      return -1;
    }

    return 1; // It's allready there assume we are good
  }

  fd = open( "/proc/devices", O_RDONLY );
  if( read( fd, buff, sizeof( buff ) - 1 ) == 0 )
  {
    if( verbose )
      fprintf( stderr, "Error Opening /proc/devices.\n" );
    errno = EBADE;
    return -1;
  }
  close( fd );
  pos = strstr( buff, "megaraid_sas_ioctl" );
  if( pos == NULL )
  {
    if( verbose )
      fprintf( stderr, "Can't find megaraid_sas_iocel major number.\n" );
    errno = EBADE;
    return -1;
  }
  major = atoi( pos - 4 );

  if( mknod( "/dev/megaraid_sas_ioctl_node", S_IFCHR|0400, makedev( major, 0 ) ) )
  {
    if( verbose )
      fprintf( stderr, "Error making megaraid_sas_ioctl_node.\n" );
    return -1;
  }

  return 1;
}

int check_megadev( void )
{
  struct stat statbuf;

  // for now presencance of this is good enough, eventually should really check
  // to see if the driver is loaded and then create /dev/megadev0 if it dosen't exist
  // mknod char major = grep misc /dev/devices  minor = megadev0 in /dev/misc
  if( !stat( "/dev/megadev0", &statbuf ) && S_ISCHR( statbuf.st_mode ) )
    return 1;
  else
    return 0;
}

int lsi_open( const char *device, struct device_handle *drive, __attribute__((unused)) const enum driver_type driver )
{
  struct stat statbuf;
  char dev_name[50];
  char *delim;
  int dev_len;

  drive->driver = 0;

  delim = strchr( device, ':' );
  if ( delim )
  {
    dev_len = delim - device;

    memset( dev_name, 0, sizeof( dev_name ) );
    strncpy( dev_name, device, dev_len ); // Don't need to worry about setting the null b/c we have allready memset 0 dev_name

    int has_megasas = check_megasas();
    int has_megadev = check_megadev();
    if( ( has_megadev == -1 ) || ( has_megasas == -1 ) )
    {
      if( verbose )
        fprintf( stderr, "Error Checking for megasas or megadev driver.\n" );
      return -1;
    }

    if( !strncmp( device, "/dev/twa", ( dev_len - 1 ) ) )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_3WARE;
      drive->driver_cmd = &lsi_3ware_cmd;
    }
    else if( !strncmp( device, "/dev/twl", ( dev_len - 1 ) ) )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_3WARE;
      drive->driver_cmd = &lsi_3ware_cmd;
    }
    else if( !strncmp( device, "/dev/twavm", ( dev_len - 1 ) ) )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_3WARE;
      drive->driver_cmd = &lsi_3ware_cmd;
    }
    else if( !strncmp( device, "/dev/twlvm", ( dev_len - 1 ) ) )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_3WARE;
      drive->driver_cmd = &lsi_3ware_cmd;
    }
    else if( ( !strncmp( device, "/dev/sd", ( dev_len - 1 ) ) || !strncmp( device, "/dev/sd", ( dev_len - 2 ) ) ) && has_megasas )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_MEGASAS;
      drive->driver_cmd = &lsi_megasas_cmd;
    }
    else if( ( !strncmp( device, "host", ( dev_len - 1 ) ) || !strncmp( device, "host", ( dev_len - 2 ) ) || !strncmp( device, "host", ( dev_len - 3 ) ) ) && has_megasas )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_MEGASAS;
      drive->driver_cmd = &lsi_megasas_cmd;
      drive->hba = atoi( device + 4 );
    }
    else if( ( !strncmp( device, "/dev/sd", ( dev_len - 1 ) ) || !strncmp( device, "/dev/sd", ( dev_len - 2 ) ) ) && has_megadev )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_MEGADEV;
      drive->driver_cmd = &lsi_megadev_cmd;
    }
    else if( ( !strncmp( device, "host", ( dev_len - 1 ) ) || !strncmp( device, "host", ( dev_len - 2 ) ) || !strncmp( device, "host", ( dev_len - 3 ) ) ) && has_megadev )
    {
      drive->port = atoi( delim + 1 );
      drive->driver = DRIVER_TYPE_MEGADEV;
      drive->driver_cmd = &lsi_megadev_cmd;
      drive->hba = atoi( device + 4 );
    }
  }

  if( !drive->driver )
  {
    if( verbose )
      fprintf( stderr, "LSI device name '%s', expecting something like '/dev/tw[al]0:15',' /dev/tw[al]vm0:15', 'host1:5', or '/dev/sda:5'\n", device );
    errno = EINVAL;
    return -1;
  }

  if( /*( drive->port < 0 ) ||*/ ( drive->port > 255 ) )
  {
    if( verbose )
      fprintf( stderr, "LSI device port '%d', is out of bounds\n", drive->port );
    errno = EINVAL;
    return -1;
  }

  if( strncmp( device, "host", 4 ) )
  {
    if( verbose >= 3 )
      fprintf( stderr, "Using LSI device '%s' port '%d'\n", dev_name, drive->port );

    if( stat( dev_name, &statbuf ) )
    {
      if( verbose )
        fprintf( stderr, "Unable to stat file '%s', errno: %i\n", dev_name, errno );
      return -1;
    }

    if( ( drive->driver == DRIVER_TYPE_MEGADEV ) || ( drive->driver == DRIVER_TYPE_MEGASAS ) )
    {
      if( !S_ISBLK( statbuf.st_mode ) )
      {
        if( verbose )
          fprintf( stderr, "'%s' Is not a block device\n", dev_name );
        errno = EBADF;
        return -1;
      }
    }
    else
    {
      if( !S_ISCHR( statbuf.st_mode ) )
      {
        if( verbose )
          fprintf( stderr, "'%s' Is not a char device\n", dev_name );
        errno = EBADF;
        return -1;
      }
    }

    drive->fd = open( dev_name, O_RDONLY | O_NONBLOCK );
    if( drive->fd == -1 )
    {
      if( verbose )
        fprintf( stderr, "Error opening device '%s', errno: %i\n", dev_name, errno );
      return -1;
    }

    if( ( drive->driver == DRIVER_TYPE_MEGADEV ) || ( drive->driver == DRIVER_TYPE_MEGASAS ) )
    {
      struct sg_scsi_id sgid;

      if( ioctl( drive->fd, SG_GET_SCSI_ID, &sgid ) == 0)
        drive->hba = sgid.host_no;
      else if( ioctl( drive->fd, SCSI_IOCTL_GET_BUS_NUMBER, &drive->hba ) != 0)
      {
        close( drive->fd );
        drive->fd = -1;
        if( verbose )
          fprintf( stderr, "Error getting bus number, errno: %i\n", errno );
        return -1;
      }
      if( verbose >= 3 )
        fprintf( stderr, "    Host/HBA: %i\n", drive->hba );

      close( drive->fd );
    }
  }
  else
  {
    if( verbose >= 3 )
    {
      if( drive->driver == DRIVER_TYPE_3WARE )
        fprintf( stderr, "Using 3Ware Host/HBA '%i' port '%d'\n", drive->hba, drive->port );
      else if( drive->driver == DRIVER_TYPE_MEGADEV )
        fprintf( stderr, "Using MegaDev Host/HBA '%i' port '%d'\n", drive->hba, drive->port );
      else if( drive->driver == DRIVER_TYPE_MEGASAS )
        fprintf( stderr, "Using MegaSAS Host/HBA '%i' port '%d'\n", drive->hba, drive->port );
    }
  }

  // Now open the control file, we had to have the other for the hba, LSI makes this so complex, but much more extendable
  if( drive->driver == DRIVER_TYPE_MEGADEV )
  {
    drive->fd = open( "/dev/megadev0", O_RDONLY | O_NONBLOCK );
    if( drive->fd == -1 )
    {
      if( verbose )
        fprintf( stderr, "Error opening device '/dev/megadev0', errno: %i\n", errno );
      return -1;
    }
  }
  else if( drive->driver == DRIVER_TYPE_MEGASAS )
  {
    drive->fd = open( "/dev/megaraid_sas_ioctl_node", O_RDONLY | O_NONBLOCK );
    if( drive->fd == -1 )
    {
      drive->fd = open( "/dev/megaraid_sas_ioctl", O_RDONLY | O_NONBLOCK );
      if( drive->fd == -1 )
      {
        if( verbose )
          fprintf( stderr, "Error opening device '/dev/megaraid_sas_ioctl_node' or '/dev/megaraid_sas_ioctl', errno: %i\n", errno );
        return -1;
      }
    }
  }

  drive->close = &lsi_close;
  drive->info_mask = &lsi_info_mask;

  return 0;
}

int lsi_isvalidname( const char *device )
{
  if( !strncmp( device, "/dev/twa", 8 ) )  // should also match twavm
    return 1;

  else if( !strncmp( device, "/dev/twl", 8 ) )   // should also match twlvm
    return 1;

  else if( !strncmp( device, "/dev/sd", 7 ) && ( strlen( device ) > 8 ) && ( ( device[8] == ':' ) || ( device[9] == ':' ) ) )
    return 1;

  else if( !strncmp( device, "host", 4 ) && ( strlen( device ) > 5 ) && ( ( device[5] == ':' ) || ( device[6] == ':' ) || ( device[7] == ':' ) ) ) // should give us 4 digit host numbers
    return 1;

  return 0;
}

char *lsi_validnames( void )
{
  return "'/dev/tw[al]0:15', '/dev/tw[al]vm0:15', 'host1:5', '/dev/sda:5'";
}
