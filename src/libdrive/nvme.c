#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>

#include <linux/nvme_ioctl.h>

#include "nvme.h"

extern int verbose;


void loadInfoNVME( struct device_handle *drive, const struct nvme_identity *identity )
{
  memcpy( &( drive->drive_info ), identity, sizeof( drive->drive_info ) );
}

int exec_cmd_nvme( struct device_handle *drive, const enum cdb_command command, __attribute__((unused)) const enum cdb_rw rw, void *data, const unsigned int data_len, __u64 parms[], __attribute__((unused)) const unsigned int parm_count, const unsigned int timeout )
{
  if( command == CMD_IDENTIFY )
  {
    if( verbose >= 2 )
      fprintf( stderr, "IDENTIFY...\n" );

    if( data_len != sizeof( struct drive_info ) )
    {
      errno = EINVAL;
      return -1;
    }

    struct nvme_cdb cdb;
    struct nvme_identity_controller ident_data_controller;
    struct drive_info *info = ( struct drive_info *) data;

    memset( &cdb, 0, sizeof( cdb ) );
    memset( &ident_data_controller, 0, sizeof( ident_data_controller ) );
    memset( &data, 0, sizeof( data ) );

    cdb.opcode = NVME_OP_AMDIN_GET_LOG_IDENTIFY;
    cdb.nsid = 0;   // want to talk to the controller
    cdb.cdw10 = 1;  // controller data sctucture  see Figure 244

    // this should get us 5.15.2.2 Identify Controller data structure (CNS 01h) - Figure 247

    if( drive->driver_cmd( drive, rw, (unsigned char *)&cdb, sizeof( cdb ), &ident_data_controller, sizeof( ident_data_controller ), timeout ) )
      return -1;

    strncpy( info->serial, ident_data_controller.serial_number, MIN(SERIAL_NUMBER_LEN, NVME_SERIAL_NUMBER_LEN ) );
    strncpy( info->model, ident_data_controller.model_number, MIN(MODEL_NUMBER_LEN, NVME_MODEL_NUMBER_LEN ) );
    strncpy( info->firmware_rev, ident_data_controller.firmware_rev, MIN(FIRMWARE_REV_LEN, NVME_FIRMREV_LEN ) );

    info->supportsSanitize = 1;
    info->supportsTrim = ( ( ident_data_controller.oncs & 0x0004 ) != 0 );  // ie: dataset managment, might also need to check the DMRL and DMRSL
    info->SMARTSupport = 1;
    info->SMARTEnabled = 1;
    info->supportsLBA = 1;
    info->isSSD = 1;
    info->supportsSelfTest = ( ( ident_data_controller.oacs & 0x0010 ) != 0 );

    info->LogicalSectorSize = 512;   // overwrite with ns if there is one
    info->PhysicalSectorSize = 512;
    info->LBACount = 0;              // overwrite with ns if there is one

    info->totalCapacity = getu128le( ident_data_controller.tnvmcap );
    info->unallocatedCapacity = getu128le( ident_data_controller.unvmcap );

    if( drive->port )
    {
      struct nvme_identity_ns ident_data_ns;
      unsigned int index;

      memset( &cdb, 0, sizeof( cdb ) );
      memset( &ident_data_ns, 0, sizeof( ident_data_ns ) );

      cdb.opcode = NVME_OP_AMDIN_GET_LOG_IDENTIFY;
      cdb.nsid = drive->port;  // now the namespace

      if( drive->driver_cmd( drive, rw, (unsigned char *)&cdb, sizeof( cdb ), &ident_data_ns, sizeof( ident_data_ns ), timeout ) )
        return -1;

    // a) UUID field in the Namespace Identification Descriptor (refer to Figure 249), if present;
    // b) NGUID field in the Identify Namespace data (refer to Figure 245) or in the Namespace Identification
    // Descriptor, if present; or
    // c) EUI64 field in the Identify Namespace data or in the Namespace Identification Descriptor, if present.
    // Bit 3 in the NSFEAT field (refer to Figure 245) indicates NGUID and EUI64 reuse characteristics.  <- do we care about the WWN if it keeps changing?

      // 5.15.2.4 Namespace Identification Descriptor list (CNS 03h)
      // else
      info->WWN = getu128( ident_data_ns.nguid );
      if( !info-> WWN )
        info->WWN = getu64( ident_data_ns.eui64 );


      info->LBACount = ident_data_ns.nsze;
      info->nsCapactyLBA = ident_data_ns.ncap;
      info->nsUtilitzationLBA = ident_data_ns.nuse;
      info->supportsThin = ( ( ident_data_ns.nsfeat & 0x01 ) != 0 );

      index = ident_data_ns.flbas & 0x0F;
      info->LogicalSectorSize = pow( 2, ident_data_ns.lbaf[ index ].ds );

      // for( index = 0; index < min( ident_data_ns.nlbaf, 0x0f); i++ )
      // {
      //   look for the best performance one
      // }
    }

    return 0;
  }
  else if( command == CMD_PROBE )
  {
    struct nvme_cdb cdb;
    memset( &cdb, 0, sizeof( cdb ) );

    if( verbose >= 2 )
      fprintf( stderr, "PROBE...\n" );

    //cdb.opcode = NVME_OP_AMDIN_KEEP_ALIVE;  // seems to be optionally supported or only for FC or something
    cdb.opcode = NVME_OP_AMDIN_GET_LOG_IDENTIFY; // would be nice for a light non-returning command to "probe" with
    cdb.nsid = 0;   // want to talk to the controller
    cdb.cdw10 = 1;  // controller data sctucture  see Figure 244

    if( drive->driver_cmd( drive, rw, (unsigned char *)&cdb, sizeof( cdb ), NULL, 0, timeout ) )
      return -1;

    return 0;
  }
  else if( command == CMD_TRIM )
  {
    struct nvme_dsmgnt_range range;
    struct nvme_cdb cdb;
    memset( &cdb, 0, sizeof( cdb ) );
    memset( &range, 0, sizeof( range ) );

    if( verbose >= 2 )
      fprintf( stderr, "TRIM...\n" );

    if( !drive->port )
    {
      errno = EINVAL;
      return -1;
    }

    cdb.opcode = NVME_OP_DS_MANAGMENT;
    cdb.nsid = drive->port;
    cdb.cdw10 = 1;  // nr - number of ranges
    cdb.cdw11 = NVME_DS_MANAGMENT_DEALLOCATE; // attribute

    range.cattr = 0;
    range.nlba = parms[0];
    range.slba = parms[1];

    if( drive->driver_cmd( drive, rw, (unsigned char *)&cdb, sizeof( cdb ), &range, sizeof( range ), timeout ) )
      return -1;

    return 0;
  }

  errno = ENOSYS;
  return -1;
}

