#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "device.h"
#include "disk.h"
#include "libdrive.h"

int verbose = 0;

// Need open handle pooling, probably should be done at the python level

int _test_and_open( struct device_handle *drive, const char *device, char *errStr )
{
  if( getuid() != 0 )
  {
    sprintf( errStr, "Must be root.\n" );
    errno = EINVAL;
    return -1;
  }

  if( openDisk( device, drive, DRIVER_TYPE_UNKNOWN, PROTOCOL_TYPE_UNKNOWN, IDENT ) )
  {
    sprintf( errStr, "Error opening device '%s', errno: %i.\n", device, errno );
    return -1;
  }

  return 0;
}

void set_verbose( const int value )
{
  verbose = value;
}

int get_drive_info( const char *device, struct drive_info *info, char *errStr )
{
  struct device_handle drive;
  int rc;

  rc = _test_and_open( &drive, device, errStr );
  if( rc )
    return rc;

  memcpy( info, getDriveInfo( &drive ), sizeof( *info ) );

  closeDisk( &drive );

  return 0;
}

int get_smart_attrs( const char *device, struct smart_attribs *attribs, char *errStr )
{
  struct device_handle drive;
  int rc;

  rc = _test_and_open( &drive, device, errStr );
  if( rc )
    return rc;

  if( smartAttributes( &drive, attribs ) )
  {
    sprintf( errStr, "Error getting atrribs from device '%s', errno: %i.\n", device, errno );
    closeDisk( &drive );
    return -1;
  }

  closeDisk( &drive );

  return 0;
}

int smart_status( const char *device, int *deviceFault, int *thresholdExceeded, char *errStr )
{
  struct device_handle drive;
  int rc;

  rc = _test_and_open( &drive, device, errStr );
  if( rc )
    return rc;

  rc = smartStatus( &drive );
  if( rc == -1 )
  {
    sprintf( errStr, "Error getting status from device '%s', errno: %i.\n", device, errno );
    closeDisk( &drive );
    return -1;
  }

  closeDisk( &drive );

  if( rc & STATUS_DRIVE_FAULT )
    *deviceFault = 1;
  else
    *deviceFault = 0;

  if( rc & STATUS_THRESHOLD_EXCEEDED )
    *thresholdExceeded = 1;
  else
    *thresholdExceeded = 0;

  return 0;
}

int drive_last_selftest_passed( const char *device, int *testPassed, char *errStr )
{
  struct device_handle drive;
  int rc;

  rc = _test_and_open( &drive, device, errStr );
  if( rc )
    return rc;

  rc = smartSelfTestStatus( &drive );
  if( rc == -1 )
  {
    sprintf( errStr, "Error getting selftest info from device '%s', errno: %i.\n", device, errno );
    closeDisk( &drive );
    return -1;
  }

  closeDisk( &drive );

  if( ( rc & SELFTEST_STATUS_MASK ) == SELFTEST_FAILED )
    *testPassed = 0;
  else
    *testPassed = 1;

  return 0;
}

int log_entry_count( const char *device, int *logCount, char *errStr )
{
  struct device_handle drive;
  int rc;

  rc = _test_and_open( &drive, device, errStr );
  if( rc )
    return rc;

  rc = smartLogCount( &drive );
  if( rc == -1 )
  {
    sprintf( errStr, "Error getting long entry count from device '%s', errno: %i.\n", device, errno );
    closeDisk( &drive );
    return -1;
  }

  closeDisk( &drive );

  *logCount = rc;

  return 0;
}
