#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#include "scsi.h"
#include "disk.h"

extern int verbose;

#define min(a,b) (((a) < (b)) ? (a) : (b))

static unsigned long long parse_scsi_number( const unsigned char *buff, const int len )
{
  int i;
  unsigned long long value = 0;

  for( i = 0; i < len; i++ )
    value = ( value << 8 ) + buff[i];

  return value;
}

static int parse_scsi_field( const unsigned char *buff, const unsigned int buff_len, unsigned int *offset, unsigned int *code, unsigned int *flags, unsigned int *len, unsigned char **data ) // return 0 for no more
{
  if( ( *offset + 4 ) >= buff_len )
    return 0;

  *code  = buff[(*offset)++] << 8;
  *code += buff[(*offset)++];
  *flags = buff[(*offset)++];
  *len   = buff[(*offset)++];
  *data =  (unsigned char *) buff + *offset;
  (*offset) += *len;

  return 1;
}

static int load_cdb_scsi( unsigned char cdb[SCSI_16_CDB_LEN], const enum cdb_command command, __attribute__((unused)) int ext, __attribute__((unused)) __u64 parms[], __attribute__((unused)) const unsigned int parm_count )
{
  memset( cdb, 0, SCSI_16_CDB_LEN );

  __u8 control;

  control = 0;

  switch( command )
  {
    case CMD_PROBE:
      if( verbose >= 2 )
        fprintf( stderr, "PROBE...\n" );
      cdb[0] = SCSI_OP_INQUIRY;
      cdb[3] = ( sizeof( struct scsi_identity_page0 ) & 0xff00 ) >> 8;
      cdb[4] = sizeof( struct scsi_identity_page0 ) & 0x00ff;
      cdb[5] = control;
      return 0;

    case CMD_SCSI_INQUIRY:
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_OP_INQUIRY...\n" );
      cdb[0] = SCSI_OP_INQUIRY;
      cdb[3] = ( sizeof( struct scsi_identity_page0 ) & 0xff00 ) >> 8;
      cdb[4] = sizeof( struct scsi_identity_page0 ) & 0x00ff;
      cdb[5] = control;
      return 0;

    case CMD_SCSI_SENSE:
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_SENSE...\n" );
      cdb[0] = SCSI_OP_REQUEST_SENSE; // 8 bit length
      cdb[3] = 0x00;
      cdb[4] = 0xF0; // 240 // FOR all these hardcoded lengths, we really should do the send short length to get the required length of the field first thing, and then re-reqest
      cdb[5] = control;
      return 0;

    case CMD_SCSI_LOG_PAGE:
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_LOG_PAGE, page 0x%02llx...\n", parms[0] );

      if( parm_count != 1 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_OP_LOG_SENSE;
      cdb[2] = SCSI_PC_CUMULATIVE | parms[0];
      cdb[7] = 0x02;
      cdb[8] = 0x00; // 512
      cdb[9] = control;
      return 0;

    case CMD_SCSI_VPD_PAGE:
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_VPD_PAGE, page 0x%02llx...\n", parms[0] );

      if( parm_count != 1 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_OP_INQUIRY;
      cdb[1] = 1;
      cdb[2] = parms[0];
      cdb[3] = 0x00;
      cdb[4] = 0xff; // normally we do 512, but megaraid (at least in JBOD mode)(or mabel it's the SEAGATE disk I had attached to the controller)  can't handle more that 127, hopfully that works for everyone else
      cdb[5] = control;
      return 0;

    case CMD_SCSI_READ_CAPACITY:
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_READ_CAPACITY...\n" );
      cdb[0] = SCSI_READ_CAPACITY_16;
      cdb[1] = 0x10;

      cdb[10] = 0x00;
      cdb[11] = 0x00;
      cdb[12] = 0x02;
      cdb[13] = 0x00; // 512
      cdb[15] = control;
      return 0;

    case CMD_RESET: // really should be active... b/c there is a RESET for scsi
      if( verbose >= 2 )
        fprintf( stderr, "ACTIVE...\n" );
      cdb[0] = SCSI_OP_START_STOP_UNIT;
      cdb[1] = 0x01; /// immed = 1, ie block the call while it is happening
      cdb[3] = 0x00;
      cdb[4] = 0x10;
      cdb[5] = control;
      return 0;

    case CMD_STANDBY:
      if( verbose >= 2 )
        fprintf( stderr, "SLEEP/STANDBY...\n" );
      cdb[0] = SCSI_OP_START_STOP_UNIT;
      cdb[1] = 0x01; /// immed = 1, ie block the call while it is happening
      cdb[3] = 0x00;
      cdb[4] = 0x30;
      cdb[5] = control;
      return 0;

    case CMD_IDLE:
      if( verbose >= 2 )
        fprintf( stderr, "IDLE...\n" );
      cdb[0] = SCSI_OP_START_STOP_UNIT;
      cdb[1] = 0x01; /// immed = 1, ie block the call while it is happening
      cdb[3] = 0x00;
      cdb[4] = 0x20;
      cdb[5] = control;
      return 0;

    case CMD_SCSI_TRIM:
/*
  parms[0] = buff size
*/
      if( verbose >= 2 )
        fprintf( stderr, "TRIM...\n" );

      if( parm_count != 1 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_OP_UNMAP;
      cdb[1] = 0x00; // anchor
      cdb[6] = 0x00; // group number -- lookup
      cdb[7] = ( parms[0] & 0xff00 ) >> 8;
      cdb[8] = parms[0] & 0x00ff;
      cdb[9] = control;
      return 0;

    //case CMD_GET_MAXLBA:
    //  return 0;

    case CMD_SCSI_WRITESAME:
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_WRITESAME...\n" );

      if( parm_count != 2 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_WRITE_SAME_16;
      cdb[1] = 0x00;

      cdb[2] = ( parms[0] & 0xFF00000000000000 ) >> 56;
      cdb[3] = ( parms[0] & 0x00FF000000000000 ) >> 48;
      cdb[4] =  ( parms[0] & 0x0000FF0000000000 ) >> 40;
      cdb[5] =  ( parms[0] & 0x000000FF00000000 ) >> 32;
      cdb[6] =  ( parms[0] & 0x00000000FF000000 ) >> 24;
      cdb[7] =  ( parms[0] & 0x0000000000FF0000 ) >> 16;
      cdb[8] =  ( parms[0] & 0x000000000000FF00 ) >> 8;
      cdb[9] =  ( parms[0] & 0x00000000000000FF );

      cdb[10] = ( parms[1] & 0x00000000FF000000 ) >> 24;
      cdb[12] = ( parms[1] & 0x0000000000FF0000 ) >> 16;
      cdb[13] = ( parms[1] & 0x000000000000FF00 ) >> 8;
      cdb[14] = ( parms[1] & 0x00000000000000FF );

      cdb[15] = control;

      return 0;

    case CMD_SCSI_SEND_DIAGNOSTIC:
/*
  parms[0] = buff size
*/
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_SEND_DIAGNOSTIC...\n" );

      if( parm_count != 2 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_OP_SEND_DIAGNOSTIC;
      cdb[1] = parms[0];
      cdb[3] = ( parms[1] & 0xff00 ) >> 8;
      cdb[4] = parms[1] & 0x00ff;
      cdb[5] = control;
      return 0;

    case CMD_SCSI_RECEIVE_DIAGNOSTIC:
/*
  parms[0] = page
  parms[1] = buff size
*/
      if( verbose >= 2 )
        fprintf( stderr, "SCSI_RECEIVE_DIAGNOSTIC...\n" );

      if( parm_count != 2 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_OP_RECEIVE_DIAGNOSTIC;
      cdb[1] = 0x01;
      cdb[2] = parms[0];
      cdb[3] = ( parms[1] & 0xff00 ) >> 8;
      cdb[4] = parms[1] & 0x00ff;
      cdb[5] = control;
      return 0;

    case CMD_READ:
/*
  parms[0] = LBA;
  parms[1] = count;
  parms[2] = DMA; // ignored
  parms[3] = FUA;
*/
      if( verbose >= 2 )
        fprintf( stderr, "READ...\n" );

      if( parm_count != 4 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_READ_16;
      if( parms[3] )
        cdb[1] = 0x80;
      else
        cdb[1] = 0x00;

      cdb[2] = ( parms[0] & 0xFF00000000000000 ) >> 56;
      cdb[3] = ( parms[0] & 0x00FF000000000000 ) >> 48;
      cdb[4] =  ( parms[0] & 0x0000FF0000000000 ) >> 40;
      cdb[5] =  ( parms[0] & 0x000000FF00000000 ) >> 32;
      cdb[6] =  ( parms[0] & 0x00000000FF000000 ) >> 24;
      cdb[7] =  ( parms[0] & 0x0000000000FF0000 ) >> 16;
      cdb[8] =  ( parms[0] & 0x000000000000FF00 ) >> 8;
      cdb[9] =  ( parms[0] & 0x00000000000000FF );

      cdb[10] = ( parms[1] & 0x00000000FF000000 ) >> 24;
      cdb[11] = ( parms[1] & 0x0000000000FF0000 ) >> 16;
      cdb[12] = ( parms[1] & 0x000000000000FF00 ) >> 8;
      cdb[13] = ( parms[1] & 0x00000000000000FF );

      cdb[15] = control;

      return 0;

    case CMD_WRITE:
/*
  parms[0] = LBA;
  parms[1] = count;
  parms[2] = DMA; // ignored
  parms[3] = FUA;
*/
      if( verbose >= 2 )
        fprintf( stderr, "WRITE...\n" );

      if( parm_count != 4 )
      {
        errno = EINVAL;
        return -1;
      }

      cdb[0] = SCSI_WRITE_16;
      if( parms[3] )
        cdb[1] = 0x80;
      else
        cdb[1] = 0x00;

      cdb[2] = ( parms[0] & 0xFF00000000000000 ) >> 56;
      cdb[3] = ( parms[0] & 0x00FF000000000000 ) >> 48;
      cdb[4] =  ( parms[0] & 0x0000FF0000000000 ) >> 40;
      cdb[5] =  ( parms[0] & 0x000000FF00000000 ) >> 32;
      cdb[6] =  ( parms[0] & 0x00000000FF000000 ) >> 24;
      cdb[7] =  ( parms[0] & 0x0000000000FF0000 ) >> 16;
      cdb[8] =  ( parms[0] & 0x000000000000FF00 ) >> 8;
      cdb[9] =  ( parms[0] & 0x00000000000000FF );

      cdb[10] = ( parms[1] & 0x00000000FF000000 ) >> 24;
      cdb[11] = ( parms[1] & 0x0000000000FF0000 ) >> 16;
      cdb[12] = ( parms[1] & 0x000000000000FF00 ) >> 8;
      cdb[13] = ( parms[1] & 0x00000000000000FF );

      cdb[15] = control;

      return 0;

    default:
      errno = ENOSYS;
      return -1;
  }
}

void loadInfoSCSI( struct device_handle *drive, const struct scsi_identity *identity )
{
  memcpy( &( drive->drive_info ), identity, sizeof( drive->drive_info ) );
}


#define INC_I_CHECK_MAX_SCSI_ATTRIBS i++; \
if( i >= MAX_SCSI_ATTRIBS ) \
{  \
  if( verbose >= 2 ) \
    fprintf( stderr, "More SCSI Attributes found than fit in smart_attribs: %i\n", i ); \
  errno = EBADE; \
  return -1; \
} \

int exec_cmd_scsi( struct device_handle *drive, const enum cdb_command command, const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], const unsigned int parm_count, const unsigned int timeout )
{
  __u8 cdb[SCSI_16_CDB_LEN];

  if( command == CMD_IDENTIFY )
  {
    unsigned char tdata[512];
    __u64 tmpParm[1];
    unsigned int offset;
    unsigned int buff_len;
    struct scsi_identity_page0 *page0 = ( struct scsi_identity_page0 *) tdata;
    struct drive_info *info = ( struct drive_info *) data;

    if( data_len < sizeof( struct scsi_identity_page0 ) )
    {
      errno = EINVAL;
      return -1;
    }

    memset( data, 0, data_len );
    // what to do with [GLTSD (Global Logging Target Save Disable) set. Enable Save with '-S on'] ??

    memset( tdata, 0, sizeof( tdata ) );
    if( exec_cmd_scsi( drive, CMD_SCSI_INQUIRY, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    if( ( ( page0->peripherial_info & 0xe0 ) != 0 ) || ( ( ( page0->peripherial_info & 0x1f ) != SCSI_DIRECT_ACCESS ) ) )
    {
      if( verbose >= 2 )
        fprintf( stderr, "Not a Direct access block device (ie. Harddrive ).\n" );
      errno = EINVAL;
      return -1;
    }

    strncpy( info->vendor_id, (char *) page0->vendor_id, SCSI_VENDOR_ID_RAW_LEN );
    info->vendor_id[SCSI_VENDOR_ID_RAW_LEN - 1] = '\0';
    strncpy( info->model, (char *) page0->model_number, SCSI_MODEL_NUMBER_RAW_LEN );
    info->model[SCSI_MODEL_NUMBER_RAW_LEN  - 1] = '\0';
    strncpy( info->version, (char *) page0->product_rev, SCSI_VERSION_RAW_LEN );
    info->version[SCSI_VERSION_RAW_LEN- 1] = '\0';
    info->supportsLBA = ( page0->peripherial_info == 0x00 ); // is a Direct Access Block Device
    info->SCSI_version = ( page0->version_descriptor1[0] << 8 ) + page0->version_descriptor1[1];

    memset( tdata, 0, sizeof( tdata ) );
    tmpParm[0] = SCSI_LOG_SENCE_PAGE_LIST;
    if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
    {
      if( errno != 52 ) // vmware (and mabey others) won't have SCSI log, should have a better way to detect "INVALID COMMAND OPERATION CODE"
        return -1;
    }
    else
    {
      info->supportsWriteSame = 1;
      info->SMARTSupport = 1;
      info->SMARTEnabled = 1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      for( ; offset < buff_len; offset++ )
      {
        switch( tdata[offset] )
        {
          case( SCSI_LOG_SENCE_PAGE_ERROR_WRITE ):
            info->hasLogPageErrorWrite = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_ERROR_READ ):
            info->hasLogPageErrorRead = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_ERROR_VERIFY ):
            info->hasLogPageErrorVerify = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_ERROR_NON_MEDIUM ):
            info->hasLogPageErrorNonMedium = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_TEMPERATURE ):
            info->hasLogPageTemperature = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_START_STOP ):
            info->hasLogPageStartStop = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_SELF_TEST ):
            info->hasLogPageSelfTest = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_SSD ):
            info->hasLogPageSSD = 1;
            info->isSSD = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_BACKGROUND_SCAN ):
            info->hasLogPageBackgroundScan = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_FACTORY_LOG  ):
            info->hasLogPageFactoryLog = 1;
            break;
          case( SCSI_LOG_SENCE_PAGE_INFO_EXCEPT ):
            info->hasLogPageInfoExcept = 1;
            break;
        }
      }
    }

    memset( tdata, 0, sizeof( tdata ) );
    tmpParm[0] = SCSI_VPD_PAGE_LIST;
    if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
      return -1;

    offset = 4;
    buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

    for( ; offset < buff_len; offset++ )
    {
      switch( tdata[offset] )
      {
        case( SCSI_VPD_PAGE_SERIAL ):
          info->hasVPDPageSerial = 1;
          break;
        case( SCSI_VPD_PAGE_IDENT ):
          info->hasVPDPageIdentification = 1;
          break;
        case( SCSI_VPD_PAGE_BLOCK_LIMITS ):
          info->hasVPDPageBlockLimits = 1;
          break;
        case( SCSI_VPD_PAGE_BLOCK_DEV_CHRSTCS ):
          info->hasVPDPageBlockDeviceCharacteristics = 1;
          break;
      }
    }

    if( info->hasVPDPageSerial )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_VPD_PAGE_SERIAL;
      if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      strncpy( info->serial, (char *) &tdata[4], min( SERIAL_NUMBER_LEN, tdata[3] ) );
      info->serial[SERIAL_NUMBER_LEN - 1] = '\0';
    }
    else
      strncpy( info->serial, "<Unknown>", SERIAL_NUMBER_LEN );

    if( info->hasVPDPageIdentification )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_VPD_PAGE_IDENT;
      if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      for( ; offset < buff_len; offset++ )
      {
        if( ( ( tdata[offset + 1] & 0x0f ) == 3 ) && ( tdata[offset + 3] == 8 ) )
        {
          info->WWN = getu64( &tdata[offset + 4] ); // ( ( ( __u64 ) ( tdata[offset + 4] ) ) << 56 ) | ( ( ( __u64 ) ( tdata[offset + 5] ) ) << 48 ) | ( ( ( __u64 ) ( tdata[offset + 6] ) ) << 40 ) | ( ( ( __u64 ) tdata[offset + 7] ) << 32 ) | ( ( ( __u64 ) tdata[offset + 8] ) << 24 ) | ( ( ( __u64 ) tdata[offset + 9] ) << 16 ) | ( ( ( __u64 ) tdata[offset + 10] ) << 8 ) | tdata[offset + 11];
          break;
        }
      }
    }

    if( info->hasVPDPageBlockLimits )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_VPD_PAGE_BLOCK_LIMITS;
      if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      if( ( tdata[3] > 0x10 ) && ( tdata[20] | tdata[21] | tdata[22] | tdata[23] ) )// ie tdata[20-23] is non-zero
      {
        info->supportsTrim = 1;
        info->maxUnmapLBACount = getu32( &tdata[20] ); //( (__u64) tdata[20] << 24 ) + ( (__u64) tdata[21] << 16 ) + ( (__u64) tdata[22] << 8 ) + ( (__u64) tdata[23] );
        info->maxUnmapDescriptorCount = getu32( &tdata[24] ); //( (__u64) tdata[24] << 24 ) + ( (__u64) tdata[25] << 16 ) + ( (__u64) tdata[26] << 8 ) + ( (__u64) tdata[27] );
        info->maxWriteSameLength = getu64( &tdata[36] ); //( ( __u64 ) tdata[36] << 56 ) + ( ( __u64 ) tdata[37] << 48 ) + ( ( __u64 ) tdata[38] << 40 ) + ( ( __u64 ) tdata[39] << 32 ) + ( (__u64) tdata[40] << 24 ) + ( (__u64) tdata[41] << 16 ) + ( (__u64) tdata[42] << 8 ) + ( (__u64) tdata[43] );
      }
    }

    if( info->hasVPDPageBlockDeviceCharacteristics )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_VPD_PAGE_BLOCK_DEV_CHRSTCS;
      if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      info->RPM = parse_scsi_number( &tdata[4], 2 );
      if( info->RPM == 1 )
      {
        info->isSSD = 1;
        info->RPM = 0;
      }
    }

    if( info->supportsLBA )
    {
      memset( tdata, 0, sizeof( tdata ) );
      if( exec_cmd_scsi( drive, CMD_SCSI_READ_CAPACITY, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
        return -1;

      info->LBACount = getu64( &tdata[0] ); // ( (__u64) tdata[0] << 56 ) + ( (__u64) tdata[1] << 48 ) + ( (__u64) tdata[2] << 40 ) + ( (__u64) tdata[3] << 32 ) + ( (__u64) tdata[4] << 24 ) + ( (__u64) tdata[5] << 16 ) + ( (__u64) tdata[6] << 8 ) + ( (__u64) tdata[7] );

      info->LogicalSectorSize = getu32( &tdata[8] ); // ( (__u32) tdata[8] << 24 ) + ( (__u32) tdata[9] << 16 ) + ( (__u32) tdata[10] << 8 ) + ( (__u32) tdata[11] );

      if( tdata[13] & 0x0f )
        info->PhysicalSectorSize = pow( 2, ( tdata[13] & 0x0f ) ) * info->LogicalSectorSize;
      else
        info->PhysicalSectorSize = info->LogicalSectorSize;
    }

    return 0;
  }
  else if( command == CMD_IDENTIFY_ENCLOSURE )
  {
    unsigned char tdata[512];
    __u64 tmpParm[2];
    unsigned int offset;
    unsigned int buff_len;

    struct scsi_identity_page0 *page0 = ( struct scsi_identity_page0 *) tdata;
    struct enclosure_info *info = ( struct enclosure_info *) data;

    if( data_len < sizeof( struct scsi_identity_page0 ) )
    {
      errno = EINVAL;
      return -1;
    }

    memset( data, 0, data_len );
    memset( tdata, 0, sizeof( tdata ) );
    if( exec_cmd_scsi( drive, CMD_SCSI_INQUIRY, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    if( ( ( page0->peripherial_info & 0xe0 ) != 0 ) || ( ( ( page0->peripherial_info & 0x1f ) != SCSI_ENCLOSURE_SERVICES ) ) )
    {
      if( verbose >= 2 )
        fprintf( stderr, "Not an Enclosure.\n" );
      errno = EINVAL;
      return -1;
    }

    strncpy( info->vendor_id, (char *) page0->vendor_id, SCSI_VENDOR_ID_RAW_LEN );
    info->vendor_id[SCSI_VENDOR_ID_RAW_LEN - 1] = '\0';
    strncpy( info->model, (char *) page0->model_number, SCSI_MODEL_NUMBER_RAW_LEN );
    info->model[SCSI_MODEL_NUMBER_RAW_LEN  - 1] = '\0';
    strncpy( info->version, (char *) page0->product_rev, SCSI_VERSION_RAW_LEN );
    info->version[SCSI_VERSION_RAW_LEN- 1] = '\0';
    info->SCSI_version = ( page0->version_descriptor1[0] << 8 ) + page0->version_descriptor1[1];

    tmpParm[0] = 0;
    tmpParm[1] = sizeof( tdata );
    memset( tdata, 0, sizeof( tdata ) );  // burn() ?
    if( exec_cmd_scsi( drive, CMD_SCSI_RECEIVE_DIAGNOSTIC, RW_READ, tdata, sizeof( tdata ), tmpParm, 2, timeout ) )
      return -1;

    offset = 4;
    buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

    for( ; offset < buff_len; offset++ )
    {
      switch( tdata[offset] )
      {
        case( SCSI_DIAGNOSTIC_PAGE_SES_CONFIGURATION ):
          info->hasDiagnosticPageConfiguration = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_ENCLOSURE_CNTRL_STATUS ):
          info->hasDiagnosticPageControlStatus = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_ENCLOSURE_HELP_TEXT ):
          info->hasDiagnosticPageHelpText = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_ENCLOSURE_STRING ):
          info->hasDiagnosticPageString = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_THRESHOLD ):
          info->hasDiagnosticPageThreshold = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_ELEMENT_DESCRIPTOR ):
          info->hasDiagnosticPageElementDescriptor = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_SHORT_ENCOLSURE_STATUS ):
          info->hasDiagnosticPageShortStatus = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_ENCOLSURE_BUSY ):
          info->hasDiagnosticPageBusy = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_ADDITIONAL_ELEMENT ):
          info->hasDiagnosticPageAdditionalElement = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_SUBENCOLSURE_HELP_TEXT ):
          info->hasDiagnosticPageSubHelpText = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_SUBENCOLSURE_STRING ):
          info->hasDiagnosticPageSubString = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_SUPPORTED_SES_DIAGNOSTIC_PAGES ):
          info->hasDiagnosticPageSupportedDiagnostigPages = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_DOWNLOAD_MICROCODE ):
          info->hasDiagnosticPageDownloadMicroCode = 1;
          break;
        case( SCSI_DIAGNOSTIC_PAGE_SES_SUBENCOLSURE_NICKNAME ):
          info->hasDiagnosticPageSubNickname = 1;
          break;
      }
    }

    memset( tdata, 0, sizeof( tdata ) );
    tmpParm[0] = SCSI_VPD_PAGE_LIST;
    if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
      return -1;

    offset = 4;
    buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

    for( ; offset < buff_len; offset++ )
    {
      switch( tdata[offset] )
      {
        case( SCSI_VPD_PAGE_SERIAL ):
          info->hasVPDPageSerial = 1;
          break;
        case( SCSI_VPD_PAGE_IDENT ):
          info->hasVPDPageIdentification = 1;
          break;
      }
    }

    if( info->hasVPDPageSerial )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_VPD_PAGE_SERIAL;
      if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      strncpy( info->serial, (char *) &tdata[4], min( SERIAL_NUMBER_LEN, tdata[3] ) );
      info->serial[SERIAL_NUMBER_LEN - 1] = '\0';
    }
    else
      strncpy( info->serial, "<Unknown>", SERIAL_NUMBER_LEN );

    if( info->hasVPDPageIdentification )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_VPD_PAGE_IDENT;
      if( exec_cmd_scsi( drive, CMD_SCSI_VPD_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      for( ; offset < buff_len; offset++ )
      {
        if( ( ( tdata[offset + 1] & 0x0f ) == 3 ) && ( tdata[offset + 3] == 8 ) )
        {
          info->WWN = getu64( &tdata[offset + 5] ); // ( ( ( __u64 ) ( tdata[offset + 5] ) ) << 48 ) | ( ( ( __u64 ) ( tdata[offset + 6] ) ) << 40 ) | ( ( ( __u64 ) tdata[offset + 7] ) << 32 ) | ( ( ( __u64 ) tdata[offset + 8] ) << 24 ) | ( ( ( __u64 ) tdata[offset + 9] ) << 16 ) | ( ( ( __u64 ) tdata[offset + 10] ) << 8 ) | tdata[offset + 11];
          break;
        }
      }
    }

    return 0;
  }
  else if( command == CMD_SMART_STATUS )
  {
    unsigned char tdata[512];
    __u64 tmpParm[1];
    unsigned int offset;
    unsigned int buff_len;
    unsigned int parm_code;
    unsigned int flags;
    unsigned int len;
    unsigned char *wrkData;
    unsigned char asc, ascq;

    if( parm_count != 1 )
    {
      errno = EINVAL;
      return -1;
    }

    asc = 0;
    ascq = 0;

    if( drive->drive_info.hasLogPageInfoExcept )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_INFO_EXCEPT;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;
      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        if( parm_code == 0 )
        {
          if( len >= 2 )
          {
            asc = wrkData[0];
            ascq = wrkData[1];
          // if len >= 3, wrkData[2] -> curTemp
          // if len >= 4, wrkData[3] -> tripTemp
          // if len >= 5, wrkData[4] -> maxTemp
          /* also :
              if ((! temperatureSet) && hasTempLogPage) {
        if (0 == scsiGetTemp(device, &currTemp, &trTemp)) {
            *currenttemp = currTemp;
            *triptemp = trTemp;
        }
    }
    */
          }
        }
      }
      if( verbose >= 3 )
        fprintf( stderr, "asc:   0x%02x ascq: 0x%02x\n", asc, ascq );
    }

    if( asc == 0 )
    {
      memset( tdata, 0, sizeof( tdata ) );
      if( exec_cmd_scsi( drive, CMD_SCSI_SENSE, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
        return -1;

      if( ( tdata[0] != 0x70 ) && ( tdata[0] != 0x71 ) )
      {
        errno = EBADE;
        return -1;
      }

      asc = tdata[12];
      ascq = tdata[13];
    }

    if( verbose >= 3 )
      fprintf( stderr, "asc:   0x%02x ascq: 0x%02x\n", asc, ascq );

    *parms = STATUS_DRIVE_GOOD;
    if( asc == SCSI_ASC_IMPENDING_FAILURE )
      *parms |= STATUS_THRESHOLD_EXCEEDED;

    if( ( asc == SCSI_ASC_WARNING ) && ( ( ascq == SCSI_ASCQ_SELFTEST_FAILED ) || ( ascq == SCSI_ASCQ_PRE_SCAN_MEDIUM_ERROR ) || ( ascq == SCSI_ASCQ_MEDIUM_SCAN_MEDIUM_ERROR ) ) )
      *parms |= STATUS_DRIVE_FAULT;

    return 0;
  }
  else if( command == CMD_SMART_ATTRIBS )
  {
    unsigned int i;
    unsigned char tdata[512];
    __u64 tmpParm[1];
    unsigned int offset;
    unsigned int buff_len;
    unsigned int parm_code;
    unsigned int flags;
    unsigned int len;
    unsigned char *wrkData;
    struct smart_attribs *wrkAttribs = data;

    if( data_len < sizeof( struct smart_attribs ) )
    {
      errno = EINVAL;
      return -1;
    }

    memset( data, 0, data_len );

    wrkAttribs->protocol = PROTOCOL_TYPE_SCSI;
    i = 0;

    if( drive->drive_info.hasLogPageErrorWrite )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_ERROR_WRITE;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        wrkAttribs->data.scsi[i].page_code    = SCSI_LOG_SENCE_PAGE_ERROR_WRITE;
        wrkAttribs->data.scsi[i].parm_code    = parm_code;
        wrkAttribs->data.scsi[i].value        = parse_scsi_number( wrkData, len );

        INC_I_CHECK_MAX_SCSI_ATTRIBS
      }
    }

    if( drive->drive_info.hasLogPageErrorRead )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_ERROR_READ;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        wrkAttribs->data.scsi[i].page_code    = SCSI_LOG_SENCE_PAGE_ERROR_READ;
        wrkAttribs->data.scsi[i].parm_code    = parm_code;
        wrkAttribs->data.scsi[i].value        = parse_scsi_number( wrkData, len );

        INC_I_CHECK_MAX_SCSI_ATTRIBS
      }
    }

    if( drive->drive_info.hasLogPageErrorVerify )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_ERROR_VERIFY;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        wrkAttribs->data.scsi[i].page_code    = SCSI_LOG_SENCE_PAGE_ERROR_VERIFY;
        wrkAttribs->data.scsi[i].parm_code    = parm_code;
        wrkAttribs->data.scsi[i].value        = parse_scsi_number( wrkData, len );

        INC_I_CHECK_MAX_SCSI_ATTRIBS
      }
    }

    if( drive->drive_info.hasLogPageErrorNonMedium )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_ERROR_NON_MEDIUM;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        wrkAttribs->data.scsi[i].page_code    = SCSI_LOG_SENCE_PAGE_ERROR_NON_MEDIUM;
        wrkAttribs->data.scsi[i].parm_code    = parm_code;
        wrkAttribs->data.scsi[i].value        = parse_scsi_number( wrkData, len );

        INC_I_CHECK_MAX_SCSI_ATTRIBS
      }
    }

    if( drive->drive_info.hasLogPageTemperature )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_TEMPERATURE;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        if( parm_code == 0 )
        {
          wrkAttribs->data.scsi[i].page_code    = SCSI_LOG_SENCE_PAGE_TEMPERATURE;
          wrkAttribs->data.scsi[i].parm_code    = parm_code;
          wrkAttribs->data.scsi[i].value        = parse_scsi_number( wrkData, len );

          INC_I_CHECK_MAX_SCSI_ATTRIBS

          break;
        }
      }
    }

    if( drive->drive_info.hasLogPageBackgroundScan )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_BACKGROUND_SCAN;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        if( parm_code == 0 )
        {
          struct sence_log_15h_100h *tmp = (struct sence_log_15h_100h *)wrkData;
          wrkAttribs->data.scsi[i].page_code    = SCSI_LOG_SENCE_PAGE_BACKGROUND_SCAN;
          wrkAttribs->data.scsi[i].parm_code    = parm_code;
          wrkAttribs->data.scsi[i].value        = ( tmp->accumulated_power_on_min[0] << ( 8 * 3 ) ) +
                                                  ( tmp->accumulated_power_on_min[1] << ( 8 * 2 ) ) +
                                                  ( tmp->accumulated_power_on_min[2] << ( 8 * 1 ) ) +
                                                  ( tmp->accumulated_power_on_min[3] );

          INC_I_CHECK_MAX_SCSI_ATTRIBS

          break;
        }
      }
    }

    if( drive->drive_info.hasLogPageFactoryLog )
    {
      memset( tdata, 0, sizeof( tdata ) );
      tmpParm[0] = SCSI_LOG_SENCE_PAGE_FACTORY_LOG;
      if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
        return -1;

      offset = 4;
      buff_len = parse_scsi_number( &tdata[2], 2 ) + offset;

      while( parse_scsi_field( tdata, buff_len, &offset, &parm_code, &flags, &len, &wrkData ) )
      {
        if( parm_code == 0 )
        {
          wrkAttribs->data.scsi[i].page_code    = SCSI_LOG_SENCE_PAGE_FACTORY_LOG;
          wrkAttribs->data.scsi[i].parm_code    = parm_code;
          wrkAttribs->data.scsi[i].value        = parse_scsi_number( wrkData, len );

          INC_I_CHECK_MAX_SCSI_ATTRIBS

          break;
        }
      }
    }

    wrkAttribs->count = i;

    return 0;
  }
  else if( command == CMD_SMART_LOG_COUNT )
  {
    if( parm_count != 1 )
    {
      errno = EINVAL;
      return -1;
    }

    *parms = 0; // Don't know where we would get this that isn't coverd by the "attribs"

    return 0;
  }
  else if( command == CMD_SELF_TEST_STATUS )
  {
    unsigned char tdata[512];
    __u64 tmpParm[1];
    if( parm_count != 1 )
    {
      errno = EINVAL;
      return -1;
    }

    if( !drive->drive_info.hasLogPageSelfTest )
    {
      *parms = 0;
      return 0;
    }

    memset( tdata, 0, sizeof( tdata ) );
    tmpParm[0] = SCSI_LOG_SENCE_PAGE_SELF_TEST;
    if( exec_cmd_scsi( drive, CMD_SCSI_LOG_PAGE, RW_READ, tdata, sizeof( tdata ), tmpParm, 1, timeout ) )
      return -1;

    // top (most recent) log starts at 5
    // see table 251 - Self-test results log parameter format
    // result is offset 4 && 0x0f

    switch( tdata[8] & 0x0f )
    { // see "Table 253. SELF-TEST RESULTS field
      case 0x00:
        *parms = SELFTEST_COMPLETE;
        break;
      case 0x01:
        *parms = SELFTEST_HOST_ABBORTED;
        break;
      case 0x02:
        *parms = SELFTEST_HOST_INTERRUPTED;
        break;
      case 0x03:
      case 0x04:
      case 0x05:
      case 0x06:
        *parms = SELFTEST_FAILED | ( tdata[8] & 0x0f );
        break;
      case 0x07:
        *parms = SELFTEST_FAILED | ( ( tdata[9] << 4 ) | 0x07 ) ;
        break;
      case 0x0F:
        *parms = SELFTEST_PER_COMPLETE | tdata[9];
        break;
      default:
        *parms = SELFTEST_UNKNOWN | ( tdata[8] & 0x0f );
        break;
    }

    return 0;
  }
  else if( command == CMD_START_SELF_TEST )
  {
    __u64 tmpParm[2];
    tmpParm[0] = 0; // control field
    tmpParm[1] = 0; // length

    if( parm_count != 1 )
    {
      errno = EINVAL;
      return -1;
    }

    if( !drive->drive_info.hasLogPageSelfTest )
    {
      if( verbose >= 2 )
        fprintf( stderr, "No Self-test Log Page\n" );
      return 2;
    }

    if( ( ( *parms & 0x0F ) == SELFTEST_CONVEYANCE ) || ( *parms == SELFTEST_OFFLINE ) )
    {
     if( verbose >= 2 )
        fprintf( stderr, "Conveyance and Offline not supported by SCSI.\n" );
      errno = EBADE;
      return -1;
    }

    if( *parms == SELFTEST_ABORT ) /// if it's foreground do ABORT TASK
      tmpParm[0] = 0x80; // code = 100b  self-test = 0 - Abort background self-test
    else
    {
      if( ( ( *parms & 0x0F ) == SELFTEST_EXTENDED ) )
        tmpParm[0] = 0x40; // code = 010b  self-test = 0 - Backgroup Short
      else if ( ( ( *parms & 0x0F ) == SELFTEST_SHORT )  )
        tmpParm[0] = 0x20; // code = 001b  self-test = 0 - Background Extended
      else
      {
        if( verbose >= 2 )
          fprintf( stderr, "Self test Confustion, type: 0x%llx\n", *parms );
        errno = EBADE;
        return -1;
      }

      if( ( *parms & 0xF0 ) == SELFTEST_CAPTIVE ) // do foreground
        tmpParm[0] |= 0x80;
    }

    if( exec_cmd_scsi( drive, CMD_SCSI_SEND_DIAGNOSTIC, RW_NONE, NULL, 0, tmpParm, 2, timeout ) )
      return -1;

    return 0;
  }
  else if( command == CMD_WRITESAME )
  {
    unsigned char tdata[512];
    int i;

    if( parm_count != 3 )
    {
      errno = EINVAL;
      return -1;
    }

    memset( tdata, 0, sizeof( tdata ) );

    i = 0;
    while( i < 512 )
    {
      tdata[ i++ ] = ( parms[2] & 0x000000FF );
      tdata[ i++ ] = ( parms[2] & 0x0000FF00 ) >> 8;
      tdata[ i++ ] = ( parms[2] & 0x00FF0000 ) >> 16;
      tdata[ i++ ] = ( parms[2] & 0xFF000000 ) >> 24;
    }

    if( exec_cmd_scsi( drive, CMD_SCSI_WRITESAME, RW_WRITE, tdata, sizeof( tdata ), parms, 2, timeout ) )
      return -1;

    return 0;
  }
  else if( command == CMD_TRIM )
  {
    /*
      parms[0] = LBA;
      parms[1] = count;
    */
    __u64 tmpParm[1];
    unsigned int i, j;
    unsigned int chunk_size;
    long long unsigned pos;
    long long unsigned length;
    long long unsigned tmp;
    unsigned char tdata[512];

    if( parm_count != 2 )
    {
      errno = EINVAL;
      return -1;
    }

    memset( tdata, 0, sizeof( tdata ) );

    length = parms[1];
    if( length == 0 )
      length = drive->drive_info.LBACount - parms[0];

    pos = parms[0];
    chunk_size = drive->drive_info.maxUnmapLBACount;

    while( length )
    {
      j = 0;
      i = 8;
      while( ( j < 1/*drive->drive_info.maxUnmapDescriptorCount*/ ) && ( i + 16 ) < 512 )
      {
        tmp = min( length, chunk_size );

        tdata[ i++ ] = ( pos & 0xFF00000000000000 ) >> 56;
        tdata[ i++ ] = ( pos & 0x00FF000000000000 ) >> 48;
        tdata[ i++ ] = ( pos & 0x0000FF0000000000 ) >> 40;
        tdata[ i++ ] = ( pos & 0x000000FF00000000 ) >> 32;
        tdata[ i++ ] = ( pos & 0x00000000FF000000 ) >> 24;
        tdata[ i++ ] = ( pos & 0x0000000000FF0000 ) >> 16;
        tdata[ i++ ] = ( pos & 0x000000000000FF00 ) >> 8;
        tdata[ i++ ] = ( pos & 0x00000000000000FF );

        tdata[ i++ ] = ( tmp & 0xFF000000 ) >> 24;
        tdata[ i++ ] = ( tmp & 0x00FF0000 ) >> 16;
        tdata[ i++ ] = ( tmp & 0x0000FF00 ) >> 8;
        tdata[ i++ ] = ( tmp & 0x000000FF );
        i += 4;

        if( verbose >= 4 )
          fprintf( stderr, "SCSI: Unmaping from %llu for %llu \n", pos, tmp );

        if( length <= chunk_size )
        {
          length = 0;
          break;
        }

        length -= chunk_size;
        pos += chunk_size;
        j++;
      }

      tdata[0] = ( ( i - 2 ) & 0xFF00 ) >> 8;
      tdata[1] = ( ( i - 2 ) & 0x00FF );
      tdata[2] = ( ( i - 8 ) & 0xFF00 ) >> 8;
      tdata[3] = ( ( i - 8 ) & 0x00FF );

      tmpParm[0] = i;

      if( exec_cmd_scsi( drive, CMD_SCSI_TRIM, RW_WRITE, tdata, i, tmpParm, 1, timeout ) )
        return -1;
    }
    return 0;
  }

  if( load_cdb_scsi( cdb, command, rw, parms, parm_count ) )
    return -1;

  if( verbose >= 4 )
    dump_bytes( "SCSI: cdb out", cdb, sizeof( cdb ) );

  if( drive->driver_cmd( drive, rw, cdb, sizeof( cdb ), data, data_len, timeout ) )
    return -1;

  if( verbose >= 4 )
    dump_bytes( "SCSI: cdb in", cdb, sizeof( cdb ) );

  /*
  if( ( cdb[0] == 0x09 ) && ( cdb[1] == 0x0c ) )  <--- this is for ata, how do we know for scsi?
  {
    if( unload_cdb_scsi( cdb, command, rw, parms, parm_count  ) )
      return -1;
  }
  else
  {
  */
    memset( parms, 0, parm_count );
  //}

  return 0;
}