// NOTE: NVME dosn't really have a "cdb", so we will use our own struct "nvme_cdb" to transport commands here
int nvme_cmd( struct device_handle *drive, __attribute__((unused)) const enum cdb_rw rw, unsigned char *cdb, const unsigned int cdb_len, void *data, const unsigned int data_len, const unsigned int timeout )
{
  int rc;
  struct nvme_admin_cmd cmd;
  struct nvme_cdb *ncdb = (struct nvme_cdb *)cdb;

  if( cdb_len != sizeof( struct nvme_cdb ) )
  {
    errno = EINVAL;
    return -1;
  }

  memset( &cmd, 0, sizeof( cmd ) );

  // io_hdr.dxfer_direction	= data ? ( ( rw == RW_WRITE ) ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV) : SG_DXFER_NONE;
  // io_hdr.dxfer_len	= data ? data_len : 0;

  cmd.opcode = ncdb->opcode;
  cmd.flags = 0;
  cmd.nsid = ncdb->nsid;
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

/*
struct nvme_passthru_cmd64 {
        __u8    opcode;
        __u8    flags;
        __u16   rsvd1;
        __u32   nsid;
        __u32   cdw2;
        __u32   cdw3;
        __u64   metadata;
        __u64   addr;
        __u32   metadata_len;
        __u32   data_len;
        __u32   cdw10;
        __u32   cdw11;
        __u32   cdw12;
        __u32   cdw13;
        __u32   cdw14;
        __u32   cdw15;
        __u32   timeout_ms;
        __u32   rsvd2;
        __u64   result;
};
*/

  if( verbose >= 4 )
    fprintf( stderr, "NVME: command in opcode: 0x%02x, data_len: 0x%04x, nsid: 0x%08x, cdw: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", cmd.opcode, cmd.data_len, cmd.nsid, cmd.cdw10, cmd.cdw11, cmd.cdw12, cmd.cdw13, cmd.cdw14, cmd.cdw15 );

  rc = ioctl( drive->fd, NVME_IOCTL_ADMIN_CMD, &cmd );
  if( verbose >= 3 )
    fprintf( stderr, "NVME: send returnd: %i, errno: %i\n", rc, errno );

  if( rc != 0 )
  {
    if( verbose >= 2 )
      fprintf( stderr, "NVME: ioctl Failed, rc: %i(0x%04x), errno: %i, result: 0x%08x\n", rc, rc, errno, cmd.result );

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

int nvme_open( const char *device, struct device_handle *drive )  // just over PCI for now, mabey FC later
{
  struct stat statbuf;
  unsigned int nsid;
  char * delim;

  delim = strchr( device, ':' );
  if ( delim )
  {
    delim[0] = '\0';
    nsid = atoi( delim + 1 );
  }
  else
    nsid = 0;

  if( stat( device, &statbuf ) )
  {
    if( verbose )
      fprintf( stderr, "Unable to stat file '%s', errno: %i\n", device, errno );
    return -1;
  }

  if( !S_ISCHR( statbuf.st_mode ) ) // for admin commands it should work for char /dev/nvme0 and block /dev/nvme0n1
  {
    if( verbose )
      fprintf( stderr, "'%s' Is not a char device\n", device );
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
  drive->port = nsid;

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
  return "'/dev/nvme0', '/dev/nvme0:1'";
}
