#ifndef _SCSI_H
#define _SCSI_H

#include <linux/types.h>
#include "cdb.h"
#include "device.h"

#define SCSI_16_CDB_LEN        16

#define SCSI_VENDOR_ID_RAW_LEN      8
#define SCSI_MODEL_NUMBER_RAW_LEN   16
#define SCSI_VERSION_RAW_LEN        4

struct scsi_identity_page0
{
  __u8 peripherial_info;                          // 0
  __u8 pad_1[1];
  __u8 version;                                   // 2
  __u8 pad_2[5];
  __u8 vendor_id[SCSI_VENDOR_ID_RAW_LEN];         // 8-15
  __u8 model_number[SCSI_MODEL_NUMBER_RAW_LEN];   // 16-31
  __u8 product_rev[SCSI_VERSION_RAW_LEN];         // 32-35
  __u8 pad_3[22];
  __u8 version_descriptor1[2];                    // 58-59
  __u8 version_descriptor2[2];                    // 60-61
  __u8 version_descriptor3[2];                    // 62-63
  __u8 version_descriptor4[2];                    // 64-65
  __u8 version_descriptor5[2];                    // 66-67
  __u8 version_descriptor6[2];                    // 68-69
  __u8 version_descriptor7[2];                    // 70-71
  __u8 version_descriptor8[2];                    // 72-73
} __attribute__((packed));

struct scsi_identity
{
  __u8 data[512];
} __attribute__((packed));

struct sence_log_15h_100h  // background scan status log
{ // bytes 0-3 are sucked up by the log scanner
  __u8 accumulated_power_on_min[4];              // 4-7CMD_SCSI_TEST_START
  __u8 pad_1;
  __u8 scan_status;                              // 9
  __u8 num_scans_performed[2];                   // 10-11
  __u8 scan_progress[2];                         // 12-13
  __u8 num_medium_scans_performed[2];            // 14-15
} __attribute__((packed));

// see http://tldp.org/HOWTO/SCSI-Generic-HOWTO/
#define SG_CHECK_CONDITION    0x02

#define SG_ERR_DID_NO_CONNECT 0x01
#define SG_ERR_DID_BUS_BUSY   0x02
#define SG_ERR_DID_TIME_OUT   0x03
#define SG_ERR_DID_RESET      0x08

#define SG_ERR_DRIVER_TIMEOUT 0x06
#define SG_ERR_DRIVER_SENSE   0x08

enum SCSI_PERIPHERAL_TYPES
{
  SCSI_DIRECT_ACCESS      = 0x00,
  SCSI_ENCLOSURE_SERVICES = 0x0d,
};

enum SCSI_COMMANDS
{
  SCSI_OP_REQUEST_SENSE   = 0x03,

  SCSI_OP_INQUIRY         = 0x12,

  SCSI_OP_MODE_SENSE_6    = 0x1a,
  SCSI_OP_START_STOP_UNIT = 0x1b,
  SCSI_OP_RECEIVE_DIAGNOSTIC = 0x1c,
  SCSI_OP_SEND_DIAGNOSTIC = 0x1d,

  SCSI_OP_UNMAP           = 0x42,

  SCSI_OP_LOG_SENSE       = 0x4d,

  SCSI_READ_16            = 0x88,
  SCSI_WRITE_16           = 0x8a,

  SCSI_WRITE_SAME_16      = 0x93,

  SCSI_READ_CAPACITY_16   = 0x9e,
};

enum SCSI_LOG_SENCE_PAGES
{
  SCSI_LOG_SENCE_PAGE_LIST              = 0x00,
  SCSI_LOG_SENCE_PAGE_ERROR_WRITE       = 0x02,
  SCSI_LOG_SENCE_PAGE_ERROR_READ        = 0x03,
  SCSI_LOG_SENCE_PAGE_ERROR_VERIFY      = 0x05,
  SCSI_LOG_SENCE_PAGE_ERROR_NON_MEDIUM  = 0x06,
  SCSI_LOG_SENCE_PAGE_TEMPERATURE       = 0x0d,
  SCSI_LOG_SENCE_PAGE_START_STOP        = 0x0e,
  SCSI_LOG_SENCE_PAGE_SELF_TEST         = 0x10,
  SCSI_LOG_SENCE_PAGE_SSD               = 0x11, // SBC-3
  SCSI_LOG_SENCE_PAGE_BACKGROUND_SCAN   = 0x15, // SBC-3
  SCSI_LOG_SENCE_PAGE_INFO_EXCEPT       = 0x2f,
  SCSI_LOG_SENCE_PAGE_FACTORY_LOG       = 0x3e, // Segate-Hatichi
};

