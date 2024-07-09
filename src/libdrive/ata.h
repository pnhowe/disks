#ifndef _ATA_H
#define _ATA_H

#include <linux/types.h>
#include "disk.h"
#include "cdb.h"

#define	ATA_16		      0x85
#define ATA_CDB_LEN         16

#define ATA_SERIAL_NUMBER_RAW_LEN 10
#define ATA_FIRMWARE_REV_RAW_LEN 4
#define ATA_MODEL_NUMBER_RAW_LEN 20

// https://people.freebsd.org/~imp/asiabsdcon2015/works/d2161r5-ATAATAPI_Command_Set_-_3.pdf

struct ata_identity // needs to be 512 bytes
{
  __u16 pad_0[10];
  __u16 serial_number[ATA_SERIAL_NUMBER_RAW_LEN]; // 10-19
  __u16 pad_20[3];
  __u16 firmware_rev[ATA_FIRMWARE_REV_RAW_LEN];       // 23-26
  __u16 model_number[ATA_MODEL_NUMBER_RAW_LEN];   // 27-46
  __u16 pad_47[2];
  __u16 capabilities49;                           // 49
  __u16 pad_50[10];
  __u16 lba28[2];                                 // 60-61
  __u16 pad_62[7];
  __u16 capabilities69;                           // 69
  __u16 pad_70[10];
  __u16 ata_major_version;                        // 80
  __u16 ata_minor_version;                        // 81
  __u16 capabilities82;                           // 82
  __u16 capabilities83;                           // 83
  __u16 capabilities84;                           // 84
  __u16 capabilities85;                           // 85
  __u16 pad_86[14];
  __u16 lba48[4];                                 // 100-103
  __u16 pad_104[2];
  __u16 sector_size_info;                         // 106
  __u16 pad_107[1];
  __u16 wwn[4];                                   // 108-111
  __u16 pad_112[5];
  __u16 logical_sector_size[2];                   // 117-118
  __u16 pad_119[50];
  __u16 data_set_mgnt;                            // 169
  __u16 pad_170[36];
  __u16 sct_commands;                             // 206
  __u16 pad_207[10];
  __u16 nominal_media_rotation;                   // 217
  __u16 pad_218[12];
  __u16 lbaextnded[4];                            // 230-233
  __u16 pad_end[24];
} __attribute__((packed));
// use this for size dependant stuff -> ASSERT_SIZEOF_STRUCT(ata_identify_device, 512);


enum ATA_COMMANDS
{
  ATA_OP_IDENTIFY       = 0xec,

  ATA_OP_RESET          = 0x08,
  ATA_OP_SLEEP          = 0xe6,
  ATA_OP_STANDBY        = 0xe2,
  ATA_OP_IDLE           = 0xe3,
  ATA_OP_CHECK_POWER_MODE = 0xe5,

  ATA_OP_READ_NATIVE_MAX      = 0xf8,
  ATA_OP_READ_NATIVE_MAX_EXT  = 0x27,

  ATA_OP_SMART          = 0xb0,

  ATA_OP_DATA_SET_MANGEMENT = 0x06,

  ATA_OP_SETFEATURES    = 0xef,

  ATA_OP_READ_PIO       = 0x20,
  ATA_OP_READ_PIO_EXT   = 0x24,
  ATA_OP_READ_DMA       = 0xc8,
  ATA_OP_READ_DMA_EXT   = 0x25,
  ATA_OP_READ_DMA_QUEUED = 0xc7,
  ATA_OP_READ_DMA_QUEUED_EXT = 0x26,

  ATA_OP_WRITE_PIO      = 0x30,
  ATA_OP_WRITE_PIO_EXT  = 0x34,
  ATA_OP_WRITE_DMA      = 0xca,
  ATA_OP_WRITE_DMA_EXT  = 0x35,
  ATA_OP_WRITE_DMA_QUEUED = 0xcc,
  ATA_OP_WRITE_DMA_QUEUED_EXT  = 0x36,
};

