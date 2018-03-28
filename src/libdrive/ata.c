#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <linux/types.h>

#include "ata.h"
#include "cdb.h"
#include "scsi.h"
#include "disk.h"

extern int verbose;

#define min(a,b) (((a) < (b)) ? (a) : (b))

static inline int is_ext( const __u8 ata_op )
{
  switch( ata_op )
  {
    case ATA_OP_READ_NATIVE_MAX_EXT:
    case ATA_OP_READ_PIO_EXT:
    case ATA_OP_READ_DMA_EXT:
    case ATA_OP_WRITE_PIO_EXT:
    case ATA_OP_WRITE_DMA_EXT:
      return 1;

    default:
      return 0;
  }
}

static inline int is_dma( const __u8 ata_op )
{
  switch( ata_op )
  {
    case ATA_OP_READ_DMA:
    case ATA_OP_READ_DMA_EXT:
    case ATA_OP_WRITE_DMA:
    case ATA_OP_WRITE_DMA_EXT:
    case ATA_OP_DATA_SET_MANGEMENT:
      return 1;

    default:
      return 0;
  }
}

static inline int is_dma_queued( const __u8 ata_op )
{
  switch( ata_op )
  {
    case ATA_OP_READ_DMA_QUEUED:
    case ATA_OP_READ_DMA_QUEUED_EXT:
    case ATA_OP_WRITE_DMA_QUEUED:
    case ATA_OP_WRITE_DMA_QUEUED_EXT:
      return 1;

    default:
      return 0;
  }
}

// from hdparm - identify.c - print_ascii
static void atastring_2_ascii( const __u16 *ata_field, const int field_length, char *buff )
{
  int outPos = 0;
  int i;
  char cl;

  // find first non-space
  for( i = 0; i < field_length; i++ )
  {
    if( ( (char) 0x00ff & ( ata_field[i] >> 8 ) ) != ' ' )
      break;
    if( ( cl = (char) 0x00ff & ata_field[i] ) != ' ' )
    {
      if( cl != '\0' )
        buff[outPos++] = cl;
      i++;
      break;
    }
  }

  // copy the rest
  for( ; i < field_length; i++ )
  {
    // some older devices have NULLs
    cl = ata_field[i] >> 8;
    if( cl )
      buff[outPos++] = cl;
    cl = ata_field[i];
    if( cl )
      buff[outPos++] = cl;
  }

  buff[outPos] = '\0';
}

