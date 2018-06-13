#ifndef _LIBDRIVE_H
#define _LIBDRIVE_H

#include "disk.h"

void set_verbose( const int value );
int get_drive_info( const char *device, struct drive_info *info, char *errStr );
int get_smart_attrs( const char *device, struct smart_attribs *attribs, char *errStr );
int smart_status( const char *device, int *deviceFault, int *thresholdExceeded, char *errStr );
int drive_last_selftest_passed( const char *device, int *testPassed, char *errStr );
int log_entry_count( const char *device, int *logCount, char *errStr );

#endif
