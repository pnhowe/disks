#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "scsi.h"
#include "sgio.h"
#include "enclosure.h"

extern int verbose;

extern int cmdTimeout;

int openEnclosure( const char *device, struct device_handle *enclosure )
{
  int rc;
  unsigned char data[512];

  memset( &enclosure->enclosure_info, 0, sizeof( enclosure->enclosure_info ) );

  enclosure->driver = DRIVER_TYPE_SGIO;
  enclosure->protocol = PROTOCOL_TYPE_SCSI;

  if( verbose >= 3 )
    fprintf( stderr, "Opening '%s'...\n", device );

  rc = sgio_open( device, enclosure );

  if( rc )
    return -1;

  //now get the device information

  memset( &data, 0, sizeof( data ) );

  if( verbose >= 2 )
    fprintf( stderr, "IDENTIFY (SES)...\n" );

  rc = exec_cmd_scsi( enclosure, CMD_IDENTIFY_ENCLOSURE, RW_READ, data, sizeof( data ), NULL, 0, cmdTimeout );
  if( rc == -1 )
  {
    if( verbose )
      fprintf( stderr, "IDENTIFY (SES) Failed, rc: %i errno: %i\n", rc, errno );
    return -1;
  }

  memcpy( &( enclosure->enclosure_info ), data, sizeof( enclosure->enclosure_info ) );

  enclosure->enclosure_info.driver = enclosure->driver;
  enclosure->enclosure_info.protocol = enclosure->protocol;

  return 0;
}

void closeEnclosure( struct device_handle *enclosure )
{
  enclosure->close( enclosure );
}

struct enclosure_info *getEnclosureInfo( struct device_handle *enclosure )
{
  return &enclosure->enclosure_info;
}

int getString( struct device_handle *enclosure )
{
  unsigned char tdata[512];
  __u64 tmpParm[2];

  if( !enclosure->enclosure_info.hasDiagnosticPageString )
  {
    if( verbose )
      fprintf( stderr, "Does not support Diagnostic String In/Out\n" );
    errno = EINVAL;
    return -1;
  }

  if( verbose >= 2 )
    fprintf( stderr, "GET STRING PAGE (SES)...\n" );

  tmpParm[0] = 0x04;
  tmpParm[1] = sizeof( tdata );
  memset( tdata, 0, sizeof( tdata ) );
  if( exec_cmd_scsi( enclosure, CMD_SCSI_RECEIVE_DIAGNOSTIC, RW_READ, tdata, sizeof( tdata ), tmpParm, 2, cmdTimeout ) )
    return -1;

  return 0;

}

int getConfigDiagnosticPage( struct device_handle *enclosure, unsigned char *buff, int buff_len )
{
  __u64 tmpParm[2];

  if( !enclosure->enclosure_info.hasDiagnosticPageConfiguration )
  {
    if( verbose )
      fprintf( stderr, "Does not support Diagnostic String In/Out\n" );
    errno = EINVAL;
    return -1;
  }

  if( verbose >= 2 )
    fprintf( stderr, "GET CONFIG DIAG PAGE (SES)...\n" );

  tmpParm[0] = 0x01;
  tmpParm[1] = buff_len;
  memset( buff, 0, buff_len );
  if( exec_cmd_scsi( enclosure, CMD_SCSI_RECEIVE_DIAGNOSTIC, RW_READ, buff, buff_len, tmpParm, 2, cmdTimeout ) )
    return -1;

  return 0;
}

int getStatusDiagnosticPage( struct device_handle *enclosure, unsigned char *buff, int buff_len )
{
  __u64 tmpParm[2];

  if( !enclosure->enclosure_info.hasDiagnosticPageConfiguration )
  {
    if( verbose )
      fprintf( stderr, "Does not support Diagnostic String In/Out\n" );
    errno = EINVAL;
    return -1;
  }

  if( verbose >= 2 )
    fprintf( stderr, "GET STATUS DIAG PAGE (SES)...\n" );

  tmpParm[0] = 0x02;
  tmpParm[1] = buff_len;
  memset( buff, 0, buff_len );
  if( exec_cmd_scsi( enclosure, CMD_SCSI_RECEIVE_DIAGNOSTIC, RW_READ, buff, buff_len, tmpParm, 2, cmdTimeout ) )
    return -1;

  return 0;
}

int setControlDiagnosticPage( struct device_handle *enclosure, unsigned char *buff, int buff_len )
{
  __u64 tmpParm[2];

  if( !enclosure->enclosure_info.hasDiagnosticPageConfiguration )
  {
    if( verbose )
      fprintf( stderr, "Does not support Diagnostic String In/Out\n" );
    errno = EINVAL;
    return -1;
  }

  if( verbose >= 2 )
    fprintf( stderr, "SET CONTROL DIAG PAGE (SES)...\n" );

  tmpParm[0] = 0x10;
  tmpParm[1] = buff_len;
  if( exec_cmd_scsi( enclosure, CMD_SCSI_SEND_DIAGNOSTIC, RW_WRITE, buff, buff_len, tmpParm, 2, cmdTimeout ) )
    return -1;

  return 0;
}
