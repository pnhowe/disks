#ifndef _CDB_H
#define _CDB_H

enum cdb_command // NOTE:b0-cf are reserved for extended ata, d0-ff are reserved for extend scsi
{
  CMD_UNKNOWN          = 0x00,
  CMD_PROBE            = 0xff, // this is so we can detect SCSI/ATA without a full identitfy, due to the fact SCSI has to have lots of cmds to do a identify
  CMD_IDENTIFY         = 0x01,
  CMD_IDENTIFY_ENCLOSURE = 0x02,

  CMD_RESET            = 0x05,
  CMD_SLEEP            = 0x06,
  CMD_STANDBY          = 0x07,
  CMD_IDLE             = 0x08,
  CMD_GET_POWER_STATE  = 0x09,

  CMD_SMART_ENABLE     = 0x10,
  CMD_SMART_DISABLE    = 0x11,
  CMD_GET_MAXLBA       = 0x12,
  CMD_SET_MAXLBA       = 0x13,
  CMD_WRITESAME        = 0x14,
  CMD_SCT_STATUS       = 0x15,
  CMD_TRIM             = 0x16,

  CMD_SMART_ATTRIBS    = 0x17,
  CMD_SMART_STATUS     = 0x18,
  CMD_SMART_LOG_COUNT  = 0x19,

  CMD_START_SELF_TEST  = 0x1e,
  CMD_SELF_TEST_STATUS = 0x1f,

  CMD_READ             = 0x20,
  CMD_WRITE            = 0x21,

  CMD_ENABLE_WRITE_CACHE = 0x23,
  CMD_DISABLE_WRITE_CACHE = 0x24,

  // ATA internal states
  CMD_ATA_SMART_ATTRIBS_VALUES     = 0xb0,
  CMD_ATA_SMART_ATTRIBS_THRESHOLDS = 0xb1,
  CMD_ATA_SELF_TEST_STATUS         = 0xb2,
  CMD_ATA_SELF_TEST_START          = 0xb3,
  CMD_ATA_SMART_LOG_COUNT          = 0xb4,
  CMD_ATA_WRITESAME                = 0xb5,
  CMD_ATA_TRIM                     = 0xb6,

  // SCSI internal states
  CMD_SCSI_INQUIRY          = 0xd0,
  CMD_SCSI_INQUIRY_SERIAL   = 0xd1,
  CMD_SCSI_INQUIRY_IDENT    = 0xd2,
  CMD_SCSI_LOG_PAGE         = 0xd3,
  CMD_SCSI_VPD_PAGE         = 0xd4,
  CMD_SCSI_SENSE            = 0xd5,
  CMD_SCSI_TRIM             = 0xd6,

  CMD_SCSI_WRITESAME        = 0xe5,
  CMD_SCSI_READ_CAPACITY    = 0xe6,
  CMD_SCSI_SEND_DIAGNOSTIC  = 0xe7,
  CMD_SCSI_RECEIVE_DIAGNOSTIC = 0xe8,
};

enum cdb_rw
{
  RW_NONE         = 0x00,
  RW_READ         = 0x01,
  RW_WRITE        = 0x02
};

#endif

//TODO: add SLEEP and DEVICE RESET (to wake up)