static int get_tf( struct ata_tf *tf, const enum cdb_command command, int ext, __u64 parms[], const unsigned int parm_count )
{
  memset( tf, 0, sizeof( *tf ) );

  switch( command )
  {
    case CMD_IDENTIFY:
      tf->command = ATA_OP_IDENTIFY;
      tf->lob.nsect = 1;
      return 0;

    case CMD_RESET:
      tf->command = ATA_OP_RESET;
      return 0;

    case CMD_SLEEP:
      tf->command = ATA_OP_SLEEP;
      return 0;

    case CMD_STANDBY:
      tf->command = ATA_OP_STANDBY;
      return 0;

    case CMD_IDLE:
      tf->command = ATA_OP_IDLE;
      tf->lob.nsect = 0; // Standby time disabled
      return 0;

    case CMD_GET_POWER_STATE:
      tf->command = ATA_OP_CHECK_POWER_MODE;
      tf->checkCond = 1;
      return 0;

    case CMD_SMART_ENABLE:
      tf->command = ATA_OP_SMART;
      tf->lob.feat = SMART_OP_ENABLE;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_SMART_DISABLE:
      tf->command = ATA_OP_SMART;
      tf->lob.feat = SMART_OP_DISABLE;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_ATA_SMART_ATTRIBS_VALUES:
      tf->command = ATA_OP_SMART;
      tf->lob.nsect = 0x01;
      tf->lob.feat = SMART_OP_READ_VALUES;
      tf->lob.lbal = 0x00;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_ATA_SMART_ATTRIBS_THRESHOLDS:
      tf->command = ATA_OP_SMART;
      tf->lob.nsect = 0x01;
      tf->lob.feat = SMART_OP_READ_THRESHOLDS;
      tf->lob.lbal = 0x01; // some things want this to be 1?
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_SMART_STATUS:
      tf->command = ATA_OP_SMART;
      tf->lob.nsect = 0x00;
      tf->lob.feat = SMART_OP_GET_STATUS;
      tf->lob.lbal = 0x00;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      tf->checkCond = 1;
      *parms = STATUS_DRIVE_GOOD; // incase there is no return CDB
      return 0;

    case CMD_ATA_SMART_LOG_COUNT:
      tf->command = ATA_OP_SMART;
      tf->lob.nsect = 0x01;
      tf->lob.feat = SMART_OP_READ_LOG;
      tf->lob.lbal = ATA_LOG_SMART_ERR_SUMMARY;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_GET_MAXLBA:
      tf->dev = ATA_USING_LBA; // Needed for generic SGIO
      if( ext )
        tf->command = ATA_OP_READ_NATIVE_MAX_EXT;
      else
        tf->command = ATA_OP_READ_NATIVE_MAX;
      tf->checkCond = 1;
      *parms = 0; // incase there is no return CDB
      return 0;

    case CMD_ATA_WRITESAME:
      tf->command = ATA_OP_SMART;
      tf->lob.nsect = 0x01;
      tf->lob.feat = SMART_OP_WRITE_LOG;
      tf->lob.lbal = ATA_LOG_SCT_CMD;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_SCT_STATUS:
      tf->command = ATA_OP_SMART;
      tf->lob.nsect = 0x01;
      tf->lob.feat = SMART_OP_READ_LOG;
      tf->lob.lbal = ATA_LOG_SCT_CMD;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_ATA_TRIM:
      tf->command = ATA_OP_DATA_SET_MANGEMENT;
      tf->lob.nsect = 0x01; // one 512 byte block
      tf->lob.feat = 0x01;
      tf->dev = ATA_USING_LBA;
      return 0;

    case CMD_ATA_SELF_TEST_STATUS:
      tf->command = ATA_OP_SMART;
      tf->lob.nsect = 0x01;
      tf->lob.feat = SMART_OP_READ_VALUES;
      tf->lob.lbal = 0x00;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;

    case CMD_ATA_SELF_TEST_START:
      if( parm_count != 1 )
      {
        errno = EINVAL;
        return -1;
      }

      tf->command = ATA_OP_SMART;
      tf->lob.feat = SMART_OP_OFFLINE_IMMEDIATE;
      tf->lob.lbal = *parms;
      tf->lob.lbam = SMART_OP_LBAM;
      tf->lob.lbah = SMART_OP_LBAH;
      return 0;


    case CMD_READ:
/*
  parms[0] = LBA;
  parms[1] = count;
  parms[2] = DMA;
  parms[3] = FUA; // ignored
*/
      if( parm_count != 4 )
      {
        errno = EINVAL;
        return -1;
      }

      tf->dev = ATA_USING_LBA;
      tf->lob.lbah = ( ( parms[0] & ( 0xff << 16  ) ) >> 16 );
      tf->lob.lbam = ( ( parms[0] & ( 0xff << 8   ) ) >> 8 );
      tf->lob.lbal = ( ( parms[0] & ( 0xff        ) ) );

      if( ext )
      {
        if( parms[1] > 0xffff )
        {
          errno = EINVAL;
          return -1;
        }

        tf->lob.nsect =   parms[1] & 0x00ff;
        tf->hob.nsect = ( parms[1] & 0xff00 ) >> 8;

        tf->hob.lbah = ( ( parms[0] & ( (__u64) 0xff << 40 ) ) >> 40 );
        tf->hob.lbam = ( ( parms[0] & ( (__u64) 0xff << 32 ) ) >> 32 );
        tf->hob.lbal = ( ( parms[0] & ( (__u64) 0xff << 24 ) ) >> 24 );

        if( parms[2] ) // if parms[3] (FUA) do something
          tf->command = ATA_OP_READ_DMA_EXT;
        else
          tf->command = ATA_OP_READ_PIO_EXT;
      }
      else
      {
        if( parms[1] > 0xff )
        {
          errno = EINVAL;
          return -1;
        }

        tf->lob.nsect = parms[1];

        if( parms[2] )
          tf->command = ATA_OP_READ_DMA;
        else
          tf->command = ATA_OP_READ_PIO;
      }

      return 0;

    case CMD_WRITE:
/*
  parms[0] = LBA;
  parms[1] = count;
  parms[2] = DMA;
  parms[3] = FUA; // ignored
*/
      if( parm_count != 4 )
      {
        errno = EINVAL;
        return -1;
      }

      tf->dev = ATA_USING_LBA;
      tf->lob.lbah = ( ( parms[0] & ( 0xff << 16  ) ) >> 16 );
      tf->lob.lbam = ( ( parms[0] & ( 0xff << 8   ) ) >> 8 );
      tf->lob.lbal = ( ( parms[0] & ( 0xff        ) ) );

      if( ext )
      {
        if( parms[1] > 0xffff )
        {
          errno = EINVAL;
          return -1;
        }

        tf->lob.nsect =   parms[1] & 0x00ff;
        tf->hob.nsect = ( parms[1] & 0xff00 ) >> 8;

        tf->hob.lbah = ( ( parms[0] & ( (__u64) 0xff << 40 ) ) >> 40 );
        tf->hob.lbam = ( ( parms[0] & ( (__u64) 0xff << 32 ) ) >> 32 );
        tf->hob.lbal = ( ( parms[0] & ( (__u64) 0xff << 24 ) ) >> 24 );

        if( parms[2] )
          tf->command = ATA_OP_WRITE_DMA_EXT;
        else
          tf->command = ATA_OP_WRITE_PIO_EXT;
      }
      else
      {
        if( parms[1] > 0xff )
        {
          errno = EINVAL;
          return -1;
        }

        tf->lob.nsect = parms[1];

        if( parms[2] )
          tf->command = ATA_OP_WRITE_DMA;
        else
          tf->command = ATA_OP_WRITE_PIO;
      }

      return 0;

    case CMD_ENABLE_WRITE_CACHE:
      tf->command = ATA_OP_SETFEATURES;
      tf->lob.feat = 0x02;
      return 0;

    case CMD_DISABLE_WRITE_CACHE:
      tf->command = ATA_OP_SETFEATURES;
      tf->lob.feat = 0x82;
      return 0;

    default:
      errno = ENOSYS;
      return -1;
  }
}