enum ATA_LOG
{
  ATA_LOG_DIR                = 0x00,
  ATA_LOG_SMART_ERR_SUMMARY  = 0x01,
  ATA_LOG_SMART_ERR_COMP     = 0x02,
  ATA_LOG_SMART_ERR_EXT_COMP = 0x03,
  ATA_LOG_SMART_DEV_STAT     = 0x04,
  ATA_LOG_SCT_CMD            = 0xe0,
};

#define SMART_OP_LBAM 0x4f
#define SMART_OP_LBAH 0xc2

enum ATA_SMART_OP
{
  SMART_OP_READ_VALUES        = 0xd0,
  SMART_OP_READ_THRESHOLDS    = 0xd1,
  SMART_OP_OFFLINE_IMMEDIATE  = 0xd4,
  SMART_OP_READ_LOG           = 0xd5,
  SMART_OP_WRITE_LOG          = 0xd6,
  SMART_OP_GET_STATUS         = 0xda,
  SMART_OP_ENABLE             = 0xd8,
  SMART_OP_DISABLE            = 0xd9,
};

/*
 * Some useful ATA register bits
 */
enum {
  ATA_USING_LBA    = (1 << 6),
  ATA_STAT_FAULT  = (1 << 5),
  ATA_STAT_DRQ    = (1 << 3),
  ATA_STAT_ERR    = (1 << 0),
};

/*
 * Definitions and structures for use with SG_IO + ATA_16:
 */
struct ata_lba_regs {
  __u8  feat;
  __u8  nsect;
  __u8  lbal;
  __u8  lbam;
  __u8  lbah;
};

struct ata_tf {
  __u8      dev;
  enum ATA_COMMANDS   command;
  __u8      error;
  __u8      status;
  struct ata_lba_regs  lob;
  struct ata_lba_regs  hob;
  __u8 checkCond;
};

#define NUM_ATA_SMART_ATTRIBS 30

struct ata_smart_attrib // needs to be 12 bytes
{
  __u8 id;
  __u16 flags;
  __u8 cur_value;
  __u8 max_value;
  __u8 raw_value[6];
  __u8 pad;
} __attribute__((packed));

struct ata_smart_threshold // needs to be 12 bytes
{
  __u8 id;
  __u8 threshold;
  __u8 head_0;
  __u8 head_1;
  __u8 head_2;
  __u8 head_3;
  __u8 head_4;
  __u8 head_5;
  __u8 head_6;
  __u8 head_7;
  __u8 formula_rev;
  __u8 formula_structure_rev;
} __attribute__((packed));


#define SG_ATA_LBA48    1
// see libata-scsi.c
#define SG_ATA_PROTO_NON_DATA  ( 3 << 1 )
#define SG_ATA_PROTO_PIO_IN  ( 4 << 1 )
#define SG_ATA_PROTO_PIO_OUT  ( 5 << 1 )
#define SG_ATA_PROTO_DMA  ( 6 << 1 )
#define SG_ATA_PROTO_DMA_QUEUED ( 7 << 1 )
#define SG_ATA_PROTO_UDMA_IN  ( 10 << 1 )
#define SG_ATA_PROTO_UDMA_OUT  ( 11 << 1 )

enum {
  SG_CDB2_TLEN_NODATA  = 0 << 0,
  SG_CDB2_TLEN_FEAT  = 1 << 0,
  SG_CDB2_TLEN_NSECT  = 2 << 0,

  SG_CDB2_TLEN_BYTES  = 0 << 2,
  SG_CDB2_TLEN_SECTORS  = 1 << 2,

  SG_CDB2_TDIR_TO_DEV  = 0 << 3,
  SG_CDB2_TDIR_FROM_DEV  = 1 << 3,

  SG_CDB2_CHECK_COND  = 1 << 5,
};

void loadInfoATA( struct device_handle *drive, const struct ata_identity *identity );
int exec_cmd_ata( struct device_handle *drive, const enum cdb_command command, const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], const unsigned int parm_count, const unsigned int timeout );

char *ata_version_desc( unsigned int version );

#endif
