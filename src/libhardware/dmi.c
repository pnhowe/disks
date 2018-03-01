// snippits and general procedure barrowed from dmidecode see dmidecode.c in the dmidecode source
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <linux/types.h>
#include <string.h>
#include <errno.h>

#include "dmi.h"
#include "dmi_decode.h"

extern int verbose;

#define DMI_SIZE 0x10000
#define DMI_OFFSET  0xF0000

#define SUPPORTED_SMBIOS_VER 0x0207

static int checksum( const __u8 *buff, const size_t len )
{
  __u8 sum = 0;
  size_t i;

  for( i = 0; i < len; i++ )
    sum += buff[i];
  return ( sum == 0 );
}

static int legacy( const __u8 *buff, struct dmi_entry list[], const int list_size, int *entry_counter, int *group_counter )
{
  if( !checksum( buff, 0x0F ) )
  {
    errno = EINVAL;
    return -1;
  }

  if( verbose >= 2 )
    fprintf( stderr, "Legacy %u.%u Present\n", buff[0x0E] >> 4, buff[0x0E] & 0x0F );

  if( dmi_table( DWORD( buff + 0x08 ), WORD( buff + 0x06 ), WORD( buff + 0x0C ), ( ( buff[0x0E] & 0xF0 ) << 4 ) + ( buff[0x0E] & 0x0F ), list, list_size, entry_counter, group_counter ) )
    return -1;

  return 0;
}

static int smbios( const __u8 *buff, struct dmi_entry list[], const int list_size, int *entry_counter, int *group_counter )
{
  __u16 version;

  if( !checksum( buff, buff[0x05] ) || ( memcmp( buff + 0x10, "_DMI_", 5) != 0 ) || !checksum( buff + 0x10, 0x0F ) )
  {
    errno = EINVAL;
    return -1;
  }

  version = ( buff[0x06] << 8 ) + buff[0x07];
  /* Some BIOS report weird SMBIOS version, fix that up */
  switch( version )
  {
    case 0x021F:
    case 0x0221:
      version = 0x0203;
      break;
    case 0x0233:
      version = 0x0206;
      break;
  }

  if( verbose >= 2 )
    fprintf( stderr, "SMBIOS %u.%u Present\n", version >> 8, version & 0xFF );

  if( version > SUPPORTED_SMBIOS_VER )
  {
    if( verbose )
      fprintf( stderr, "Unsupported SMBIOS Version.\n" );

    errno = EINVAL;
  }

  if( dmi_table( DWORD( buff + 0x18 ), WORD( buff + 0x16 ), WORD( buff + 0x1C ), version, list, list_size, entry_counter, group_counter ) )
    return -1;

  return 0;
}

int getDMIList( struct dmi_entry list[], const int list_size )
{
  __u8 buff[DMI_SIZE];
  size_t pos;
  int table_counter;
  int entry_counter;
  int group_counter;

  if( readBlock( buff, DMI_OFFSET, DMI_SIZE ) )
    return -1;

  entry_counter = 0;
  group_counter = 0;
  table_counter = 0;

  for( pos = 0; pos <= 0XFFF0; pos += 16 )
  {
    if( !memcmp( buff + pos, "_SM_", 4 ) )
    {
      if( verbose >= 2 )
        fprintf( stderr, "SMBIOS Decodeing at %zi (0x%08zx).\n", pos, pos + DMI_OFFSET );

      if( smbios( buff + pos, list, list_size, &entry_counter, &group_counter ) )
        return -1;

      table_counter++;
      pos += 16;
    }
    else if( !memcmp( buff + pos, "_DMI_", 5 ) )
    {
      if( verbose >= 2 )
        fprintf( stderr, "Legacy Decodeing at %zi (0x%08zx).\n", pos, pos + DMI_OFFSET );

      if( legacy( buff + pos, list, list_size, &entry_counter, &group_counter ) )
        return -1;

      table_counter++;
    }
  }

  if( table_counter == 0 )
  {
    if( verbose )
      fprintf( stderr, "Nothing Found\n" );

    errno = ENOENT;
    return -1;
  }

  if( verbose >= 2 )
    fprintf( stderr, "Found %i Tables\n", table_counter );

  if( verbose >= 2 )
    fprintf( stderr, "Found %i Groups\n", group_counter );

  if( verbose >= 2 )
    fprintf( stderr, "Found %i Entries\n", entry_counter );

  return entry_counter;
}