static int parse_tf( struct ata_tf *tf, const enum cdb_command command, int ext, __u64 parms[], const unsigned int parm_count )
{
  switch( command )
  {
    case CMD_GET_MAXLBA:
      if( parm_count != 1 )
      {
        errno = EINVAL;
        return -1;
      }

      if( ext )
        *parms = ( ( ( __u64 ) ( ( tf->hob.lbah << 16 ) | ( tf->hob.lbam << 8 ) | tf->hob.lbal ) << 24 ) | ( ( tf->lob.lbah << 16 ) | ( tf->lob.lbam << 8 ) | tf->lob.lbal ) );
      else
        *parms = ( ( ( tf->dev & 0x0f ) << 24 ) | ( tf->lob.lbah << 16 ) | ( tf->lob.lbam << 8 ) | tf->lob.lbal );
      return 0;

    case CMD_SMART_STATUS:
      if( parm_count != 1 )
      {
        errno = EINVAL;
        return -1;
      }

      *parms = STATUS_DRIVE_GOOD;
      if( tf->status & 0x20 )
        *parms |= STATUS_DRIVE_FAULT;

      if( ( tf->lob.lbam == 0xf4 ) && ( tf->lob.lbah == 0x2c ) )
        *parms |= STATUS_THRESHOLD_EXCEEDED;
      else if ( ( tf->lob.lbam == 0x4f ) && ( tf->lob.lbah == 0xc2 ) )
        ; //ignore
      else
      {
        if( verbose >= 2 )
          fprintf( stderr, "Unknown SMART STATUS labm: 0x%x lbah: 0x%x\n", tf->lob.lbam, tf->lob.lbah );
        errno = EBADE;
        return -1;
      }

      return 0;

    case CMD_GET_POWER_STATE:
      if( parm_count != 1 )
      {
        errno = EINVAL;
        return -1;
      }

      *parms = POWER_UNKNOWN;

      switch( tf->lob.nsect )
      {
        case 0x00:
          *parms = POWER_STANDBY;
          break;
        case 0x40: // NV Cache active, active but spindle down
        case 0x41: // NV Cache active, active but spindle up
          *parms = POWER_ACTIVE;
          break;

        case 0x80:
          *parms = POWER_IDLE;
          break;

        case 0xFF: // Active or Idle
          *parms = POWER_ACTIVE;
          break;

        default:
          if( verbose >= 2 )
            fprintf( stderr, "Unknown Power State: 0x%x\n", tf->lob.nsect );
          errno = EBADE;
          return -1;
      }

      return 0;

    default:
      errno = EINVAL;
      return -1;
  }
  return -1;
}

// see ATA_Command_PassThrough from T10
// ATA_16 CDB:

// cdb[0]: ATA PASS THROUGH (16) SCSI command opcode byte (0x85)
// cdb[1]: multiple_count, protocol + extend
// cdb[2]: offline, ck_cond, t_dir, byte_block + t_length
// cdb[3]: features (15:8)
// cdb[4]: features (7:0)
// cdb[5]: sector_count (15:8)
// cdb[6]: sector_count (7:0)
// cdb[7]: lba_low (15:8)
// cdb[8]: lba_low (7:0)
// cdb[9]: lba_mid (15:8)
// cdb[10]: lba_mid (7:0)
// cdb[11]: lba_high (15:8)
// cdb[12]: lba_high (7:0)
// cdb[13]: device
// cdb[14]: (ata) command
// cdb[15]: control (SCSI, leave as zero)

