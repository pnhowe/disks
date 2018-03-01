#ifndef _IDE_H
#define _IDE_H

#include "disk.h"

#define HDIO_DRIVE_CMD_OFFSET  4
#define HDIO_DRIVE_TASK_OFFSET  7

#define BUFFER_LENGTH ( HDIO_DRIVE_TASK_OFFSET + 512 )

#define ATA_TASK 1
#define ATA_CMD 2

#define ATA_IDENTIFY_PACKET_DEVICE      0xa1

int ide_open( const char *device, struct device_handle *drive );
int ide_isvalidname( const char *device );
char *ide_validnames( void );

#endif