void print_sense( const char *label, const unsigned char *sb )
{
  if( !( ( sb[0] == 0x70 ) || ( sb[0] == 0x71 ) ) ) // fixed format see 2.4 of SCSI Spec
    return;

  fprintf( stderr, "%s: sence key=0x%02x asc=0x%02x ascq=0x%02x\n", label, sb[2] & 0x0f, sb[12], sb[13] );

  switch( sb[2] & 0x0f )
  {
    case 0x00:
      break;
    case 0x01:
      fprintf( stderr, "      Recovered Error\n" );
      break;
    case 0x02:
      fprintf( stderr, "      Not Ready\n" );
      break;
    case 0x03:
      fprintf( stderr, "      Medium Error\n" );
      break;
    case 0x04:
      fprintf( stderr, "      Hardware Error\n" );
      break;
    case 0x05:
      fprintf( stderr, "      Illegal Request\n" );
      break;
    case 0x06:
      fprintf( stderr, "      Unit Attention\n" );
      break;
    case 0x07:
      fprintf( stderr, "      Data Protect\n" );
      break;
    case 0x08:
      fprintf( stderr, "      Blank Check\n" );
      break;
    case 0x0b:
      fprintf( stderr, "      Aborted Command\n" );
      break;
    case 0x0e:
      fprintf( stderr, "      Misscompare\n" );
      break;
    case 0x0f:
      fprintf( stderr, "      Complete\n" );
      break;
  }

  // see http://oss.sgi.com/LDP/HOWTO/SCSI-Programming-HOWTO-22.html#sec-sensecodes
  //  or http://www.t10.org/lists/asc-num.txt
  if( ( sb[12] == 0x20 ) && ( sb[13] == 0x00 ) )
    fprintf( stderr, "         INVALID COMMAND OPERATION CODE\n" );

  if( ( sb[12] == 0x24 ) && ( sb[13] == 0x00 ) )
    fprintf( stderr, "         INVALID FIELD IN CDB\n" );

  if( ( sb[12] == 0x29 ) && ( sb[13] == 0x02 ) )
    fprintf( stderr, "         SCSI BUS RESET OCCURRED\n" );
}

