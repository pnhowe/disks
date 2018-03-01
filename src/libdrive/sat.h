#ifndef _SAT_H
#define _SAT_H

#include <linux/types.h>
#include "disk.h"

#define SAT_CDB_LEN 16

#define MAX_DXFER_LEN 512

//   http://tldp.org/HOWTO/SCSI-Generic-HOWTO/scsi_snd_cmd.html
struct linux_ioctl_send_command
{
    int inbufsize;
    int outbufsize;
    __u8 cdb[SAT_CDB_LEN];
    __u8 data[MAX_DXFER_LEN];
};

int sat_open( const char *device, struct device_handle *drive );
int sat_isvalidname( const char *device );
char *sat_validnames( void );

#endif
