/* with many thanks to Mark Lord and hdparm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
//#include <sys/types.h>
#include <sys/stat.h>

#include "device.h"
#include "disk.h"

#include "ata.h"
#include "scsi.h"

#include "sgio.h"
#include "lsi.h"
#include "ide.h"
#include "sat.h"

extern int verbose;

extern int cmdTimeout;

int openDisk( const char *device, struct device_handle *drive, const enum driver_type driver, const enum protocol_type protocol, unsigned char ident )
{
  int rc;
  unsigned char data[512];

  memset( &drive->drive_info, 0, sizeof( drive->drive_info ) );

  drive->driver = driver;
  drive->protocol = protocol;

  // Try to detect the type of disk we are working with

  if( verbose >= 3 )
    fprintf( stderr, "Opening '%s'...\n", device );

  if( drive->driver == DRIVER_TYPE_UNKNOWN )
  {
    if( lsi_isvalidname( device ) )
      rc = lsi_open( device, drive, DRIVER_TYPE_UNKNOWN );
    else if( sgio_isvalidname( device ) )
      rc = sgio_open( device, drive );
    else if( sat_isvalidname( device ) )
      rc = sat_open( device, drive );
    else if( ide_isvalidname( device ) )
      rc = ide_open( device, drive );
    else
    {
      if( verbose )
        fprintf( stderr, "Unable to determine the device type, should be %s, %s, %s, or %s\n", sgio_validnames(), sat_validnames(), lsi_validnames(), ide_validnames() );
      errno = EINVAL;
      return -1;
    }

    if( verbose >= 3 )
      fprintf( stderr, "Detected Disk of type '0x%x'\n", drive->driver );
  }
  else if( drive->driver == DRIVER_TYPE_IDE )
    rc = ide_open( device, drive );
  else if( drive->driver == DRIVER_TYPE_SGIO )
    rc = sgio_open( device, drive );
  else if( drive->driver == DRIVER_TYPE_SAT )
    rc = sat_open( device, drive );
  else if( drive->driver == DRIVER_TYPE_MEGADEV )
    rc = lsi_open( device, drive, DRIVER_TYPE_MEGADEV );
  else if( drive->driver == DRIVER_TYPE_MEGASAS )
    rc = lsi_open( device, drive, DRIVER_TYPE_MEGASAS );
  else
  {
    if( verbose )
      fprintf( stderr, "Unknown Driver type '0x%x'\n", driver );
    errno = EINVAL;
    return -1;
  }

  if( drive->driver == DRIVER_TYPE_IDE )
    drive->protocol = PROTOCOL_TYPE_ATA;

  if( rc )
    return -1;

  //now get the device information

  memset( &data, 0, sizeof( data ) );

  if( drive->protocol == PROTOCOL_TYPE_UNKNOWN )
  {
    if( verbose >= 2 )
      fprintf( stderr, "PROBE (SCSI)...\n" );

    rc = exec_cmd_scsi( drive, CMD_PROBE, RW_READ, data, sizeof( data ), NULL, 0, cmdTimeout );
    if( rc == -1 || ( strncmp( ( (char *) data ) + 8, "ATA", 3 ) == 0 ) )
    {
      drive->protocol = PROTOCOL_TYPE_ATA;
      drive->cmd = &exec_cmd_ata;
    }
    else
    {
      drive->protocol = PROTOCOL_TYPE_SCSI;
      drive->cmd = &exec_cmd_scsi;
    }
  }

  if( ident == IDENT )
  {
    if( drive->protocol == PROTOCOL_TYPE_ATA )
    {
      if( verbose >= 2 )
        fprintf( stderr, "IDENTIFY (ATA)...\n" );

      rc = exec_cmd_ata( drive, CMD_IDENTIFY, RW_READ, data, sizeof( data ), NULL, 0, cmdTimeout );
      if( rc == -1 )
      {
        if( verbose )
          fprintf( stderr, "IDENTIFY (ATA) Failed, rc: %i errno: %i\n", rc, errno );
        return -1;
      }

      drive->protocol = PROTOCOL_TYPE_ATA;
      loadInfoATA( drive, (struct ata_identity *) &data );
    }
    else if( drive->protocol == PROTOCOL_TYPE_SCSI )
    {
      if( verbose >= 2 )
        fprintf( stderr, "IDENTIFY (SCSI)...\n" );

      rc = exec_cmd_scsi( drive, CMD_IDENTIFY, RW_READ, data, sizeof( data ), NULL, 0, cmdTimeout );
      if( rc == -1 )
      {
        if( verbose )
          fprintf( stderr, "IDENTIFY (SCSI) Failed, rc: %i errno: %i\n", rc, errno );
        return -1;
      }

      drive->protocol = PROTOCOL_TYPE_SCSI;
      loadInfoSCSI( drive, (struct scsi_identity *) &data );
    }
    else
    {
      if( verbose )
        fprintf( stderr, "Unknown Protocol type '0x%x'\n", protocol );
      errno = EINVAL;
      return -1;
    }
  }

  drive->drive_info.driver = drive->driver;
  drive->drive_info.protocol = drive->protocol;
  return 0;
}

void closeDisk( struct device_handle *drive )
{
  drive->close( drive );
}

struct drive_info *getDriveInfo( struct device_handle *drive )
{
  return &drive->drive_info;
}

int enableSMART( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "SMART_ENABLE...\n" );


  if( !drive->drive_info.SMARTSupport )
  {
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_SMART_ENABLE, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SMART_ENABLE Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  drive->drive_info.SMARTEnabled = 1;

  return 0;
}

int disableSMART( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "SMART_DISABLE...\n" );

  if( !drive->drive_info.SMARTSupport )
  {
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_SMART_DISABLE, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SMART_DISABLE Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  drive->drive_info.SMARTEnabled = 0;

  return 0;
}

// NOTE: this isn't so reliable after all, if hdparm -N dosen't have trouble and this does, something is up, use -vvvv to compare outgoing cdbs
// get the size from the idendity
__u64 getMaxLBA( struct device_handle *drive )
{
  int rc;
  __u64 maxLBA = 0;

  if( verbose >= 2 )
    fprintf( stderr, "GET_MAXLBA...\n" );

  if( !drive->drive_info.supportsSETMAX )
  {
    if( verbose )
      fprintf( stderr, "Drive does not support SETMAX\n" );
    errno = EINVAL;
    return 0;
  }

  rc = drive->cmd( drive, CMD_GET_MAXLBA, RW_NONE, NULL, 0, &maxLBA, 1, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_GET_MAXLBA Failed, rc: %i errno: %i\n", rc, errno );
    return 0;
  }

  return maxLBA;
}

int setMaxLBA( struct device_handle *drive, __u64 LBA )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "SET_MAXLBA...\n" );

  if( !drive->drive_info.supportsSETMAX )
  {
    if( verbose )
      fprintf( stderr, "Drive does not support SETMAX\n" );
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_SET_MAXLBA, RW_NONE, NULL, 0, &LBA, 1, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SET_MAXLBA Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int doReset( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "RESET...\n" );

  rc = drive->cmd( drive, CMD_RESET, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "RESET Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int sleepState( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "SLEEP...\n" );

  rc = drive->cmd( drive, CMD_SLEEP, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "SLEEP Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int standbyState( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "STANDBY...\n" );

  rc = drive->cmd( drive, CMD_STANDBY, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "STANDBY Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int idleState( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "IDLE...\n" );

  rc = drive->cmd( drive, CMD_IDLE, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "IDLE Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int getPowerMode( struct device_handle *drive )
{
  int rc;
  __u64 mode;

  if( verbose >= 2 )
    fprintf( stderr, "GET_POWER_STATE...\n" );

  rc = drive->cmd( drive, CMD_GET_POWER_STATE, RW_NONE, NULL, 0, &mode, 1, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "GET_POWER_STATE Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return mode;
}

int writeSame( struct device_handle *drive, __u64 start, __u64 count, __u32 value )
{
  int rc;
  __u64 parms[3];

  parms[0] = start;
  parms[1] = count;
  parms[2] = value;

  if( verbose >= 2 )
    fprintf( stderr, "WRITESAME...\n" );

  if( !drive->drive_info.supportsWriteSame )
  {
    if( verbose )
      fprintf( stderr, "Drive does not support WriteSame\n" );
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_WRITESAME, RW_NONE, NULL, 0, parms, 3, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_WRITESAME Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

long long int wirteSameStatus( struct device_handle *drive )
{
  int rc;
  __u8 data[512];

  memset( &data, 0, sizeof( data ) );

  if( verbose >= 2 )
    fprintf( stderr, "WRITESAME_STATUS...\n" );

  if( !drive->drive_info.supportsSCT )
  {
    if( verbose )
      fprintf( stderr, "Drive does not support SCT\n" );
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_SCT_STATUS, RW_READ, data, sizeof( data ), NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SCT_STATUS Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  if( verbose >= 2 )
  {
    fprintf( stderr, "Format: 0x%02x%02x\n", data[1], data[0] );
    fprintf( stderr, "Status: 0x%02x 0x%02x 0x%02x 0x%02x\n", data[9], data[8], data[7], data[6] );
    fprintf( stderr, "State:  0x%02x\n\n", data[10] );

    fprintf( stderr, "Ext Status:    0x%02x%02x\n", data[15], data[14] );
    fprintf( stderr, "Action Code:   0x%02x%02x\n", data[17], data[16] );
    fprintf( stderr, "Function Code: 0x%02x%02x\n", data[19], data[18] );
    fprintf( stderr, "Current LBA:   0x%02x%02x%02x%02x%02x%02x%02x%02x\n", data[47], data[46], data[45], data[44], data[43], data[42], data[41], data[40] );
  }

  if( data[15] == 0xff && data[14] == 0xff )  // not done, return position
    return WRITESAME_STATUS_LBA | (((__u64)((data[45] << 16) | (data[44] << 8) | data[43]) << 24) | ((data[42] << 16) | (data[41] << 8) | data[40]));
  else
    return WRITESAME_STATUS_DONE_STATUS | ( data[15] << 8 ) | data[14];

  return 0;
}

int trim( struct device_handle *drive, __u64 start, __u64 count )
{
  int rc;
  __u64 parms[2];
  __u8 data[512];

  memset( &data, 0, sizeof( data ) );

  parms[0] = start;
  parms[1] = count;

  if( verbose >= 2 )
    fprintf( stderr, "Trim...\n" );

  if( !drive->drive_info.supportsTrim )
  {
    if( verbose )
      fprintf( stderr, "Drive does not support Trim\n" );
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_TRIM, RW_NONE, NULL, 0, parms, 2, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_TRIM Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int smartSelfTest( struct device_handle *drive, int type )
{
  int rc;
  __u64 tmpType;

  if( verbose >= 2 )
    fprintf( stderr, "START_SELF_TEST...\n" );

  tmpType = type;

  rc = drive->cmd( drive, CMD_START_SELF_TEST, RW_NONE, NULL, 0, &tmpType, 1, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "START_SELF_TEST Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int smartSelfTestStatus( struct device_handle *drive )
{
  int rc;
  __u8 data[512];
  __u64 status;

  memset( &data, 0, sizeof( data ) );

  if( verbose >= 2 )
    fprintf( stderr, "SELF_TEST_STATUS...\n" );

  rc = drive->cmd( drive, CMD_SELF_TEST_STATUS, RW_READ, data, sizeof( data ), &status, 1, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SELF_TEST_STATUS Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return status;
}

int smartAttributes( struct device_handle *drive, struct smart_attribs *attribs )
{
  int rc;

  memset( attribs, 0, sizeof( *attribs ) );

  if( verbose >= 2 )
    fprintf( stderr, "SMART_ATTRIBS...\n" );

  if( !drive->drive_info.SMARTSupport || !drive->drive_info.SMARTEnabled )
  {
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_SMART_ATTRIBS, RW_READ, attribs, sizeof( *attribs ), NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SMART_ATTRIBS Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int smartStatus( struct device_handle *drive ) //TODO: add SMART enabled
{
  int rc;
  __u64 status;

  if( verbose >= 2 )
    fprintf( stderr, "SMART_STATUS...\n" );

  if( !drive->drive_info.SMARTSupport || !drive->drive_info.SMARTEnabled )
  {
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_SMART_STATUS, RW_NONE, NULL, 0, &status, 1, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SMART_STATUS Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return status;
}

int smartLogCount( struct device_handle *drive )
{
  int rc;
  __u64 count;

  if( verbose >= 2 )
    fprintf( stderr, "SMART_LOG_COUNT...\n" );

  if( !drive->drive_info.SMARTSupport || !drive->drive_info.SMARTEnabled )
  {
    errno = EINVAL;
    return -1;
  }

  rc = drive->cmd( drive, CMD_SMART_LOG_COUNT, RW_NONE, NULL, 0, &count, 1, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_SMART_LOG_COUNT Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return count;
}

int readLBA( struct device_handle *drive, int DMA, int FUA, __u64 LBA, __u16 count, __u8 *buff )
{
  int rc;
  __u64 parms[4];

  parms[0] = LBA;
  parms[1] = count;
  parms[2] = DMA;
  parms[3] = FUA;

  if( verbose >= 2 )
    fprintf( stderr, "READ %llu...\n", LBA );

  rc = drive->cmd( drive, CMD_READ, RW_READ, buff, ( count * drive->drive_info.LogicalSectorSize ), parms, 4, cmdTimeout ); //  logical_change
  if( rc == -1 )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_READ failed, LBA: %llu, count: %u, DMA: %i, FUA: %i, rc: %i errno: %i\n", LBA, count, DMA, FUA, rc, errno );
    return -1;
  }

  return 0;
}

int writeLBA( struct device_handle *drive, int DMA, int FUA, __u64 LBA, __u16 count, __u8 *buff )
{
  int rc;
  __u64 parms[4];

  parms[0] = LBA;
  parms[1] = count;
  parms[2] = DMA;
  parms[3] = FUA;

  if( verbose >= 2 )
    fprintf( stderr, "WRITE %llu...\n", LBA );

  rc = drive->cmd( drive, CMD_WRITE, RW_WRITE, buff, ( count * drive->drive_info.LogicalSectorSize ), parms, 4, cmdTimeout ); // logical_change
  if( rc == -1 )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_WRITE failed, LBA: %llu, count: %u, DMA: %i, FUA: %i, rc: %i errno: %i\n", LBA, count, DMA, FUA, rc, errno );
    return -1;
  }

  return 0;
}

int enableWriteCache( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "ENABLE_WRITE_CACHE ...\n" );

  rc = drive->cmd( drive, CMD_ENABLE_WRITE_CACHE, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_ENABLE_WRITE_CACHE Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}

int disableWriteCache( struct device_handle *drive )
{
  int rc;

  if( verbose >= 2 )
    fprintf( stderr, "DISABLE_WRITE_CACHE ...\n" );

  rc = drive->cmd( drive, CMD_DISABLE_WRITE_CACHE, RW_NONE, NULL, 0, NULL, 0, cmdTimeout );
  if( rc )
  {
    if( verbose >= 2 )
      fprintf( stderr, "CMD_DISABLE_WRITE_CACHE Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  return 0;
}