enum SCSI_VPD_PAGES
{
  SCSI_VPD_PAGE_LIST                   = 0x00,
  SCSI_VPD_PAGE_SERIAL                 = 0x80,
  SCSI_VPD_PAGE_IDENT                  = 0x83,
  SCSI_VPD_PAGE_BLOCK_LIMITS           = 0xb0, // SBC-3
  SCSI_VPD_PAGE_BLOCK_DEV_CHRSTCS      = 0xb1, // SBC-3
};

enum SCSI_DIAGNOSTIC_PAGES
{
  SCSI_DIAGNOSTIC_PAGE_SES_CONFIGURATION = 0x01,
  SCSI_DIAGNOSTIC_PAGE_SES_ENCLOSURE_CNTRL_STATUS = 0x02,
  SCSI_DIAGNOSTIC_PAGE_SES_ENCLOSURE_HELP_TEXT = 0x03,
  SCSI_DIAGNOSTIC_PAGE_SES_ENCLOSURE_STRING = 0x04,
  SCSI_DIAGNOSTIC_PAGE_SES_THRESHOLD = 0x05,
  SCSI_DIAGNOSTIC_PAGE_SES_ELEMENT_DESCRIPTOR = 0x07,
  SCSI_DIAGNOSTIC_PAGE_SES_SHORT_ENCOLSURE_STATUS = 0x08,
  SCSI_DIAGNOSTIC_PAGE_SES_ENCOLSURE_BUSY = 0x09,
  SCSI_DIAGNOSTIC_PAGE_SES_ADDITIONAL_ELEMENT = 0x0a,
  SCSI_DIAGNOSTIC_PAGE_SES_SUBENCOLSURE_HELP_TEXT = 0x0b,
  SCSI_DIAGNOSTIC_PAGE_SES_SUBENCOLSURE_STRING = 0x0c,
  SCSI_DIAGNOSTIC_PAGE_SES_SUPPORTED_SES_DIAGNOSTIC_PAGES = 0x0d,
  SCSI_DIAGNOSTIC_PAGE_SES_DOWNLOAD_MICROCODE = 0x0e,
  SCSI_DIAGNOSTIC_PAGE_SES_SUBENCOLSURE_NICKNAME = 0x0f,
  SCSI_DIAGNOSTIC_PAGE_VENDOR_10h = 0x10,
  SCSI_DIAGNOSTIC_PAGE_VENDOR_11h = 0x11,
};

enum SCSI_PC
{
  SCSI_PC_THRESHOLD          = 0x00,
  SCSI_PC_CUMULATIVE         = 0x40,
  SCSI_PC_DEFAULT_THRESHOLD  = 0x80,
  SCSI_PC_DEFULAT_CUMULATIVE = 0xC0,
};

// see http://www.t10.org/lists/asc-num.htm
enum SCSI_ASC
{
  SCSI_ASC_WARNING           = 0x0b,
  SCSI_ASC_IMPENDING_FAILURE = 0x5d,
};

enum SCSI_ASCQ
{
  SCSI_ASCQ_SELFTEST_FAILED = 0x03,
  SCSI_ASCQ_PRE_SCAN_MEDIUM_ERROR = 0x04,
  SCSI_ASCQ_MEDIUM_SCAN_MEDIUM_ERROR = 0x05,
};

void loadInfoSCSI( struct device_handle *drive, const struct scsi_identity *identity );
int exec_cmd_scsi( struct device_handle *drive, const enum cdb_command command, const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], const unsigned int parm_count, const unsigned int timeout );

void print_sense( const char *label, const unsigned char *sb );
void dump_bytes( const char *label, const unsigned char *p, const int len );

char *scsi_descriptor_desc( unsigned int descriptor );
#endif
