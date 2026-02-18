// snippits and general procedure barrowed from pci-utils see proc.c in the pci-utils source
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pci.h"

#define PCI_FUNC(devfn)		((devfn) & 0x07)
#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)

extern int verbose;

#define min(a,b) (((a) < (b)) ? (a) : (b))

// enum and and struct From https://github.com/pciutils/pciutils/blob/master/ls-vpd.c
enum vpd_format {
  F_BINARY,
  F_TEXT,
  F_RESVD,
  F_RDWR,
};

static const struct vpd_item {
  unsigned char id1, id2;
  int format;
  const char *name;
} vpd_items[] = {
  { 'C','P', F_BINARY,	"Extended capability" },
  { 'E','C', F_TEXT,	"Engineering changes" },
  { 'M','N', F_TEXT,	"Manufacture ID" },
  { 'P','N', F_TEXT,	"Part number" },
  { 'R','V', F_RESVD,	"Reserved" },
  { 'R','W', F_RDWR,	"Read-write area" },
  { 'S','N', F_TEXT,	"Serial number" },
  { 'Y','A', F_TEXT,	"Asset tag" },
  { 'V', 0 , F_TEXT,	"Vendor specific" },
  { 'Y', 0 , F_TEXT,	"System specific" },
  /* Non-standard extensions */
  { 'C','C', F_TEXT,	"CCIN" },
  { 'F','C', F_TEXT,	"Feature code" },
  { 'F','N', F_TEXT,	"FRU" },
  { 'N','A', F_TEXT,	"Network address" },
  { 'R','M', F_TEXT,	"Firmware version" },
  { 'Z', 0 , F_TEXT,	"Product specific" },
  {  0,  0 , F_BINARY,	"Unknown" }
};

int getPCIList( struct pci_entry list[], const int list_size )
{
  char *filename = "/proc/bus/pci/devices";
  char buff[1024];
  int counter = 0;
  FILE *fp;
  unsigned int dfn, vend;

  fp = fopen( filename, "r" );
  if( !fp )
  {
    if( verbose )
      fprintf( stderr, "Error opening \"%s\", errno: %i\n", filename, errno );

    return -1;
  }

  while( fgets( buff, sizeof( buff ), fp ) )
  {
    if( sscanf( buff, "%x %x", &dfn, &vend ) != 2 )
      continue;

    if( counter < list_size )
    {
      list[counter].vendor_id = vend >> 16U;
      list[counter].device_id = vend & 0xffff;
      list[counter].bus = dfn >> 8U;
      list[counter].device = PCI_SLOT( dfn & 0xff );
      list[counter].function = PCI_FUNC( dfn & 0xff );
    }
    counter++;
  }

  fclose( fp );

  return counter - 1;
}

// https://ics.uci.edu/~iharris/ics216/pci/PCI_22.pdf
// https://github.com/pciutils/pciutils/blob/master/ls-vpd.c

int getVPDInfo( struct pci_entry *entry, struct vpd_entry list[], const int list_size )
{
  char filename[1024];
  int counter = 0;
  int rc;
  int fd;
  unsigned char buff[1024];
  unsigned char id;
  unsigned int len;

  //  Domain:Bus:Device.Function
  snprintf( filename, sizeof( filename ), "/sys/bus/pci/devices/%04x:%02x:%02x.%01x/vpd", entry->domain, entry->bus, entry->device, entry->function );

  memset( list, 0, sizeof( *list ) ); // make sure all the strings are \0 teminated, just in case

  fd = open( filename, O_RDONLY );
  if( fd == -1 )
  {
    if( errno == ENOENT )
      return 0;

    if( verbose )
      fprintf( stderr, "Error opening VPD \"%s\", errno: %i\n", filename, errno );

    return -1;
  }

  while( 1 )
  {
    if( read( fd, &id, 1 ) != 1 )
    {
      if( verbose )
        fprintf( stderr, "Error Reading next Id, errno: %i\n", errno );

      goto err;
    }

    if( id == 0x78 )
    {
      // this is the "normal" way out
      goto done;
    }

    if( read( fd, buff, 2 ) != 2 )
    {
      if( verbose )
        fprintf( stderr, "Error Reading next Len, errno: %i\n", errno );

      goto err;
    }

    len = buff[0] + ( buff[1] << 8 );

    if( id == 0x82 )
    {
      if( counter < list_size )
      {
        strcpy( list[counter].id, "ID" );  // ID is not a VPD flag, so we will barrow it to make everything fit in the same struct
        rc = read( fd, list[counter].value, min( len, sizeof( list[counter].value ) - 1 ) );
        if( rc < (int) len )
          lseek( fd, len - rc, SEEK_CUR );
      }
      else
        lseek( fd, len, SEEK_CUR );

      counter++;
    }
    else if( id == 0x90 || id == 0x91 )
    {
      if( read( fd, buff, len ) != len )
      {
        fprintf( stderr, "Error reading VPD tags" );
        goto err;
      }
      unsigned int pos = 0;
      unsigned char tag_len = 0;
      while( pos < len )
      {
        unsigned char id1 = buff[pos];
        unsigned char id2 = buff[pos + 1];
        tag_len = buff[pos + 2];

        if( counter < list_size )
        {
          const struct vpd_item *item;

          for( item = vpd_items; ( item->id1 && ( item->id1 != id1 ) ) ||
                                 ( item->id2 && ( item->id2 != id2 ) ); item++)
            ;

          if( item->format == F_TEXT )
          {
            strncpy( list[counter].id, ( char * ) &buff[pos], 2 );  // ID is not a VPD flag, so we will barrow it to make everything fit in the same struct
            strncpy( list[counter].value, ( char * ) &buff[pos + 3], tag_len );
          }
        }

        pos += 3;
        pos += tag_len;
        counter++;
      }
    }
    else
    {
      if( verbose )
        fprintf( stderr, "Unknown ID 0x%02x\n", id );

      goto err;
    }
  }

  done:
    close( fd );

    return counter - 1;

  err:
    close( fd );

    return -1;
}
