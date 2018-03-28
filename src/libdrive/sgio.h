#ifndef _SGIO_H
#define _SGIO_H

#include "disk.h"

int sgio_open( const char *device, struct device_handle *drive );
int sgio_isvalidname( const char *device );
char *sgio_validnames( void );

#endif