static int load_cdb_ata( unsigned char cdb[ATA_CDB_LEN], const struct ata_tf *tf, const enum cdb_rw rw )
{
  memset( cdb, 0, ATA_CDB_LEN );

  if( rw == RW_READ )
  {
    if( is_dma( tf->command ) )
      cdb[1] =  SG_ATA_PROTO_UDMA_IN;
    else if( is_dma_queued ( tf->command ) )
      cdb[1] = SG_ATA_PROTO_DMA_QUEUED;
    else
      cdb[1] = SG_ATA_PROTO_PIO_IN;
  }
  else if( rw == RW_WRITE )
  {
    if( is_dma( tf->command ) )
      cdb[1] = SG_ATA_PROTO_UDMA_OUT;
    else if( is_dma_queued ( tf->command ) )
      cdb[1] = SG_ATA_PROTO_DMA_QUEUED;
    else
      cdb[1] = SG_ATA_PROTO_PIO_OUT;
  }
  else
  {
    cdb[1] = SG_ATA_PROTO_NON_DATA;
  }

  if( ( rw == RW_READ ) || ( rw == RW_WRITE ) )
  {
    cdb[2] = SG_CDB2_TLEN_NSECT | SG_CDB2_TLEN_SECTORS;
    cdb[2] |= ( rw == RW_WRITE ) ? SG_CDB2_TDIR_TO_DEV : SG_CDB2_TDIR_FROM_DEV;
  }

  if( tf->checkCond )
  {
    cdb[2] = SG_CDB2_CHECK_COND;
    //if( tf->command == 0xb0 )
    //  cdb[2] |=0x0c;
  }

  cdb[ 0] = ATA_16;
  cdb[ 4] = tf->lob.feat;
  cdb[ 6] = tf->lob.nsect;
  cdb[ 8] = tf->lob.lbal;
  cdb[10] = tf->lob.lbam;
  cdb[12] = tf->lob.lbah;
  cdb[13] = tf->dev;
  cdb[14] = tf->command;
  if( is_ext( tf->command ) )
  {
    cdb[ 1] |= SG_ATA_LBA48;
    cdb[ 3]  = tf->hob.feat;
    cdb[ 5]  = tf->hob.nsect;
    cdb[ 7]  = tf->hob.lbal;
    cdb[ 9]  = tf->hob.lbam;
    cdb[11]  = tf->hob.lbah;
  }

  return 0;
}

// ATA Return Descriptor (component of descriptor sense data)
// cdb[0]: descriptor code (0x9)
// cdb[1]: additional descriptor length (0xc)
// cdb[2]: extend (bit 0)
// cdb[3]: error
// cdb[4]: sector_count (15:8)
// cdb[5]: sector_count (7:0)
// cdb[6]: lba_low (15:8)
// cdb[7]: lba_low (7:0)
// cdb[8]: lba_mid (15:8)
// cdb[9]: lba_mid (7:0)
// cdb[10]: lba_high (15:8)
// cdb[11]: lba_high (7:0)
// cdb[12]: device
// cdb[13]: status

static int unload_cdb_ata( const unsigned char cdb[ATA_CDB_LEN], struct ata_tf *tf, __attribute__((unused)) const enum cdb_rw rw )
{
  memset( tf, 0, sizeof( *tf ) );

  tf->error     = cdb[ 3];
  tf->hob.nsect = cdb[ 4];
  tf->lob.nsect = cdb[ 5];
  tf->hob.lbal  = cdb[ 6];
  tf->lob.lbal  = cdb[ 7];
  tf->hob.lbam  = cdb[ 8];
  tf->lob.lbam  = cdb[ 9];
  tf->hob.lbah  = cdb[10];
  tf->lob.lbah  = cdb[11];
  tf->dev       = cdb[12];
  tf->status    = cdb[13];

  if( verbose >= 3 )
    fprintf( stderr, "ATA: stat=0x%02x err=0x%02x nsect=0x%02x lbal=0x%02x lbam=0x%02x lbah=0x%02x dev=0x%02x\n", tf->status, tf->error, tf->lob.nsect, tf->lob.lbal, tf->lob.lbam, tf->lob.lbah, tf->dev );

  if( tf->status & ( ATA_STAT_ERR | ATA_STAT_DRQ | ATA_STAT_FAULT ) )
  {
    if( verbose >= 3 )
      fprintf( stderr, "ATA: I/O error, ata_op=0x%02x ata_status=0x%02x ata_error=0x%02x\n", tf->command, tf->status, tf->error );
    errno = EIO;
    return -1;
  }

  return 0;
}

