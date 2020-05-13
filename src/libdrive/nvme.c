#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/nvme_ioctl.h>

#include "nvme.h"

extern int verbose;

/*
  char SMARTSupport;
  char SMARTEnabled;
  char supportsSelfTest;
  char supportsLBA;
  char supportsWriteSame;
  char supportsTrim;
  char isSSD;
  int RPM;                            // = 0 for not supported
  int LogicalSectorSize;              // size of 1 LBA
  int PhysicalSectorSize;             // how large a sector is on Disk
  unsigned long long LBACount;        // !suportsLBA = 0, change when SETMAX called?
  unsigned long long WWN;             // = 0 for not supported
  // ATA_ONLY
  char firmware_rev[FIRMWARE_REV_LEN];
  int ATA_major_version;
  int ATA_minor_version;
  char bit48LBA;
  char supportsDMA;
  char supportsSCT; //? has SCSI?
  char supportsSETMAX;
  // SCSI_ONLY
  char vendor_id[VENDOR_ID_LEN];
  char version[VERSION_LEN];
  int SCSI_version;
  char hasLogPageErrorWrite;
  char hasLogPageErrorRead;
  char hasLogPageErrorVerify;
  char hasLogPageErrorNonMedium;
  char hasLogPageTemperature;
  char hasLogPageStartStop;
  char hasLogPageSelfTest;
  char hasLogPageSSD;
  char hasLogPageBackgroundScan;
  char hasLogPageFactoryLog;
  char hasLogPageInfoExcept;
  char hasVPDPageSerial;
  char hasVPDPageIdentification;
  char hasVPDPageBlockLimits;
  char hasVPDPageBlockDeviceCharacteristics;
  unsigned int maxUnmapLBACount;
  unsigned int maxUnmapDescriptorCount;
  unsigned long long maxWriteSameLength;
};
*/


void loadInfoNVME( struct device_handle *drive, const struct nvme_identity *identity )
{
  memset( &drive->drive_info, 0, sizeof( drive->drive_info ) );

  strncpy( drive->drive_info.serial, identity->serial_number, MIN(SERIAL_NUMBER_LEN, NVME_SERIAL_NUMBER_LEN ) );
  strncpy( drive->drive_info.model, identity->model_number, MIN(MODEL_NUMBER_LEN, NVME_MODEL_NUMBER_LEN ) );
  strncpy( drive->drive_info.firmware_rev, identity->firmware_rev, MIN(FIRMWARE_REV_LEN, NVME_FIRMREV_LEN ) );
}

int exec_cmd_nvme( struct device_handle *drive, const enum cdb_command command, const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], const unsigned int parm_count, const unsigned int timeout )
{
  if( command == CMD_IDENTIFY )
  {
    struct nvme_cdb cdb;

    cdb.opcode = NVME_OP_AMDIN_GET_LOG_IDENTIFY;

    if( drive->driver_cmd( drive, rw, (unsigned char *)&cdb, sizeof( cdb ), data, data_len, timeout ) )
      return -1;
    return 0;
  }
  else
  {
    errno = ENOSYS;
    return -1;
  }

}

// NOTE: NVME dosn't really have a "cdb", so we will use our own struct "nvme_cdb" to transport commands here
int nvme_cmd( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, const unsigned int timeout )
{
  int rc;
  struct nvme_passthru_cmd cmd;
  struct nvme_cdb *ncdb = (struct nvme_cdb *)cdb;

  if( cdb_len != sizeof( struct nvme_cdb ) )
  {
    errno = EINVAL;
    return -1;
  }

  // io_hdr.dxfer_direction	= data ? ( ( rw == RW_WRITE ) ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV) : SG_DXFER_NONE;
  // io_hdr.dxfer_len	= data ? data_len : 0;

  cmd.opcode = ncdb->opcode;
  cmd.flags = 0;
  cmd.nsid = 0; // namespace id
  cmd.addr = (__u64)data;
  cmd.data_len = data_len;
  cmd.cdw10 = ncdb->cdw10;
  cmd.cdw11 = ncdb->cdw11;
  cmd.cdw12 = ncdb->cdw12;
  cmd.cdw13 = ncdb->cdw13;
  cmd.cdw14 = ncdb->cdw14;
  cmd.cdw15 = ncdb->cdw15;
  cmd.timeout_ms = timeout * 1000; /* msecs */
  cmd.result = 0;

  if( verbose >= 4 )
    fprintf( stderr, "NVME: command in opcode: 0x%02x, data_len: 0x%04x, nsid: 0x%08x, cdw: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", cmd.opcode, cmd.data_len, cmd.nsid, cmd.cdw10, cmd.cdw11, cmd.cdw12, cmd.cdw13, cmd.cdw14, cmd.cdw15 );

  rc = ioctl( drive->fd, NVME_IOCTL_IO_CMD, &cmd );
  if( verbose >= 3 )
    fprintf( stderr, "NVME: send returnd: %i, errno: %i\n", rc, errno );

  if( rc != 0 )
  {
    if( verbose >= 2 )
      fprintf( stderr, "NVME: ioctl Failed, rc: %i, errno: %i\n", rc, errno );

    return -1;
  }

  if( verbose >= 3 )
    fprintf( stderr, "NVME: result: 0x%08x\n", cmd.result );

  return 0;
}


void nvme_close( struct device_handle *drive )
{
  close( drive->fd );
  drive->fd = -1;
}

void nvme_info_mask( __attribute__((unused)) struct drive_info *info )
{

}

int nvme_open( const char *device, struct device_handle *drive )  // just over PCI for now
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

  drive->driver_cmd = &nvme_cmd;
  drive->close = &nvme_close;
  drive->driver = DRIVER_TYPE_NVME;
  drive->info_mask = &nvme_info_mask;

  return 0;
}

int nvme_isvalidname( const char *device )
{
  if( !strncmp( device, "/dev/nvme", 9 ) )
    return 1;

  return 0;
}

char *nvme_validnames( void )
{
  return "'/dev/nvme0n1'";
}
