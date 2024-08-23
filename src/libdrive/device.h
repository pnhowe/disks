#ifndef _DEVICE_H
#define _DEVICE_H

#include <linux/types.h>
#include "cdb.h"

// need to be x2 + null the size of the space in or the max of ATA or size of the SCSI + null identity
#define SERIAL_NUMBER_LEN 34
#define FIRMWARE_REV_LEN 10
#define MODEL_NUMBER_LEN 42
#define VERSION_LEN 5
#define VENDOR_ID_LEN 9

enum driver_type
{
  DRIVER_TYPE_UNKNOWN  = 0x00,
  DRIVER_TYPE_IDE      = 0x01,
  DRIVER_TYPE_SGIO     = 0x02,
  DRIVER_TYPE_SAT      = 0x03,
  DRIVER_TYPE_NVME     = 0x04,

// LSI
  // DRIVER_TYPE_3WARE    = 0x10, // Removed
  DRIVER_TYPE_MEGADEV  = 0x11,
  DRIVER_TYPE_MEGASAS  = 0x12
};

enum protocol_type
{
  PROTOCOL_TYPE_UNKNOWN = 0x00,
  PROTOCOL_TYPE_ATA     = 0x01,
  PROTOCOL_TYPE_SCSI    = 0x02,
  PROTOCOL_TYPE_NVME    = 0x03
};

// the boolean flags are chars and not bit masks to make it easier to get to python, and memory is cheep
struct drive_info // had to do the char thing to get python to read it right
{
  enum driver_type driver;  // for external stuff to view, keeps other from messing with the device_handle stuffs
  enum protocol_type protocol;
  char model[MODEL_NUMBER_LEN];
  char serial[SERIAL_NUMBER_LEN];
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
  unsigned __int128 WWN;              // = 0 for not supported, or GUID
  // ATA & NVME Only
  char firmware_rev[FIRMWARE_REV_LEN];
  // ATA Only
  int ATA_major_version;
  int ATA_minor_version;
  char bit48LBA;
  char supportsDMA;
  char supportsSCT; //? has SCSI?
  char supportsSETMAX;
  // SCSI Only
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
  // NVME Only
  char supportsSanitize;
  unsigned int numberOfNamespaces;
  unsigned __int128 totalCapacity;        // device globally
  unsigned __int128 unallocatedCapacity;  // device globally
  unsigned long long int nsCapactyLBA;
  unsigned long long int nsUtilitzationLBA;
  char supportsThin;
} __attribute__((packed));
// A lot of things will break if this goes over 512


struct enclosure_info // had to do the char thing to get python to read it right
{
  enum driver_type driver;  // for external stuff to view, keeps other from messing with the device_handle stuffs
  enum protocol_type protocol;
  char model[MODEL_NUMBER_LEN];
  char serial[SERIAL_NUMBER_LEN];
  unsigned long long WWN;             // = 0 for not supported
  char vendor_id[VENDOR_ID_LEN];
  char version[VERSION_LEN];
  int SCSI_version;
  char hasDiagnosticPageConfiguration;
  char hasDiagnosticPageControlStatus;
  char hasDiagnosticPageHelpText;
  char hasDiagnosticPageString;
  char hasDiagnosticPageThreshold;
  char hasDiagnosticPageElementDescriptor;
  char hasDiagnosticPageShortStatus;
  char hasDiagnosticPageBusy;
  char hasDiagnosticPageAdditionalElement;
  char hasDiagnosticPageSubHelpText;
  char hasDiagnosticPageSubString;
  char hasDiagnosticPageSupportedDiagnostigPages;
  char hasDiagnosticPageDownloadMicroCode;
  char hasDiagnosticPageSubNickname;
  char hasVPDPageSerial;
  char hasVPDPageIdentification;
};

struct device_handle
{
  int fd;
  union
  {
    struct drive_info drive_info;
    struct enclosure_info enclosure_info;
  };
  enum driver_type driver;
  enum protocol_type protocol;

  void (*close)( struct device_handle *drive );
  void (*info_mask)( struct drive_info *info );
  int (*driver_cmd)( struct device_handle *drive, const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, const unsigned int timeout );
  int (*cmd)( struct device_handle *drive, const enum cdb_command command, const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], const unsigned int parm_count, const unsigned int timeout );

  unsigned int port; // used by lsi/nvme
  unsigned int hba;  // used by lsi
};

// set the timeout value, the default is 10 seconds
int setTimeout( int timeout );

// These are all Big endian (otherwise they would be directly case ;-) )
unsigned __int128 getu128( unsigned char * );
unsigned long long int getu64( unsigned char * );
unsigned long int getu32( unsigned char * );
unsigned int getu16( unsigned char * );

// Little Endian version, b/c not all int128s are byte aligned (really NVME people, 8 bytes, just 8 bytes)
unsigned __int128 getu128le( unsigned char * );

#endif