void loadInfoATA( struct device_handle *drive, const struct ata_identity *identity )
{
  memset( &drive->drive_info, 0, sizeof( drive->drive_info ) );

  atastring_2_ascii( identity->model_number, ATA_SERIAL_NUMBER_RAW_LEN, drive->drive_info.model );
  atastring_2_ascii( identity->serial_number, ATA_SERIAL_NUMBER_RAW_LEN, drive->drive_info.serial );
  atastring_2_ascii( identity->firmware_rev, ATA_FIRMWARE_REV_RAW_LEN, drive->drive_info.firmware_rev );

  if( identity->ata_major_version & 0x0100 )
    drive->drive_info.ATA_major_version = 8;
  else if( identity->ata_major_version & 0x0080 )
    drive->drive_info.ATA_major_version = 7;
  else if( identity->ata_major_version & 0x0040 )
    drive->drive_info.ATA_major_version = 6;
  else if( identity->ata_major_version & 0x0020 )
    drive->drive_info.ATA_major_version = 5;
  else if( identity->ata_major_version & 0x0010 )
    drive->drive_info.ATA_major_version = 4;
  else
    drive->drive_info.ATA_major_version = 0;

  drive->drive_info.ATA_minor_version = identity->ata_minor_version;

  drive->drive_info.SMARTSupport = ( ( identity->capabilities82 & 0x0001 ) != 0 );
  drive->drive_info.SMARTEnabled = ( ( identity->capabilities85 & 0x0001 ) != 0 );

  drive->drive_info.supportsLBA = ( ( identity->capabilities49 & 0x0200 ) != 0 );
  drive->drive_info.supportsDMA = ( ( identity->capabilities49 & 0x0100 ) != 0 );

  if( drive->drive_info.SMARTEnabled )
    drive->drive_info.supportsSelfTest = ( ( identity->capabilities84 & 0x0002 ) != 0 );

  drive->drive_info.supportsSETMAX = ( ( identity->capabilities83 & 0x0100 ) != 0 );

  drive->drive_info.supportsSCT = ( ( identity->sct_commands & 0x0001 ) != 0 );
  drive->drive_info.supportsWriteSame = ( ( identity->sct_commands & 0x0004 ) != 0 );

  drive->drive_info.supportsTrim = ( ( identity->data_set_mgnt & 0x0001 ) != 0 );

  if( ( identity->sector_size_info & 0xd000 ) == 0x5000 )
    drive->drive_info.LogicalSectorSize = ( ( ( ( __u64 ) identity->logical_sector_size[1] ) << 16 ) | identity->logical_sector_size[0] ) * 2; // this is the # of words, x2 to get bytes
  else
    drive->drive_info.LogicalSectorSize = 512;

  if( ( identity->sector_size_info & 0xe000 ) == 0x6000 )
    drive->drive_info.PhysicalSectorSize = ( pow( 2, ( identity->sector_size_info & 0x0007 ) ) * drive->drive_info.LogicalSectorSize );
  else
    drive->drive_info.PhysicalSectorSize = drive->drive_info.LogicalSectorSize;

  if( drive->driver == DRIVER_TYPE_IDE )
    drive->drive_info.bit48LBA = 0;  // subdriver dosen't support 48 bit stuffs
  else
    drive->drive_info.bit48LBA = ( ( identity->capabilities83 & 0x0400 ) != 0 ); // 48bit

  if( drive->drive_info.supportsLBA )
  {
    if( drive->drive_info.bit48LBA )
    {
      if( ( identity->capabilities69 & 0x0008 ) != 0 )
      {
        drive->drive_info.LBACount = ( ( ( __u64 ) identity->lbaextnded[2] ) << 32 ) | ( ( ( __u64 ) identity->lbaextnded[1] ) << 16 ) | identity->lbaextnded[0];
      }
      else
      {
        drive->drive_info.LBACount = ( ( ( __u64 ) identity->lba48[2] ) << 32 ) | ( ( ( __u64 ) identity->lba48[1] ) << 16 ) | identity->lba48[0];
      }
    }
    else
      drive->drive_info.LBACount = ( ( ( __u64 ) ( identity->lba28[1] & 0x0fff ) ) << 16 ) | identity->lba28[0];
  }

  if( ( identity->capabilities84 & 0x0100 ) ) // also 87 & 0100 "is a copy of 84 bit 8"
    drive->drive_info.WWN = ( ( ( __u64 ) ( identity->wwn[0] ) ) << 48 ) | ( ( ( __u64 ) identity->wwn[1] ) << 32 ) | ( ( ( __u64 ) identity->wwn[2] ) << 16 ) | identity->wwn[3];

  if( identity->nominal_media_rotation == 1 )
    drive->drive_info.isSSD = 1;
  else
    drive->drive_info.RPM = identity->nominal_media_rotation;
}

