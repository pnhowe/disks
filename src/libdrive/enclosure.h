#ifndef _ENCLOSURE_H
#define _ENCLOSURE_H

#include "device.h"

#define HELP_TEXT_LENGTH 50
#define DESCRIPTOR_MAX_COUNT 250

struct subenc_descriptor_element
{
  unsigned char sub_enclosure;
  unsigned char element_index;
  unsigned char element_type;
  unsigned char subelement_index;
  char help_text[ HELP_TEXT_LENGTH ];
  unsigned short value_offset;
  unsigned char value[4];
};

struct subenc_descriptors
{
  unsigned int page_size;
  unsigned int generation;
  unsigned char count;
  struct subenc_descriptor_element descriptors[ DESCRIPTOR_MAX_COUNT ];
};

#define SUBENC_TYPE_POWER_SUPPLY  0x02
#define SUBENC_TYPE_COOLING       0x03
#define SUBENC_TYPE_LCC           0x07
#define SUBENC_TYPE_UPS           0x0b
#define SUBENC_TYPE_CHASSIS       0x0e

#define SUBENC_ELEMENT_TYPE_POWER_SUPPLY  0x02
#define SUBENC_ELEMENT_TYPE_COOLING       0x03
#define SUBENC_ELEMENT_TYPE_TEMP_SENSOR   0x04
#define SUBENC_ELEMENT_TYPE_ENC_SRV_CNTRL 0x07
#define SUBENC_ELEMENT_TYPE_UPS           0x0b
#define SUBENC_ELEMENT_TYPE_DISPLAY       0x0c
#define SUBENC_ELEMENT_TYPE_ENCLOSURE     0x0e
#define SUBENC_ELEMENT_TYPE_LANGUAGE      0x10
#define SUBENC_ELEMENT_TYPE_ARRY_DEV_SLT  0x17
#define SUBENC_ELEMENT_TYPE_SAS_EXPANDER  0x18
#define SUBENC_ELEMENT_TYPE_SAS_CONNECTOR 0x19
#define SUBENC_ELEMENT_TYPE_EXPANDER_PHY  0x81

// open and closing the device, open returns < 0 on error
int openEnclosure( const char *device, struct device_handle *enclosure );
void closeEnclosure( struct device_handle *enclosure );

// enclosure info
struct enclosure_info *getEnclosureInfo( struct device_handle *enclosure );

// Works with Diagnostic Pages
int getStatusDiagnosticPage( struct device_handle *enclosure, unsigned char *buff, int buff_len );
int getConfigDiagnosticPage( struct device_handle *enclosure, unsigned char *buff, int buff_len );
int setControlDiagnosticPage( struct device_handle *enclosure, unsigned char *buff, int buff_len ); // the buff should of come from getConfigStatusDiagnosticPage, with modifications

#endif
