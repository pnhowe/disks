#ifndef _LIBENCLOSURE_H
#define _LIBENCLOSURE_H

#include "enclosure.h"

void set_verbose( const int value );
int get_enclosure_info( const char *device, struct enclosure_info *info, char *errStr );
int get_subenc_descriptors( const char *device, struct subenc_descriptors *descriptors, char *errStr );
int set_subenc_descriptors( const char *device, struct subenc_descriptors *descriptors, char *errStr );


#endif
