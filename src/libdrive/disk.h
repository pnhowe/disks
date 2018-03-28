#ifndef _DISK_H
#define _DISK_H

#include <linux/types.h>
#include "cdb.h"
#include "device.h"

struct smart_attrib_ata
{
  unsigned char id;
  unsigned char current;
  unsigned char max;
  unsigned char threshold;
  unsigned long long raw;
}  __attribute__((packed));

struct smart_attrib_scsi
{
  unsigned char page_code;
  unsigned char parm_code;
  unsigned long long value;
}  __attribute__((packed));

#define MAX_ATTRIB_SIZE  500
#define MAX_ATA_ATTRIBS  ( MAX_ATTRIB_SIZE / sizeof( struct smart_attrib_ata ) )
#define MAX_SCSI_ATTRIBS ( MAX_ATTRIB_SIZE / sizeof( struct smart_attrib_scsi ) )

// TODO: assert MAX_ATA_ATTRIBS < NUM_ATA_SMART_ATTRIBS

struct smart_attribs // needs to be smaller that 512
{
  enum protocol_type protocol; // size 4 ( techinically sizeof( int ), so we should leave a little extra depending on compliler )
  unsigned char count; // size 1
  union
  {
    unsigned char raw[ MAX_ATTRIB_SIZE ]; // gives some space for the others to exist in, without havving to malloc it
    struct smart_attrib_ata ata[ MAX_ATA_ATTRIBS ];  // even thou *ata, ata[], and ata[0] seem to do the same, only the last option works, first one gives segfauls, second dosen't compile
    struct smart_attrib_scsi scsi[ MAX_SCSI_ATTRIBS ];
  } data;
};

// TODO: asert sizeof( smart_attribs ) > 512

#define IDENT 1
#define NO_IDENT 0

// open and closing the device, open returns < 0 on error
int openDisk( const char *device, struct device_handle *drive, const enum driver_type driver, const enum protocol_type protocol, unsigned char ident ); // if no_ident specified, protocal can't be unknown
void closeDisk( struct device_handle *drive );

// drive info
struct drive_info *getDriveInfo( struct device_handle *drive );

// returns -1 on error
int enableSMART( struct device_handle *drive );
int disableSMART( struct device_handle *drive );

// return -1 on error, getMaxLBA > 0 -> value == 0 not supported
__u64 getMaxLBA( struct device_handle *drive );
int setMaxLBA( struct device_handle *drive, __u64 LBA );

// returns -1 on error
int doReset( struct device_handle *drive );
int sleepState( struct device_handle *drive );
int standbyState( struct device_handle *drive );
int idleState( struct device_handle *drive );
#define POWER_ACTIVE    0x01
#define POWER_SLEEP     0x02
#define POWER_STANDBY   0x03
#define POWER_IDLE      0x04
#define POWER_UNKNOWN   0x00
int getPowerMode( struct device_handle *drive );

// returns -1 on error, if count == 0, will do the end of the disk
int writeSame( struct device_handle *drive, __u64 start, __u64 count, __u32 value );

// LBAs only go up to 48 bits, messing with the top few bits should be safe, sorry this is kinda mess, in a hurry and this is in the spirit of this ATA control fun
#define WRITESAME_STATUS_LBA          ( (__u64) 1 << 62 )
#define WRITESAME_STATUS_DONE_STATUS  ( (__u64) 1 << 61 )
// returns LBA Postition, if rc | STATUS_LBA -> LBA   if rc | STATUS_DONE_STATUS -> wipe status     if < 0 -> error
long long int wirteSameStatus( struct device_handle *drive );

// returns -1 on error, if count == 0, will do the end of the disk
int trim( struct device_handle *drive, __u64 start, __u64 count );

// type is ABORT or OFFLINE or ( ( SHORT or EXTENDED or CONVEYANCE ) | CAPTIVE )
// returns -1 on error
#define SELFTEST_SHORT      0x01
#define SELFTEST_EXTENDED   0x02
#define SELFTEST_CONVEYANCE 0x03
#define SELFTEST_OFFLINE    0x04
#define SELFTEST_CAPTIVE    0x10
#define SELFTEST_ABORT      0xFF
int smartSelfTest( struct device_handle *drive, int type );

// return -1 for error, 0 for unknown, SELFTEST_PER_COMPLETE | percent complete (whole number), SELFTEST_FAILED | failed code, or one of the others, or SELFTEST_UNKNOWN | self test value
// avoiding the first bit incase we end up in a 16 bit int and it thinks it is negative
#define SELFTEST_STATUS_MASK      0x7000
#define SELFTEST_VALUE_MASK       0x0fff
#define SELFTEST_PER_COMPLETE     0x1000
#define SELFTEST_COMPLETE         0x2000
#define SELFTEST_HOST_ABBORTED    0x3000
#define SELFTEST_HOST_INTERRUPTED 0x4000
#define SELFTEST_FAILED           0x5000
#define SELFTEST_UNKNOWN          0x7000
int smartSelfTestStatus( struct device_handle *drive );

// return -1 for error
int smartAttributes( struct device_handle *drive, struct smart_attribs *attribs );

// return -1 for error, otherwise a combination of DRIVE_GOOD or ( DRIVE_FAULT and/or THRESHOLD_EXCEEDED )
#define STATUS_DRIVE_GOOD         0x0000
#define STATUS_DRIVE_FAULT        0x0001
#define STATUS_THRESHOLD_EXCEEDED 0x0002
int smartStatus( struct device_handle *drive );

// return # of Log Entries, -1 for error
int smartLogCount( struct device_handle *drive );

// call getMaXLBA before read or write, returns -1 for error
// DMA -> 1 for DMA 0->PIO (for ATA), ignored for SCSI
// FMA -> 1 for FUA 0 for not
// buff must be size count * logical sector size
int readLBA( struct device_handle *drive, int DMA, int FUA, __u64 LBA, __u16 count, __u8 *buff );
int writeLBA( struct device_handle *drive, int DMA, int FUA, __u64 LBA, __u16 count, __u8 *buff );

// returns -1 on error
int enableWriteCache( struct device_handle *drive );
int disableWriteCache( struct device_handle *drive );

#endif