int exec_cmd_ata( struct device_handle *drive, const enum cdb_command command, const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], const unsigned int parm_count, const unsigned int timeout )
{
  struct ata_tf tf;
  __u8 cdb[ATA_CDB_LEN];

  if( command == CMD_SMART_ATTRIBS )
  {
    unsigned int i, j;
    unsigned char tdata[512];
    struct smart_attribs *wrkAttribs = data;
    struct ata_smart_attrib *attrib = (struct ata_smart_attrib *) &tdata[2];
    struct ata_smart_threshold *threshold = (struct ata_smart_threshold *) &tdata[2];

    if( data_len < sizeof( struct smart_attribs ) )
    {
      errno = EINVAL;
      return -1;
    }

    memset( data, 0, data_len );

    wrkAttribs->protocol = PROTOCOL_TYPE_ATA;

    memset( tdata, 0, sizeof( tdata ) );
    if( exec_cmd_ata( drive, CMD_ATA_SMART_ATTRIBS_VALUES, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    j = 0;
    for( i = 0; i < NUM_ATA_SMART_ATTRIBS; i++ )
    {
      if( !attrib[i].id ) // have seen holes in the table
        continue;

      wrkAttribs->data.ata[j].id      = attrib[i].id;
      wrkAttribs->data.ata[j].current = attrib[i].cur_value;
      wrkAttribs->data.ata[j].max     = attrib[i].max_value;
      if( ( attrib[i].id == 194 ) || ( attrib[i].id ==  190 ) || ( attrib[i].id == 231 ) )
        wrkAttribs->data.ata[j].raw = attrib[i].raw_value[0];
      else
        wrkAttribs->data.ata[j].raw = ( ( long long int )attrib[i].raw_value[5] ) << ( 5 * 8 ) |
                                      ( ( long long int )attrib[i].raw_value[4] ) << ( 4 * 8 ) |
                                      ( ( long long int )attrib[i].raw_value[3] ) << ( 3 * 8 ) |
                                      ( ( long long int )attrib[i].raw_value[2] ) << ( 2 * 8 ) |
                                      ( ( long long int )attrib[i].raw_value[1] ) << ( 1 * 8 ) |
                                      ( ( long long int )attrib[i].raw_value[0] );
      j++;
    }
    wrkAttribs->count = j;

    memset( tdata, 0, sizeof( tdata ) );
    if( exec_cmd_ata( drive, CMD_ATA_SMART_ATTRIBS_THRESHOLDS, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    for( i = 0; ( i < NUM_ATA_SMART_ATTRIBS ) && threshold[i].id; i++ )
    {
      for( j = 0; j < wrkAttribs->count; j++ )
        if( wrkAttribs->data.ata[j].id == threshold[i].id )
        {
          wrkAttribs->data.ata[j].threshold = threshold[i].threshold;
          break;
        }
    }

    return 0;
  }
  else if( command == CMD_SELF_TEST_STATUS )
  {
    unsigned char tdata[512];

    if( parm_count != 1 )
    {
      errno = EINVAL;
      return -1;
    }

    memset( tdata, 0, sizeof( tdata ) );
    if( exec_cmd_ata( drive, CMD_ATA_SELF_TEST_STATUS, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    if( verbose >= 3 )
    {
      fprintf( stderr, "Self Test Status:     0x%02x\n", tdata[363] );
      fprintf( stderr, "   Status:            0x%01x\n", ( tdata[363] & 0xF0 ) >> 4 );
      fprintf( stderr, "   Percent Remaining: %u\n",( ( tdata[363] & 0x0F ) * 10 ) );
    }

    *parms = 0;

    switch( ( tdata[363] & 0xF0 ) >> 4 )
    { // see "Table 64 — Self-test execution status values" (page 258) in the ATA/ATAPI rev 8 document
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
      case 0x07:
      case 0x08:
        *parms = SELFTEST_FAILED | ( ( tdata[363] & 0xF0 ) >> 4 );
        break;
      case 0x0F:
        *parms = SELFTEST_PER_COMPLETE | ( 100 - ( ( tdata[363] & 0x0F ) * 10 ) );
        break;
      default:
        *parms = SELFTEST_UNKNOWN | tdata[363];
        break;
    }
    return 0;
  }
  else if( command == CMD_START_SELF_TEST )
  {
    unsigned char tdata[512];
    __u64 tmpParm;

    if( parm_count != 1 )
    {
      errno = EINVAL;
      return -1;
    }

    memset( tdata, 0, sizeof( tdata ) );
    if( exec_cmd_ata( drive, CMD_ATA_SELF_TEST_STATUS, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    if( ( tdata[367] & 0x01 ) == 0 )
    {
      if( verbose >= 2 )
        fprintf( stderr, "SMART OFFLINE IMMEDIATE not supported\n" );
      return -1;
    }

    if( verbose >= 2 )
    {
      if( tdata[367] & 0x08 )
        fprintf( stderr, "Off-line Read Scanning Supported\n" );
      if( tdata[367] & 0x10 )
        fprintf( stderr, "Short and Extended Selftest Supported\n" );
      if( tdata[367] & 0x20 )
        fprintf( stderr, "Conveyance Selftest Supported\n" );
      if( tdata[367] & 0x40 )
        fprintf( stderr, "Selective Selftest Supported\n" );
    }

    // Note: tdata[372] contains polling interval in minutes if anyone cares

    tmpParm = 0;

    if( *parms == SELFTEST_ABORT )
      tmpParm = 0x7f;
    else if( *parms == SELFTEST_OFFLINE ) // no captive off-line
      tmpParm = 0x00;
    else
    {  // TODO: show a error when select test isn't supported, instead of the vauge Confusion thing, leave Confusion for Confusion
      if( ( ( *parms & 0x0F ) == SELFTEST_CONVEYANCE ) && ( tdata[367] & 0x20 ) )
        tmpParm = 0x03;
      else if( ( ( *parms & 0x0F ) == SELFTEST_EXTENDED ) && ( tdata[367] & 0x10 ) )
        tmpParm = 0x02;
      else if ( ( ( *parms & 0x0F ) == SELFTEST_SHORT ) && ( tdata[367] & 0x10 ) )
        tmpParm = 0x01;
      else
      {
        if( verbose >= 2 )
          fprintf( stderr, "Self test Confusion, type: 0x%llx, tdata[367]: 0x%x\n", *parms, tdata[367] );
        errno = EBADE;
        return -1;
      }

      if( ( *parms & 0xF0 ) == SELFTEST_CAPTIVE )
        tmpParm |= 0x80;
    }

    if( exec_cmd_ata( drive, CMD_ATA_SELF_TEST_START, RW_NONE, NULL, 0, &tmpParm, 1, timeout ) )
      return -1;

    return 0;
  }
  else if( command == CMD_SMART_LOG_COUNT )
  {
    int i;
    unsigned char tdata[512];

    if( parm_count != 1 )
    {
      errno = EINVAL;
      return -1;
    }

    memset( tdata, 0, sizeof( tdata ) );
    if( exec_cmd_ata( drive, CMD_ATA_SMART_LOG_COUNT, RW_READ, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    if( verbose >= 2 )
    {
      fprintf( stderr, "Error Log Version: 0x%02x\n", tdata[0] );
      fprintf( stderr, "Error Log Index: 0x%02x\n", tdata[1] );
      for( i = 0; i < 5; i++ )
        fprintf( stderr, "Error #%i: CMD: 0x%02x at: %u\n", i, tdata[ ( ( i * 90 ) + 2 ) + 60 ], (__u16) ( tdata[ ( ( i * 90 ) + 2 ) + 60 + 8 ] << 24 | tdata[ ( ( i * 90 ) + 2 ) + 60 + 9 ] << 16 | tdata[( ( i * 90 ) + 2 ) + 60 + 10 ] << 8 | tdata[ ( ( i * 90 ) + 2 ) + 60 + 11 ] ) );
      fprintf( stderr, "Error Count: %i ( 0x%02x%02x )\n", ( tdata[453] << 8 ) | tdata[452], tdata[453], tdata[452] );
    }

    if( tdata[1] != 0x00 )
      *parms = ( tdata[453] << 8 ) | tdata[452];
    else
      *parms = 0;

    return 0;
  }
  else if( command == CMD_WRITESAME )
  {
    unsigned char tdata[512];
    if( parm_count != 3 )
    {
      errno = EINVAL;
      return -1;
    }

    memset( tdata, 0, sizeof( tdata ) );

    tdata[0]  = 0x02;
    tdata[1]  = 0x00; // action = 0x0002   316
    tdata[2]  = 0x01;
    tdata[3]  = 0x00; // function = 0x0001
    // 4->11     start
    tdata[4] =  ( parms[0] & 0x00000000000000FF );
    tdata[5] =  ( parms[0] & 0x000000000000FF00 ) >> 8;
    tdata[6] =  ( parms[0] & 0x0000000000FF0000 ) >> 16;
    tdata[7] =  ( parms[0] & 0x00000000FF000000 ) >> 24;
    tdata[8] =  ( parms[0] & 0x000000FF00000000 ) >> 32;
    tdata[9] =  ( parms[0] & 0x0000FF0000000000 ) >> 40;
    tdata[10] = ( parms[0] & 0x00FF000000000000 ) >> 48;
    tdata[11] = ( parms[0] & 0xFF00000000000000 ) >> 56;
    // 12->19    count
    tdata[12] = ( parms[1] & 0x00000000000000FF );
    tdata[13] = ( parms[1] & 0x000000000000FF00 ) >> 8;
    tdata[14] = ( parms[1] & 0x0000000000FF0000 ) >> 16;
    tdata[15] = ( parms[1] & 0x00000000FF000000 ) >> 24;
    tdata[16] = ( parms[1] & 0x000000FF00000000 ) >> 32;
    tdata[17] = ( parms[1] & 0x0000FF0000000000 ) >> 40;
    tdata[18] = ( parms[1] & 0x00FF000000000000 ) >> 48;
    tdata[19] = ( parms[1] & 0xFF00000000000000 ) >> 56;
    // 20->23    pattern
    tdata[20] = ( parms[2] & 0x000000FF );
    tdata[21] = ( parms[2] & 0x0000FF00 ) >> 8;
    tdata[22] = ( parms[2] & 0x00FF0000 ) >> 16;
    tdata[23] = ( parms[2] & 0xFF000000 ) >> 24;

    if( exec_cmd_ata( drive, CMD_ATA_WRITESAME, RW_WRITE, tdata, sizeof( tdata ), NULL, 0, timeout ) )
      return -1;

    return 0;
  }
  else if( command == CMD_TRIM )
  {
/*
  parms[0] = LBA;
  parms[1] = count;
*/
    long long unsigned i;
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

    while( length )
    {
      i = 0;
      while( ( i + 8 ) < 512 )
      {
        tmp = min( length, 0xFFFF );

        tdata[ i++ ] = ( pos & 0x00000000000000FF );
        tdata[ i++ ] = ( pos & 0x000000000000FF00 ) >> 8;
        tdata[ i++ ] = ( pos & 0x0000000000FF0000 ) >> 16;
        tdata[ i++ ] = ( pos & 0x00000000FF000000 ) >> 24;
        tdata[ i++ ] = ( pos & 0x000000FF00000000 ) >> 32;
        tdata[ i++ ] = ( pos & 0x0000FF0000000000 ) >> 40;

        tdata[ i++ ] = ( tmp & 0x00FF );
        tdata[ i++ ] = ( tmp & 0xFF00 ) >> 8;

        if( verbose >= 4 )
          fprintf( stderr, "ATA: Trimming from %llu for %llu \n", pos, tmp );

        if( length <= 0xFFFF )
        {
          length = 0;
          break;
        }

        length -= 0xFFFF;
        pos += 0xFFFF;
      }

      if( exec_cmd_ata( drive, CMD_ATA_TRIM, RW_WRITE, tdata, sizeof( tdata ), NULL, 0, timeout ) )
        return -1;
    }
    return 0;
  }
  if( get_tf( &tf, command, drive->drive_info.bit48LBA, parms, parm_count ) )
    return -1;

  if( load_cdb_ata( cdb, &tf, rw ) )
    return -1;

  if( verbose >= 4 )
    dump_bytes( "ATA: cdb out", cdb, sizeof( cdb ) );

  if( drive->driver_cmd( drive, rw, cdb, sizeof( cdb ), data, data_len, timeout ) )
    return -1;

  if( verbose >= 4 )
    dump_bytes( "ATA: cdb in", cdb, sizeof( cdb ) );

  if( tf.checkCond )
  {
    if( verbose >= 4 )
      fprintf( stderr, "Checking CDB.\n" );

    if( ( cdb[0] != 0x09 ) && ( cdb[1] != 0x0c ) )
    {
      if( verbose >= 2 )
        fprintf( stderr, "Warning: Check Condition requested, but no CDB.\n" );
      //errno = EBADE;  can't bail here, b/c some raid controllers strip the CDB for some reason (3ware, megaraid)
      //return -1;      perhaps some kind of degenrate operating mode like smartctls permissive mode instead, or only bypass for 3ware/megaraid
       memset( parms, 0, parm_count );
    }
    else
    {
      if( unload_cdb_ata( cdb, &tf, rw ) )
        return -1;

      if( parse_tf( &tf, command, drive->drive_info.bit48LBA, parms, parm_count ) )
        return -1;
    }
  }
  else
  {
    memset( parms, 0, parm_count );
  }

  return 0;
}

char *ata_version_desc( unsigned int version ) // from Table 31 — Minor version number
{
  switch( version )
  {
    case( 0x000D ):
     return "ATA/ATAPI-4 X3T13 1153D version 6";
    case( 0x000E ):
     return "ATA/ATAPI-4 T13 1153D version 13";
    case( 0x000F ):
     return "ATA/ATAPI-4 X3T13 1153D version 7";
    case( 0x0010 ):
     return "ATA/ATAPI-4 T13 1153D version 18";
    case( 0x0011 ):
     return "ATA/ATAPI-4 T13 1153D version 15";
    case( 0x0012 ):
     return "ATA/ATAPI-4 published, ANSI INCITS 317-1998";
    case( 0x0013 ):
     return "ATA/ATAPI-5 T13 1321D version 3";
    case( 0x0014 ):
     return "ATA/ATAPI-4 T13 1153D version 14";
    case( 0x0015 ):
     return "ATA/ATAPI-5 T13 1321D version 1";
    case( 0x0016 ):
     return "ATA/ATAPI-5 published, ANSI INCITS 340-2000";
    case( 0x0017 ):
     return "ATA/ATAPI-4 T13 1153D version 17";
    case( 0x0018 ):
     return "ATA/ATAPI-6 T13 1410D version 0";
    case( 0x0019 ):
     return "ATA/ATAPI-6 T13 1410D version 3a";
    case( 0x001A ):
     return "ATA/ATAPI-7 T13 1532D version 1";
    case( 0x001B ):
     return "ATA/ATAPI-6 T13 1410D version 2";
    case( 0x001C ):
     return "ATA/ATAPI-6 T13 1410D version 1";
    case( 0x001D ):
     return "ATA/ATAPI-7 published ANSI INCITS 397-2005";
    case( 0x001E ):
     return "ATA/ATAPI-7 T13 1532D version 0";
    case( 0x0021 ):
     return "ATA/ATAPI-7 T13 1532D version 4a";
    case( 0x0022 ):
     return "ATA/ATAPI-6 published, ANSI INCITS 361-2002";
    case( 0x0027 ):
     return "ATA8-ACS version 3c";
    case( 0x0028 ):
     return "ATA8-ACS version 6";
    case( 0x0029 ):
     return "ATA8-ACS version 4";
    case( 0x0033 ):
     return "ATA8-ACS version 3e";
    case( 0x0039 ):
     return "ATA8-ACS version 4c";
    case( 0x0042 ):
     return "ATA8-ACS version 3f";
    case( 0x0052 ):
     return "ATA8-ACS version 3b";
    case( 0x0107 ):
     return "ATA8-ACS version 2d";
    case( 0xFFFF ):
    case( 0x0000 ):
     return "Minor version is not reported";

    default:
      return "Unknown";
  }
}
