#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "device.h"
#include "enclosure.h"
#include "libenclosure.h"

int verbose = 0;

#define MAX_SUBENC 10
#define MAX_DIAG_PAGE_SIZE 2048

#define min(a,b) (((a) < (b)) ? (a) : (b))

int _test_and_open( struct device_handle *encolsure, const char *device, char *errStr )
{
  if( getuid() != 0 )
  {
    sprintf( errStr, "Must be root.\n" );
    errno = EINVAL;
    return -1;
  }

  if( openEnclosure( device, encolsure ) )
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

int get_enclosure_info( const char *device, struct enclosure_info *info, char *errStr )
{
  struct device_handle enclosure;
  int rc;

  rc = _test_and_open( &enclosure, device, errStr );
  if( rc )
    return rc;

  memcpy( info, getEnclosureInfo( &enclosure ), sizeof( *info ) );

  closeEnclosure( &enclosure );

  return 0;
}

int get_subenc_descriptors( const char *device, struct subenc_descriptors *descriptors, char *errStr )
{
  struct device_handle enclosure;
  int rc;

  unsigned char desc_buff[ 2048 ];
  unsigned char diag_buff[ MAX_DIAG_PAGE_SIZE ];
  unsigned int buff_pos, element_offset, text_offset, element_header_count;
  unsigned char sub, sub_count;
  unsigned char i, j;

  unsigned char counts[ MAX_SUBENC ];

  rc = _test_and_open( &enclosure, device, errStr );
  if( rc )
    return rc;

  memset( descriptors, 0, sizeof( *descriptors ) );
  descriptors->count = 0;

  memset( desc_buff, 0, sizeof( desc_buff ) );
  if( getConfigDiagnosticPage( &enclosure, desc_buff, sizeof( desc_buff ) ) )
  {
    sprintf( errStr, "Error getting Config Diag Page, errno: %i.\n", errno );
    closeEnclosure( &enclosure );
    return -1;
  }

  memset( diag_buff, 0, sizeof( diag_buff ) );
  if( getStatusDiagnosticPage( &enclosure, diag_buff, sizeof( diag_buff ) ) )
  {
    sprintf( errStr, "Error getting Status Page, errno: %i.\n", errno );
    closeEnclosure( &enclosure );
    return -1;
  }

  descriptors->page_size = ( diag_buff[2] << 8 ) + diag_buff[3];
  descriptors->generation = ( diag_buff[4] << 24 ) + ( diag_buff[5] << 16 ) + ( diag_buff[6] << 8 ) + diag_buff[7];

  if( descriptors->page_size + 4 > MAX_DIAG_PAGE_SIZE )
  {
    sprintf( errStr, "page_size greater than MAX_DIAG_PAGE_SIZE, page_size:%i\n", ( descriptors->page_size + 4 ) );
    closeEnclosure( &enclosure );
    return -1;
  }

  sub_count = desc_buff[1] + 1; // Sub enc #0 is the Primary LCC, so sub_count + 1

  if( sub_count > MAX_SUBENC )
  {
    sprintf( errStr, "Number of subenc larger than supported, sub_count: %i.\n", sub_count );
    closeEnclosure( &enclosure );
    return -1;
  }

  buff_pos = 8;
  text_offset = 8;
  element_header_count = 0;

  for( sub = 0; sub < sub_count; sub++ )
  {
    counts[ sub ] = desc_buff[ buff_pos + 2 ];
    element_header_count += counts[ sub ];

    text_offset = desc_buff[ buff_pos + 63 ] * 18; // Number of version descriptors (18 bytes each)
    text_offset += desc_buff[ text_offset ] * 4; // Number of buffer Descriptors (4 bytes each)
    text_offset += desc_buff[ text_offset ]; // number of vpd pages (1 byte each)
    text_offset++; // text length

    descriptors->descriptors[ descriptors->count ].sub_enclosure = sub;
    descriptors->descriptors[ descriptors->count ].element_index = 0;
    descriptors->descriptors[ descriptors->count ].element_type = 0;
    descriptors->descriptors[ descriptors->count ].value_offset = 0;
    descriptors->descriptors[ descriptors->count ].subelement_index = 0;
    strncpy( descriptors->descriptors[ descriptors->count ].help_text, (char *) &desc_buff[ text_offset ], min( desc_buff[ text_offset - 1 ], HELP_TEXT_LENGTH ) );
    descriptors->count++;

    buff_pos += desc_buff[ buff_pos + 3 ] + 4;
  }

  text_offset = buff_pos + ( 4 * element_header_count );
  element_offset = 12;
  for( sub = 0; sub < sub_count; sub++ )
  {
    for( i = 0; i < counts[ sub ]; i++ )
    {
      for( j = 0; j < desc_buff[ buff_pos + 1 ]; j++ )
      {
        descriptors->descriptors[ descriptors->count ].sub_enclosure = sub;
        descriptors->descriptors[ descriptors->count ].element_index = i;
        descriptors->descriptors[ descriptors->count ].element_type = desc_buff[ buff_pos ];
        descriptors->descriptors[ descriptors->count ].value_offset = element_offset;
        descriptors->descriptors[ descriptors->count ].subelement_index = j;
        strncpy( descriptors->descriptors[ descriptors->count ].help_text, (char *) &desc_buff[ text_offset ], min( desc_buff[ buff_pos + 3 ], HELP_TEXT_LENGTH ) ); // text isn't right, going to have to put the offset in here, and then when we get to the end of the sub enc, come back and put the text in it's place
        strncpy( (char *) descriptors->descriptors[ descriptors->count ].value, (char *) &diag_buff[ element_offset ], 4 );
        element_offset += 4;
        descriptors->count++;

        if( descriptors->count >= DESCRIPTOR_MAX_COUNT )
        {
          sprintf( errStr, "Number of descriptors larger than supported, count: %i.\n", descriptors->count );
          closeEnclosure( &enclosure );
          return -1;
        }
      }

      element_offset += 4;
      text_offset += desc_buff[ buff_pos + 3 ];
      buff_pos += 4;
    }
  }

  closeEnclosure( &enclosure );

  return 0;
}

int set_subenc_descriptors( const char *device, struct subenc_descriptors *descriptors, char *errStr )
{
  struct device_handle enclosure;
  int rc;

  unsigned char diag_buff[ MAX_DIAG_PAGE_SIZE ];
  unsigned char i;

  if( descriptors->count == 0 ) // it's an error to not set anything, just pretend all is ok
    return 0;

  memset( diag_buff, 0, sizeof( diag_buff ) );
  diag_buff[ 0 ] = 0x02;
  diag_buff[ 2 ] = ( descriptors->page_size & 0xff00 ) >> 8;
  diag_buff[ 3 ] = ( descriptors->page_size & 0x00ff );
  diag_buff[ 4 ] = ( descriptors->generation & 0xff000000 ) >> 24;
  diag_buff[ 5 ] = ( descriptors->generation & 0x00ff0000 ) >> 16;
  diag_buff[ 6 ] = ( descriptors->generation & 0x0000ff00 ) >> 8;
  diag_buff[ 7 ] = ( descriptors->generation & 0x000000ff );

  for( i = 0; i < descriptors->count; i++ )
  {
    strncpy( (char *) &diag_buff[ descriptors->descriptors[ i ].value_offset ], (char *) &descriptors->descriptors[ i ].value, 4 );
  }

  rc = _test_and_open( &enclosure, device, errStr );
  if( rc )
    return rc;

  if( setControlDiagnosticPage( &enclosure, diag_buff, descriptors->page_size + 4 ) )
  {
    sprintf( errStr, "Error setting Status Page, errno: %i.\n", errno );
    closeEnclosure( &enclosure );
    return -1;
  }

  closeEnclosure( &enclosure );

  return 0;
}