void dump_bytes( const char *label, const unsigned char *p, const int len )
{
  int i, j;

  if( label )
    fprintf( stderr, "%s:\n", label );
  for( i = 0; i < len; i += 20 )
  {
    fprintf( stderr, "    %4i: ", i );
    for( j = 0; ( j < 20 ) && ( ( i + j ) < len ); j++ )
      fprintf( stderr, "%02x ", p[i + j] );
    for( ; j < 20; j++ )
      fprintf( stderr, "   " );
    fprintf( stderr, "     |" );
    for( j = 0; ( j < 20 ) && ( ( i + j ) < len ); j++ )
    {
      if( isprint( p[i + j] ) )
        fprintf( stderr, "%c", p[i + j] );
      else
        fprintf( stderr, " " );
    }
    for( ; j < 20; j++ )
      fprintf( stderr, " " );
    fprintf( stderr, "|\n" );
  }
}


char *scsi_descriptor_desc( unsigned int descriptor ) //http://www.t10.org/lists/stds.htm
{  // also in encinfo.c
  switch( descriptor )
  {
    case( 0x0000 ):
      return "Version Descriptor Not Supported or No Standard Identified";
    case( 0x0020 ):
      return "SAM (no version claimed)";
    case( 0x003B ):
      return "SAM T10/0994-D revision 18";
    case( 0x003C ):
      return "SAM ANSI INCITS 270-1996";
    case( 0x0040 ):
      return "SAM-2 (no version claimed)";
    case( 0x0054 ):
      return "SAM-2 T10/1157-D revision 23";
    case( 0x0055 ):
      return "SAM-2 T10/1157-D revision 24";
    case( 0x005C ):
      return "SAM-2 ANSI INCITS 366-2003";
    case( 0x0060 ):
      return "SAM-3 (no version claimed)";
    case( 0x0062 ):
      return "SAM-3 T10/1561-D revision 7";
    case( 0x0075 ):
      return "SAM-3 T10/1561-D revision 13";
    case( 0x0076 ):
      return "SAM-3 T10/1561-D revision 14";
    case( 0x0077 ):
      return "SAM-3 ANSI INCITS 402-200x";
    case( 0x0080 ):
      return "SAM-4 (no version claimed)";
    case( 0x0120 ):
      return "SPC (no version claimed)";
    case( 0x013B ):
      return "SPC T10/0995-D revision 11a";
    case( 0x013C ):
      return "SPC ANSI INCITS 301-1997";
    case( 0x0140 ):
      return "MMC (no version claimed)";
    case( 0x015B ):
      return "MMC T10/1048-D revision 10a";
    case( 0x015C ):
      return "MMC ANSI INCITS 304-1997";
    case( 0x0160 ):
      return "SCC (no version claimed)";
    case( 0x017B ):
      return "SCC T10/1047-D revision 06c";
    case( 0x017C ):
      return "SCC ANSI INCITS 276-1997";
    case( 0x0180 ):
      return "SBC (no version claimed)";
    case( 0x019B ):
      return "SBC T10/0996-D revision 08c";
    case( 0x019C ):
      return "SBC ANSI INCITS 306-1998";
    case( 0x01A0 ):
      return "SMC (no version claimed)";
    case( 0x01BB ):
      return "SMC T10/0999-D revision 10a";
    case( 0x01BC ):
      return "SMC ANSI INCITS 314-1998";
    case( 0x01C0 ):
      return "SES (no version claimed)";
    case( 0x01DB ):
      return "SES T10/1212-D revision 08b";
    case( 0x01DC ):
      return "SES ANSI INCITS 305-1998";
    case( 0x01DD ):
      return "SES T10/1212 revision 08b w/ Amendment ANSI INCITS.305/AM1-2000";
    case( 0x01DE ):
      return "SES ANSI INCITS 305-1998 w/ Amendment ANSI INCITS.305/AM1-2000";
    case( 0x01E0 ):
      return "SCC-2 (no version claimed)";
    case( 0x01FB ):
      return "SCC-2 T10/1125-D revision 4";
    case( 0x01FC ):
      return "SCC-2 ANSI INCITS 318-1998";
    case( 0x0200 ):
      return "SSC (no version claimed)";
    case( 0x0201 ):
      return "SSC T10/0997-D revision 17";
    case( 0x0207 ):
      return "SSC T10/0997-D revision 22";
    case( 0x021C ):
      return "SSC ANSI INCITS 335-2000";
    case( 0x0220 ):
      return "RBC (no version claimed)";
    case( 0x0238 ):
      return "RBC T10/1240-D revision 10a";
    case( 0x023C ):
      return "RBC ANSI INCITS 330-2000";
    case( 0x0240 ):
      return "MMC-2 (no version claimed)";
    case( 0x0255 ):
      return "MMC-2 T10/1228-D revision 11";
    case( 0x025B ):
      return "MMC-2 T10/1228-D revision 11a";
    case( 0x025C ):
      return "MMC-2 ANSI INCITS 333-2000";
    case( 0x0260 ):
      return "SPC-2 (no version claimed)";
    case( 0x0267 ):
      return "SPC-2 T10/1236-D revision 12";
    case( 0x0269 ):
      return "SPC-2 T10/1236-D revision 18";
    case( 0x0275 ):
      return "SPC-2 T10/1236-D revision 19";
    case( 0x0276 ):
      return "SPC-2 T10/1236-D revision 20";
    case( 0x0277 ):
      return "SPC-2 ANSI INCITS 351-2001";
    case( 0x0280 ):
      return "OCRW (no version claimed)";
    case( 0x029E ):
      return "OCRW ISO/IEC 14776-381";
    case( 0x02A0 ):
      return "MMC-3 (no version claimed)";
    case( 0x02B5 ):
      return "MMC-3 T10/1363-D revision 9";
    case( 0x02B6 ):
      return "MMC-3 T10/1363-D revision 10g";
    case( 0x02B8 ):
      return "MMC-3 ANSI INCITS 360-2002";
    case( 0x02E0 ):
      return "SMC-2 (no version claimed)";
    case( 0x02F5 ):
      return "SMC-2 T10/1383-D revision 5";
    case( 0x02FC ):
      return "SMC-2 T10/1383-D revision 6";
    case( 0x02FD ):
      return "SMC-2 T10/1383-D revision 7";
    case( 0x02FE ):
      return "SMC-2 ANSI INCITS 382-2004";
    case( 0x0300 ):
      return "SPC-3 (no version claimed)";
    case( 0x0301 ):
      return "SPC-3 T10/1416-D revision 7";
    case( 0x0307 ):
      return "SPC-3 T10/1416-D revision 21";
    case( 0x030F ):
      return "SPC-3 T10/1416-D revision 22";
    case( 0x0320 ):
      return "SBC-2 (no version claimed)";
    case( 0x0322 ):
      return "SBC-2 T10/1417-D revision 5a";
    case( 0x0324 ):
      return "SBC-2 T10/1417-D revision 15";
    case( 0x033B ):
      return "SBC-2 T10/1417-D revision 16";
    case( 0x033D ):
      return "SBC-2 ANSI INCITS 405-200x";
    case( 0x0340 ):
      return "OSD (no version claimed)";
    case( 0x0341 ):
      return "OSD T10/1355-D revision 0";
    case( 0x0342 ):
      return "OSD T10/1355-D revision 7a";
    case( 0x0343 ):
      return "OSD T10/1355-D revision 8";
    case( 0x0344 ):
      return "OSD T10/1355-D revision 9";
    case( 0x0355 ):
      return "OSD T10/1355-D revision 10";
    case( 0x0356 ):
      return "OSD ANSI INCITS 400-2004";
    case( 0x0360 ):
      return "SSC-2 (no version claimed)";
    case( 0x0374 ):
      return "SSC-2 T10/1434-D revision 7";
    case( 0x0375 ):
      return "SSC-2 T10/1434-D revision 9";
    case( 0x037D ):
      return "SSC-2 ANSI INCITS 380-2003";
    case( 0x0380 ):
      return "BCC (no version claimed)";
    case( 0x03A0 ):
      return "MMC-4 (no version claimed)";
    case( 0x03B0 ):
      return "MMC-4 T10/1545-D revision 5";
    case( 0x03BD ):
      return "MMC-4 T10/1545-D revision 3";
    case( 0x03BE ):
      return "MMC-4 T10/1545-D revision 3d";
    case( 0x03BF ):
      return "MMC-4 ANSI INCITS 401-200x";
    case( 0x03C0 ):
      return "ADC (no version claimed)";
    case( 0x03D5 ):
      return "ADC T10/1558-D revision 6";
    case( 0x03D6 ):
      return "ADC T10/1558-D revision 7";
    case( 0x03D7 ):
      return "ADC ANSI INCITS 403-200x";
    case( 0x03E0 ):
      return "SES-2 (no version claimed)";
    case( 0x0400 ):
      return "SSC-3 (no version claimed)";
    case( 0x0420 ):
      return "MMC-5 (no version claimed)";
    case( 0x0440 ):
      return "OSD-2 (no version claimed)";
    case( 0x0460 ):
      return "SPC-4 (no version claimed)";
    case( 0x0480 ):
      return "SMC-3 (no version claimed)";
    case( 0x04A0 ):
      return "ADC-2 (no version claimed)";
    case( 0x0820 ):
      return "SSA-TL2 (no version claimed)";
    case( 0x083B ):
      return "SSA-TL2 T10.1/1147-D revision 05b";
    case( 0x083C ):
      return "SSA-TL2 ANSI INCITS 308-1998";
    case( 0x0840 ):
      return "SSA-TL1 (no version claimed)";
    case( 0x085B ):
      return "SSA-TL1 T10.1/0989-D revision 10b";
    case( 0x085C ):
      return "SSA-TL1 ANSI INCITS 295-1996";
    case( 0x0860 ):
      return "SSA-S3P (no version claimed)";
    case( 0x087B ):
      return "SSA-S3P T10.1/1051-D revision 05b";
    case( 0x087C ):
      return "SSA-S3P ANSI INCITS 309-1998";
    case( 0x0880 ):
      return "SSA-S2P (no version claimed)";
    case( 0x089B ):
      return "SSA-S2P T10.1/1121-D revision 07b";
    case( 0x089C ):
      return "SSA-S2P ANSI INCITS 294-1996";
    case( 0x08A0 ):
      return "SIP (no version claimed)";
    case( 0x08BB ):
      return "SIP T10/0856-D revision 10";
    case( 0x08BC ):
      return "SIP ANSI INCITS 292-1997";
    case( 0x08C0 ):
      return "FCP (no version claimed)";
    case( 0x08DB ):
      return "FCP T10/0993-D revision 12";
    case( 0x08DC ):
      return "FCP ANSI INCITS 269-1996";
    case( 0x08E0 ):
      return "SBP-2 (no version claimed)";
    case( 0x08FB ):
      return "SBP-2 T10/1155-D revision 4";
    case( 0x08FC ):
      return "SBP-2 ANSI INCITS 325-1999";
    case( 0x0900 ):
      return "FCP-2 (no version claimed)";
    case( 0x0901 ):
      return "FCP-2 T10/1144-D revision 4";
    case( 0x0915 ):
      return "FCP-2 T10/1144-D revision 7";
    case( 0x0916 ):
      return "FCP-2 T10/1144-D revision 7a";
    case( 0x0917 ):
      return "FCP-2 ANSI INCITS 350-2003";
    case( 0x0918 ):
      return "FCP-2 T10/1144-D revision 8";
    case( 0x0920 ):
      return "SST (no version claimed)";
    case( 0x0935 ):
      return "SST T10/1380-D revision 8b";
    case( 0x0940 ):
      return "SRP (no version claimed)";
    case( 0x0954 ):
      return "SRP T10/1415-D revision 10";
    case( 0x0955 ):
      return "SRP T10/1415-D revision 16a";
    case( 0x095C ):
      return "SRP ANSI INCITS 365-2002";
    case( 0x0960 ):
      return "iSCSI (no version claimed)";
    case( 0x0980 ):
      return "SBP-3 (no version claimed)";
    case( 0x0982 ):
      return "SBP-3 T10/1467-D revision 1f";
    case( 0x0994 ):
      return "SBP-3 T10/1467-D revision 3";
    case( 0x099A ):
      return "SBP-3 T10/1467-D revision 4";
    case( 0x099B ):
      return "SBP-3 T10/1467-D revision 5";
    case( 0x099C ):
      return "SBP-3 ANSI INCITS 375-2004";
    case( 0x09C0 ):
      return "ADP (no version claimed)";
    case( 0x09E0 ):
      return "ADT (no version claimed)";
    case( 0x09F9 ):
      return "ADT T10/1557-D revision 11";
    case( 0x09FA ):
      return "ADT T10/1557-D revision 14";
    case( 0x09FD ):
      return "ADT ANSI INCITS 406-200x";
    case( 0x0A00 ):
      return "FCP-3 (no version claimed)";
    case( 0x0A20 ):
      return "ADT-2 (no version claimed)";
    case( 0x0AA0 ):
      return "SPI (no version claimed)";
    case( 0x0AB9 ):
      return "SPI T10/0855-D revision 15a";
    case( 0x0ABA ):
      return "SPI ANSI INCITS 253-1995";
    case( 0x0ABB ):
      return "SPI T10/0855-D revision 15a with SPI Amnd revision 3a";
    case( 0x0ABC ):
      return "SPI ANSI INCITS 253-1995 with SPI Amnd ANSI INCITS 253/AM1-1998";
    case( 0x0AC0 ):
      return "Fast-20 (no version claimed)";
    case( 0x0ADB ):
      return "Fast-20 T10/1071 revision 6";
    case( 0x0ADC ):
      return "Fast-20 ANSI INCITS 277-1996";
    case( 0x0AE0 ):
      return "SPI-2 (no version claimed)";
    case( 0x0AFB ):
      return "SPI-2 T10/1142-D revision 20b";
    case( 0x0AFC ):
      return "SPI-2 ANSI INCITS 302-1999";
    case( 0x0B00 ):
      return "SPI-3 (no version claimed)";
    case( 0x0B18 ):
      return "SPI-3 T10/1302-D revision 10";
    case( 0x0B19 ):
      return "SPI-3 T10/1302-D revision 13a";
    case( 0x0B1A ):
      return "SPI-3 T10/1302-D revision 14";
    case( 0x0B1C ):
      return "SPI-3 ANSI INCITS 336-2000";
    case( 0x0B20 ):
      return "EPI (no version claimed)";
    case( 0x0B3B ):
      return "EPI T10/1134 revision 16";
    case( 0x0B3C ):
      return "EPI ANSI INCITS TR-23 1999";
    case( 0x0B40 ):
      return "SPI-4 (no version claimed)";
    case( 0x0B54 ):
      return "SPI-4 T10/1365-D revision 7";
    case( 0x0B55 ):
      return "SPI-4 T10/1365-D revision 9";
    case( 0x0B56 ):
      return "SPI-4 ANSI INCITS 362-2002";
    case( 0x0B59 ):
      return "SPI-4 T10/1365-D revision 10";
    case( 0x0B60 ):
      return "SPI-5 (no version claimed)";
    case( 0x0B79 ):
      return "SPI-5 T10/1525-D revision 3";
    case( 0x0B7A ):
      return "SPI-5 T10/1525-D revision 5";
    case( 0x0B7B ):
      return "SPI-5 T10/1525-D revision 6";
    case( 0x0B7C ):
      return "SPI-5 ANSI INCITS 367-2003";
    case( 0x0BE0 ):
      return "SAS (no version claimed)";
    case( 0x0BE1 ):
      return "SAS T10/1562-D revision 1";
    case( 0x0BF5 ):
      return "SAS T10/1562-D revision 3";
    case( 0x0BFA ):
      return "SAS T10/1562-D revision 4";
    case( 0x0BFB ):
      return "SAS T10/1562-D revision 4";
    case( 0x0BFC ):
      return "SAS T10/1562-D revision 5";
    case( 0x0BFD ):
      return "SAS ANSI INCITS 376-2003";
    case( 0x0C00 ):
      return "SAS-1.1 (no version claimed)";
    case( 0x0C07 ):
      return "SAS-1.1 T10/1601-D revision 9";
    case( 0x0D20 ):
      return "FC-PH (no version claimed)";
    case( 0x0D3B ):
      return "FC-PH ANSI INCITS 230-1994";
    case( 0x0D3C ):
      return "FC-PH ANSI INCITS 230-1994 with Amnd 1 ANSI INCITS 230/AM1-1996";
    case( 0x0D40 ):
      return "FC-AL (no version claimed)";
    case( 0x0D5C ):
      return "FC-AL ANSI INCITS 272-1996";
    case( 0x0D60 ):
      return "FC-AL-2 (no version claimed)";
    case( 0x0D61 ):
      return "FC-AL-2 T11/1133-D revision 7";
    case( 0x0D7C ):
      return "FC-AL-2 ANSI INCITS 332-1999";
    case( 0x0D7D ):
      return "FC-AL-2 ANSI INCITS 332-1999 with Amnd 1 AM1-2002";
    case( 0x0D80 ):
      return "FC-PH-3 (no version claimed)";
    case( 0x0D9C ):
      return "FC-PH-3 ANSI INCITS 303-1998";
    case( 0x0DA0 ):
      return "FC-FS (no version claimed)";
    case( 0x0DB7 ):
      return "FC-FS T11/1331-D revision 1.2";
    case( 0x0DB8 ):
      return "FC-FS T11/1331-D revision 1.7";
    case( 0x0DBC ):
      return "FC-FS ANSI INCITS 373-2003";
    case( 0x0DC0 ):
      return "FC-PI (no version claimed)";
    case( 0x0DDC ):
      return "FC-PI ANSI INCITS 352-2002";
    case( 0x0DE0 ):
      return "FC-PI-2 (no version claimed)";
    case( 0x0DE2 ):
      return "FC-PI-2 T11/1506-D revision 5";
    case( 0x0E00 ):
      return "FC-FS-2 (no version claimed)";
    case( 0x0E20 ):
      return "FC-LS (no version claimed)";
    case( 0x0E40 ):
      return "FC-SP (no version claimed)";
    case( 0x0E42 ):
      return "FC-SP T11/1570-D revision 1.6";
    case( 0x12E0 ):
      return "FC-DA (no version claimed)";
    case( 0x12E2 ):
      return "FC-DA T11/1513-DT revision 3.1";
    case( 0x1300 ):
      return "FC-Tape (no version claimed)";
    case( 0x1301 ):
      return "FC-Tape T11/1315 revision 1.16";
    case( 0x131B ):
      return "FC-Tape T11/1315 revision 1.17";
    case( 0x131C ):
      return "FC-Tape ANSI INCITS TR-24 1999";
    case( 0x1320 ):
      return "FC-FLA (no version claimed)";
    case( 0x133B ):
      return "FC-FLA T11/1235 revision 7";
    case( 0x133C ):
      return "FC-FLA ANSI INCITS TR-20 1998";
    case( 0x1340 ):
      return "FC-PLDA (no version claimed)";
    case( 0x135B ):
      return "FC-PLDA T11/1162 revision 2.1";
    case( 0x135C ):
      return "FC-PLDA ANSI INCITS TR-19 1998";
    case( 0x1360 ):
      return "SSA-PH2 (no version claimed)";
    case( 0x137B ):
      return "SSA-PH2 T10.1/1145-D revision 09c";
    case( 0x137C ):
      return "SSA-PH2 ANSI INCITS 293-1996";
    case( 0x1380 ):
      return "SSA-PH3 (no version claimed)";
    case( 0x139B ):
      return "SSA-PH3 T10.1/1146-D revision 05b";
    case( 0x139C ):
      return "SSA-PH3 ANSI INCITS 307-1998";
    case( 0x14A0 ):
      return "IEEE 1394 (no version claimed)";
    case( 0x14BD ):
      return "ANSI IEEE 1394-1995";
    case( 0x14C0 ):
      return "IEEE 1394a (no version claimed)";
    case( 0x14E0 ):
      return "IEEE 1394b (no version claimed)";
    case( 0x15E0 ):
      return "ATA/ATAPI-6 (no version claimed)";
    case( 0x15FD ):
      return "ATA/ATAPI-6 ANSI INCITS 361-2002";
    case( 0x1600 ):
      return "ATA/ATAPI-7 (no version claimed)";
    case( 0x1602 ):
      return "ATA/ATAPI-7 T13/1532-D revision 3";
    case( 0x1728 ):
      return "Universal Serial Bus Specification, Revision 1.1";
    case( 0x1729 ):
      return "Universal Serial Bus Specification, Revision 2";
    case( 0x1730 ):
      return "USB Mass Storage Class Bulk-Only Transport, Revision 1";
    case( 0x1EA0 ):
      return "SAT (no version claimed)";

    default:
      return "Unknown";
  }
}
