#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "dmi_decode.h"

extern int verbose;

#define TABLE_LOOKUP( __NAME__, __TABLE__, __MIN__, __MAX__ ) static const char *__NAME__( int code )\
{ \
  if( code >= __MIN__ && code <= __MAX__ ) \
    return __TABLE__[ code - __MIN__ ]; \
  return "OUT_OF_SPEC"; \
}

static const char *base_board_type[] = {
        "Unknown", /* 0x01 */
        "Other",
        "Server Blade",
        "Connectivity Switch",
        "System Management Module",
        "Processor Module",
        "I/O Module",
        "Memory Module",
        "Daughter Board",
        "Motherboard",
        "Processor+Memory Module",
        "Processor+I/O Module",
        "Interconnect Board" /* 0x0D */
};

static const char *chassis_type[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Desktop",
        "Low Profile Desktop",
        "Pizza Box",
        "Mini Tower",
        "Tower",
        "Portable",
        "Laptop",
        "Notebook",
        "Hand Held",
        "Docking Station",
        "All In One",
        "Sub Notebook",
        "Space-saving",
        "Lunch Box",
        "Main System Chassis",
        "Expansion Chassis",
        "Sub Chassis",
        "Bus Expansion Chassis",
        "Peripheral Chassis",
        "RAID Chassis",
        "Rack Mount Chassis",
        "Sealed-case PC",
        "Multi-system",
        "CompactPCI",
        "AdvancedTCA",
        "Blade",
        "Blade Enclosing" /* 0x1D */
};

static const char *chassis_state[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Safe",
        "Warning",
        "Critical",
        "Non-recoverable" /* 0x06 */
};

static const struct {
        int value;
        const char *name;
} processor_family[] = {
        { 0x01, "Other" },
        { 0x02, "Unknown" },
        { 0x03, "8086" },
        { 0x04, "80286" },
        { 0x05, "80386" },
        { 0x06, "80486" },
        { 0x07, "8087" },
        { 0x08, "80287" },
        { 0x09, "80387" },
        { 0x0A, "80487" },
        { 0x0B, "Pentium" },
        { 0x0C, "Pentium Pro" },
        { 0x0D, "Pentium II" },
        { 0x0E, "Pentium MMX" },
        { 0x0F, "Celeron" },
        { 0x10, "Pentium II Xeon" },
        { 0x11, "Pentium III" },
        { 0x12, "M1" },
        { 0x13, "M2" },
        { 0x14, "Celeron M" },
        { 0x15, "Pentium 4 HT" },
        { 0x18, "Duron" },

        { 0x19, "K5" },
        { 0x1A, "K6" },
        { 0x1B, "K6-2" },
        { 0x1C, "K6-3" },
        { 0x1D, "Athlon" },
        { 0x1E, "AMD29000" },
        { 0x1F, "K6-2+" },
        { 0x20, "Power PC" },
        { 0x21, "Power PC 601" },
        { 0x22, "Power PC 603" },
        { 0x23, "Power PC 603+" },
        { 0x24, "Power PC 604" },
        { 0x25, "Power PC 620" },
        { 0x26, "Power PC x704" },
        { 0x27, "Power PC 750" },
        { 0x28, "Core Duo" },
        { 0x29, "Core Duo Mobile" },
        { 0x2A, "Core Solo Mobile" },
        { 0x2B, "Atom" },

        { 0x30, "Alpha or Pentium Pro" },
        { 0x31, "Alpha 21064" },
        { 0x32, "Alpha 21066" },
        { 0x33, "Alpha 21164" },
        { 0x34, "Alpha 21164PC" },
        { 0x35, "Alpha 21164a" },
        { 0x36, "Alpha 21264" },
        { 0x37, "Alpha 21364" },
        { 0x38, "Turion II Ultra Dual-Core Mobile M" },
        { 0x39, "Turion II Dual-Core Mobile M" },
        { 0x3A, "Athlon II Dual-Core M" },
        { 0x3B, "Opteron 6100" },
        { 0x3C, "Opteron 4100" },

        { 0x40, "MIPS" },
        { 0x41, "MIPS R4000" },
        { 0x42, "MIPS R4200" },
        { 0x43, "MIPS R4400" },
        { 0x44, "MIPS R4600" },
        { 0x45, "MIPS R10000" },

        { 0x50, "SPARC" },
        { 0x51, "SuperSPARC" },
        { 0x52, "MicroSPARC II" },
        { 0x53, "MicroSPARC IIep" },
        { 0x54, "UltraSPARC" },
        { 0x55, "UltraSPARC II" },
        { 0x56, "UltraSPARC IIi" },
        { 0x57, "UltraSPARC III" },
        { 0x58, "UltraSPARC IIIi" },

        { 0x60, "68040" },
        { 0x61, "68xxx" },
        { 0x62, "68000" },
        { 0x63, "68010" },
        { 0x64, "68020" },
        { 0x65, "68030" },

        { 0x70, "Hobbit" },

        { 0x78, "Crusoe TM5000" },
        { 0x79, "Crusoe TM3000" },
        { 0x7A, "Efficeon TM8000" },

        { 0x80, "Weitek" },

        { 0x82, "Itanium" },
        { 0x83, "Athlon 64" },
        { 0x84, "Opteron" },
        { 0x85, "Sempron" },
        { 0x86, "Turion 64" },
        { 0x87, "Dual-Core Opteron" },
        { 0x88, "Athlon 64 X2" },
        { 0x89, "Turion 64 X2" },
        { 0x8A, "Quad-Core Opteron" },
        { 0x8B, "Third-Generation Opteron" },
        { 0x8C, "Phenom FX" },
        { 0x8D, "Phenom X4" },
        { 0x8E, "Phenom X2" },
        { 0x8F, "Athlon X2" },
        { 0x90, "PA-RISC" },
        { 0x91, "PA-RISC 8500" },
        { 0x92, "PA-RISC 8000" },
        { 0x93, "PA-RISC 7300LC" },
        { 0x94, "PA-RISC 7200" },
        { 0x95, "PA-RISC 7100LC" },
        { 0x96, "PA-RISC 7100" },

        { 0xA0, "V30" },
        { 0xA1, "Quad-Core Xeon 3200" },
        { 0xA2, "Dual-Core Xeon 3000" },
        { 0xA3, "Quad-Core Xeon 5300" },
        { 0xA4, "Dual-Core Xeon 5100" },
        { 0xA5, "Dual-Core Xeon 5000" },
        { 0xA6, "Dual-Core Xeon LV" },
        { 0xA7, "Dual-Core Xeon ULV" },
        { 0xA8, "Dual-Core Xeon 7100" },
        { 0xA9, "Quad-Core Xeon 5400" },
        { 0xAA, "Quad-Core Xeon" },
        { 0xAB, "Dual-Core Xeon 5200" },
        { 0xAC, "Dual-Core Xeon 7200" },
        { 0xAD, "Quad-Core Xeon 7300" },
        { 0xAE, "Quad-Core Xeon 7400" },
        { 0xAF, "Multi-Core Xeon 7400" },
        { 0xB0, "Pentium III Xeon" },
        { 0xB1, "Pentium III Speedstep" },
        { 0xB2, "Pentium 4" },
        { 0xB3, "Xeon" },
        { 0xB4, "AS400" },
        { 0xB5, "Xeon MP" },
        { 0xB6, "Athlon XP" },
        { 0xB7, "Athlon MP" },
        { 0xB8, "Itanium 2" },
        { 0xB9, "Pentium M" },
        { 0xBA, "Celeron D" },
        { 0xBB, "Pentium D" },
        { 0xBC, "Pentium EE" },
        { 0xBD, "Core Solo" },
        { 0xBE, "Core 2 or K7" },
        { 0xBF, "Core 2 Duo" },
        { 0xC0, "Core 2 Solo" },
        { 0xC1, "Core 2 Extreme" },
        { 0xC2, "Core 2 Quad" },
        { 0xC3, "Core 2 Extreme Mobile" },
        { 0xC4, "Core 2 Duo Mobile" },
        { 0xC5, "Core 2 Solo Mobile" },
        { 0xC6, "Core i7" },
        { 0xC7, "Dual-Core Celeron" },
        { 0xC8, "IBM390" },
        { 0xC9, "G4" },
        { 0xCA, "G5" },
        { 0xCB, "ESA/390 G6" },
        { 0xCC, "z/Architectur" },
        { 0xCD, "Core i5" },
        { 0xCE, "Core i3" },

        { 0xD2, "C7-M" },
        { 0xD3, "C7-D" },
        { 0xD4, "C7" },
        { 0xD5, "Eden" },
        { 0xD6, "Multi-Core Xeon" },
        { 0xD7, "Dual-Core Xeon 3xxx" },
        { 0xD8, "Quad-Core Xeon 3xxx" },
        { 0xD9, "Nano" },
        { 0xDA, "Dual-Core Xeon 5xxx" },
        { 0xDB, "Quad-Core Xeon 5xxx" },

        { 0xDD, "Dual-Core Xeon 7xxx" },
        { 0xDE, "Quad-Core Xeon 7xxx" },
        { 0xDF, "Multi-Core Xeon 7xxx" },
        { 0xE0, "Multi-Core Xeon 3400" },

        { 0xE6, "Embedded Opteron Quad-Core" },
        { 0xE7, "Phenom Triple-Core" },
        { 0xE8, "Turion Ultra Dual-Core Mobile" },
        { 0xE9, "Turion Dual-Core Mobile" },
        { 0xEA, "Athlon Dual-Core" },
        { 0xEB, "Sempron SI" },
        { 0xEC, "Phenom II" },
        { 0xED, "Athlon II" },
        { 0xEE, "Six-Core Opteron" },
        { 0xEF, "Sempron M" },

        { 0xFA, "i860" },
        { 0xFB, "i960" },

        { 0x104, "SH-3" },
        { 0x105, "SH-4" },
        { 0x118, "ARM" },
        { 0x119, "StrongARM" },
        { 0x12C, "6x86" },
        { 0x12D, "MediaGX" },
        { 0x12E, "MII" },
        { 0x140, "WinChip" },
        { 0x15E, "DSP" },
        { 0x1F4, "Video Processor" },
};

static const char *processor_status[] = {
        "Unknown", /* 0x00 */
        "Enabled",
        "Disabled By User",
        "Disabled By BIOS",
        "Idle",
        "OUT_OF_SPEC",
        "OUT_OF_SPEC",
        "Other" /* 0x07 */
};

static const char *log_descriptor_type[] = {
        NULL, /* 0x00 */
        "Single-bit ECC memory error",
        "Multi-bit ECC memory error",
        "Parity memory error",
        "Bus timeout",
        "I/O channel block",
        "Software NMI",
        "POST memory resize",
        "POST error",
        "PCI parity error",
        "PCI system error",
        "CPU failure",
        "EISA failsafe timer timeout",
        "Correctable memory log disabled",
        "Logging disabled",
        NULL, /* 0x0F */
        "System limit exceeded",
        "Asynchronous hardware timer expired",
        "System configuration information",
        "Hard disk information",
        "System reconfigured",
        "Uncorrectable CPU-complex error",
        "Log area reset/cleared",
        "System boot" /* 0x17 */
};

static const char *event_log_status_valid[] = {
        "Invalid", /* 0 */
        "Valid" /* 1 */
};
static const char *event_log_status_full[] = {
        "Not Full", /* 0 */
        "Full" /* 1 */
};

static const char *log_descriptor_format[] = {
        "None", /* 0x00 */
        "Handle",
        "Multiple-event",
        "Multiple-event handle",
        "POST results bitmap",
        "System management",
        "Multiple-event system management" /* 0x06 */
};

static const char *memory_module_type[] = {
        "Other", /* 0 */
        "Unknown",
        "Standard",
        "FPM",
        "EDO",
        "Parity",
        "ECC",
        "SIMM",
        "DIMM",
        "Burst EDO",
        "SDRAM" /* 10 */
};

static const char *memory_device_type[] = {
        "Other", /* 0x01 */
        "Unknown",
        "DRAM",
        "EDRAM",
        "VRAM",
        "SRAM",
        "RAM",
        "ROM",
        "Flash",
        "EEPROM",
        "FEPROM",
        "EPROM",
        "CDRAM",
        "3DRAM",
        "SDRAM",
        "SGRAM",
        "RDRAM",
        "DDR",
        "DDR2",
        "DDR2 FB-DIMM",
        "Reserved",
        "Reserved",
        "Reserved",
        "DDR3",
        "FBD2", /* 0x19 */
};

static const char *memory_error_type[] = {
        "Other", /* 0x01 */
        "Unknown",
        "OK",
        "Bad Read",
        "Parity Error",
        "Single-bit Error",
        "Double-bit Error",
        "Multi-bit Error",
        "Nibble Error",
        "Checksum Error",
        "CRC Error",
        "Corrected Single-bit Error",
        "Corrected Error",
        "Uncorrectable Error" /* 0x0E */
};

static const char *memory_error_granularity[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Device Level",
        "Memory Partition Level" /* 0x04 */
};

static const char *memory_error_operation[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Read",
        "Write",
        "Partial Write" /* 0x05 */
};

static const char *voltage_probe_location[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Processor",
        "Disk",
        "Peripheral Bay",
        "System Management Module",
        "Motherboard",
        "Memory Module",
        "Processor Module",
        "Power Unit",
        "Add-in Card" /* 0x0B */
};

static const char *probe_status[] = {
        "Other", /* 0x01 */
        "Unknown",
        "OK",
        "Non-critical",
        "Critical",
        "Non-recoverable" /* 0x06 */
};

static const char *cooling_device_type[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Fan",
        "Centrifugal Blower",
        "Chip Fan",
        "Cabinet Fan",
        "Power Supply Fan",
        "Heat Pipe",
        "Integrated Refrigeration",
        "Active Cooling",
        "Passive Cooling" /* 0x11 */
};

static const char *temperature_probe_location[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Processor",
        "Disk",
        "Peripheral Bay",
        "System Management Module",
        "Motherboard",
        "Memory Module",
        "Processor Module",
        "Power Unit",
        "Add-in Card",
        "Front Panel Board",
        "Back Panel Board",
        "Power System Board",
        "Drive Back Plane" /* 0x0F */
};

static const char *power_supply_type[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Linear",
        "Switching",
        "Battery",
        "UPS",
        "Converter",
        "Regulator" /* 0x08 */
};

static const char *power_supply_status[] = {
        "Other", /* 0x01 */
        "Unknown",
        "OK",
        "Non-critical",
        "Critical" /* 0x05 */
};

static const char *power_supply_range_switching[] = {
        "Other", /* 0x01 */
        "Unknown",
        "Manual",
        "Auto-switch",
        "Wide Range",
        "N/A" /* 0x06 */
};

static void dump_bytes( const __u8 *p, const int len )
{
  int i, j;

  for( i = 0; i < len; i += 20 )
  {
    fprintf( stderr, "    %4i: ", i );
    for( j = 0; ( j < 20 ) && ( ( i + j ) < len ); j++ )
      fprintf( stderr, "%02x ", p[i + j] );
    for( ; j < 20; j++ )
      fprintf( stderr, "   " );
    fprintf( stderr, "     |" );
    for( j = 0; ( j < 20 ) && ( ( i + j ) < len ); j++ )
    {
      if( isprint( p[i + j] ) )
        fprintf( stderr, "%c", p[i + j] );
      else
        fprintf( stderr, " " );
    }
    for( ; j < 20; j++ )
      fprintf( stderr, " " );
    fprintf( stderr, "|\n" );
  }
}

static inline void to_dmi_header( struct dmi_header *hdr, const __u8 *data )
{
  hdr->type = data[0];
  hdr->length = data[1];
  hdr->handle = WORD( data + 2 );
  hdr->data = ( __u8 *) data;
}

static char *dmi_string( const struct dmi_header *hdr, __u8 id )
{
  char *str = (char *) hdr->data;

  if( id == 0 )
    return "Not Specified";

  str += hdr->length;

  while( id > 1 && *str )
  {
    str += strlen( str ) + 1;
    id--;
  }

  if( !*str )
    return "ERR";

  return str;
}

static void dmi_system_uuid( char *wrkBuff, const __u8 *p, const int ver )
{
  int only0xFF = 1, only0x00 = 1;
  int i;

  for( i = 0; i < 16 && ( only0x00 || only0xFF ); i++ )
  {
    if( p[i] != 0x00 )
      only0x00 = 0;
    if( p[i] != 0xFF )
      only0xFF = 0;
  }

  if( only0xFF )
  {
    strcpy( wrkBuff, "Not Present" );
    return;
  }
  if (only0x00)
  {
    strcpy( wrkBuff, "Not Settable" );
    return;
  }

  /*
  for the reson behid the versino check, see dmiddecodes's source
  */
  if (ver >= 0x0206)
    sprintf( wrkBuff, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                      p[3], p[2], p[1], p[0], p[5], p[4], p[7], p[6],
                      p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
  else
    sprintf( wrkBuff, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                      p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                      p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}

TABLE_LOOKUP( dmi_base_board_type, base_board_type, 0x01, 0x0d )

TABLE_LOOKUP( dmi_chassis_type, chassis_type, 0x01, 0x1d )

TABLE_LOOKUP( dmi_chassis_state, chassis_state, 0x01, 0x06 )

static const char *dmi_processor_family( int code )
{
  size_t i;

  for( i = 0; i < ARRAY_SIZE( processor_family ); i++ )
  {
    if( processor_family[i].value == code )
      return processor_family[i].name;
  }

  return "OUT_OF_SPEC";
}

static void dmi_memory_module_type( char *wrkBuff, __u16 code )
{
  int i;

  if( code  == 0 )
    strcpy( wrkBuff, "None" );
  else
  {
    wrkBuff[0] = '\0';
    for( i = 0; i <= 10; i++ )
      if( code & ( 1 << i ) )
      {
        strcat( wrkBuff, " " );
        strcat( wrkBuff, memory_module_type[i] );
      }
  }
}

static void dmi_memory_module_size( char *wrkBuff, __u8 code )
{
  switch( code & 0x7F )
  {
    case 0x7D:
      strcpy( wrkBuff, "Not Determinable" );
      break;

    case 0x7E:
      strcpy( wrkBuff, "Disabled" );
      break;

    case 0x7F:
      strcpy( wrkBuff, "Not Installed" );
      return;

    default:
      sprintf( wrkBuff, "%u MB", 1 << ( code & 0x7F ) );
  }

  if( code & 0x80 )
    strcat( wrkBuff, " (Double-bank)" );
  else
    strcat( wrkBuff, " (Single-bank)" );
}

static void dmi_memory_module_error( char *wrkBuff, __u8 code )
{
  if( code & ( 1 << 2 ) )
      strcpy( wrkBuff, "See Event Log" );
  else
  {
    if( ( code & 0x03 ) == 0 )
      strcpy( wrkBuff, "OK" );
    if( code &  (1 << 0 ) )
      strcpy( wrkBuff, "Uncorrectable Errors" );
    if( code & ( 1 << 1 ) )
      strcpy( wrkBuff, "Correctable Errors" );
  }
}

static void dmi_event_log_status( char *wrkBuff, __u8 code )
{
  sprintf( wrkBuff, "%s, %s", event_log_status_valid[( code >> 0 ) & 1], event_log_status_full[( code >> 1 ) & 1] );
}

static const char *dmi_event_log_descriptor_type( __u8 code )
{
  if( code <= 0x17 && log_descriptor_type[code] != NULL)
    return log_descriptor_type[code];
  if( code >= 0x80 && code <= 0xFE )
    return "OEM-specific";
  if( code == 0xFF )
    return "End of log";
  return "OUT_OF_SPEC";
}

static const char *dmi_event_log_descriptor_format( __u8 code )
{
  if( code <= 0x06 )
    return log_descriptor_format[code];
  if( code >= 0x80 )
    return "OEM-specific";
  return "OUT_OF_SPEC";
}

static void dmi_memory_device_size( char *wrkBuff, __u16 code )
{
  if( code == 0 )
    strcpy( wrkBuff, "No Module Installed" );
  else if ( code == 0xFFFF )
    strcpy( wrkBuff, "Unknown" );
  else
  {
    if( code & 0x8000 )
      sprintf( wrkBuff, "%u kB", code & 0x7FFF );
    else
      sprintf( wrkBuff, "%u MB", code );
  }
}

static void dmi_memory_device_extended_size( char *wrkBuff, __u32 code )
{
  code &= 0x7FFFFFFFUL;

  /* Use the most suitable unit depending on size */
  if( code & 0x3FFUL )
    sprintf( wrkBuff, "%lu MB", ( unsigned long )code );
  else if( code & 0xFFFFFUL )
    sprintf( wrkBuff, "%lu GB", ( unsigned long )code >> 10 );
  else
    sprintf( wrkBuff, "%lu TB", ( unsigned long )code >> 20 );
}

static void dmi_memory_device_set( char *wrkBuff, __u8 code )
{
  if( code == 0 )
    strcpy( wrkBuff, "None" );
  else if( code == 0xFF )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%u", code );
}

TABLE_LOOKUP( dmi_memory_device_type, memory_device_type, 0x01, 0x19 )

static void dmi_memory_device_speed( char *wrkBuff, __u16 code )
{
  if( code == 0 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%u MHz", code );
}

TABLE_LOOKUP( dmi_memory_error_type, memory_error_type, 0x01, 0x0e )

TABLE_LOOKUP( dmi_memory_error_granularity, memory_error_granularity, 0x01, 0x04 )

TABLE_LOOKUP( dmi_memory_error_operation, memory_error_operation, 0x01, 0x05 )

static void dmi_memory_error_syndrome( char *wrkBuff, __u32 code )
{
  if( code == 0x00000000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "0x%08x", code );
}

static void dmi_32bit_memory_error_address( char *wrkBuff, __u32 code )
{
  if( code == 0x80000000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "0x%08x", code );
}

static void dmi_64bit_memory_error_address( char *wrkBuff, __u64 code )
{
  if( code == 0x8000000000000000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "0x%016llx", code );
}

TABLE_LOOKUP( dmi_voltage_probe_location, voltage_probe_location, 0x01, 0x0b )

TABLE_LOOKUP( dmi_probe_status, probe_status, 0x01, 0x06 )

static void dmi_voltage_probe_value( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%.3f V", ( (float) code ) / 1000.0 );
}

static void dmi_voltage_probe_resolution( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%.1f mV", ( (float) code ) / 10.0 );
}

static void dmi_probe_accuracy( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%.2f%%", ( (float) code ) / 100.0 );
}

TABLE_LOOKUP( dmi_cooling_device_type, cooling_device_type, 0x01, 0x11 )

static void dmi_cooling_device_speed( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown Or Non-rotating" );
  else
    sprintf( wrkBuff, "%u RPM", code );
}

TABLE_LOOKUP( dmi_temperature_probe_location, temperature_probe_location, 0x01, 0x0f )

static void dmi_temperature_probe_value( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%.1f deg C", ( (float) code ) / 10.0 );
}

static void dmi_temperature_probe_resolution( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%.3f deg C", ( (float) code ) / 1000.0 );
}

static void dmi_current_probe_value( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%.3f A", ( (float) code ) / 1000.0 );
}

static void dmi_current_probe_resolution( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%.1f mA", ( (float) code ) / 10.0 );
}

static void dmi_power_supply_power( char *wrkBuff, __u16 code )
{
  if( code == 0x8000 )
    strcpy( wrkBuff, "Unknown" );
  else
    sprintf( wrkBuff, "%u W", code);
}

TABLE_LOOKUP( dmi_power_supply_type, power_supply_type, 0x01, 0x08 )

TABLE_LOOKUP( dmi_power_supply_status, power_supply_status, 0x01, 0x05 )

TABLE_LOOKUP( dmi_power_supply_range_switching, power_supply_range_switching, 0x01, 0x06 )

inline static void addEntry( const char *type, const char *name, const char *value, struct dmi_entry list[], const int list_size, int *counter, const int group )
{
  if( *counter < list_size )
  {
    list[*counter].group = group;
    strncpy( list[*counter].type, type, DMI_ENTRY_TYPE_LEN - 1 );
    strncpy( list[*counter].name, name, DMI_ENTRY_NAME_LEN - 1 );
    strncpy( list[*counter].value, value, DMI_ENTRY_VALUE_LEN - 1 );
    if( verbose >= 3 )
      fprintf( stderr, "Added Entry %i, type: '%s', name: '%s', value: '%s'\n", *counter, list[*counter].type, list[*counter].name, list[*counter].value );
  }
  (*counter)++;
}

static void dmi_decode( const struct dmi_header *hdr, const int version, struct dmi_entry list[], const int list_size, int *counter, const int group )
{
  char wrkValue[DMI_ENTRY_VALUE_LEN];
  int i;

  switch( hdr->type )
  {
    case 0:  // section 7.1   Only One
      addEntry( "BIOS Info", "Vendor", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "BIOS Info", "Version", dmi_string( hdr, hdr->data[0x05] ), list, list_size, counter, group );
      addEntry( "BIOS Info", "Release Date", dmi_string( hdr, hdr->data[0x08] ), list, list_size, counter, group );

      if( hdr->length < 0x18 )
        break;
      if( ( hdr->data[0x14] != 0xFF ) && ( hdr->data[0x15] != 0xFF ) )
      {
        sprintf( wrkValue, "%u.%u", hdr->data[0x14], hdr->data[0x15] );
        addEntry( "BIOS Info", "BIOS Revision", wrkValue, list, list_size, counter, group );
      }
      if( ( hdr->data[0x16] != 0xFF ) && ( hdr->data[0x17] != 0xFF ) )
      {
        sprintf( wrkValue, "%u.%u", hdr->data[0x16], hdr->data[0x17] );
        addEntry( "BIOS Info", "Firmware Revision", wrkValue, list, list_size, counter, group );
      }

      break;

    case 1:  // section 7.2   Only One
      if( hdr->length < 0x08 )
        break;
      addEntry( "System Info", "Manufacturer", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "System Info", "Product Name", dmi_string( hdr, hdr->data[0x05] ), list, list_size, counter, group );
      addEntry( "System Info", "Version", dmi_string( hdr, hdr->data[0x06] ), list, list_size, counter, group );
      addEntry( "System Info", "Serial Number", dmi_string( hdr, hdr->data[0x07] ), list, list_size, counter, group );

      if( hdr->length < 0x19 )
        break;
      dmi_system_uuid( wrkValue, hdr->data + 0x08, version );
      addEntry( "System Info", "UUID", wrkValue, list, list_size, counter, group );

      if( hdr->length < 0x1b )
        break;
      addEntry( "System Info", "SKU Number", dmi_string( hdr, hdr->data[0x19] ), list, list_size, counter, group );
      addEntry( "System Info", "Family", dmi_string( hdr, hdr->data[0x1A] ), list, list_size, counter, group );

      break;

    case 2: // section 7.3
      if( hdr->length < 0x08 )
        break;
      addEntry( "Base Board Information", "Manufacturer", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "Base Board Information", "Product Name", dmi_string( hdr, hdr->data[0x05] ), list, list_size, counter, group );
      addEntry( "Base Board Information", "Version", dmi_string( hdr, hdr->data[0x06] ), list, list_size, counter, group );
      addEntry( "Base Board Information", "Serial Number", dmi_string( hdr, hdr->data[0x07] ), list, list_size, counter, group );

      if( hdr->length < 0x09 )
        break;
      addEntry( "Base Board Information", "Asset Tag", dmi_string( hdr, hdr->data[0x08] ), list, list_size, counter, group );

      if( hdr->length < 0x0E )
        break;
      addEntry( "Base Board Information", "Type", dmi_base_board_type( hdr->data[0x0D] ), list, list_size, counter, group );

      break;

    case 3: // section 7.4
      if( hdr->length < 0x09 )
        break;
      addEntry( "Chassis Information", "Manufacturer", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "Chassis Information", "Type", dmi_chassis_type( hdr->data[0x05] & 0x7F ), list, list_size, counter, group );
      addEntry( "Chassis Information", "Version", dmi_string( hdr, hdr->data[0x06] ), list, list_size, counter, group );
      addEntry( "Chassis Information", "Serial Number", dmi_string( hdr, hdr->data[0x07] ), list, list_size, counter, group );
      addEntry( "Chassis Information", "Asset Tag", dmi_string( hdr, hdr->data[0x08] ), list, list_size, counter, group );

      if( hdr->length < 0x0D )
        break;
      addEntry( "Chassis Information", "Boot-up State", dmi_chassis_state( hdr->data[0x09] ), list, list_size, counter, group );
      addEntry( "Chassis Information", "Power Supply State", dmi_chassis_state( hdr->data[0x0A] ), list, list_size, counter, group );
      addEntry( "Chassis Information", "Thermal State", dmi_chassis_state( hdr->data[0x0B] ), list, list_size, counter, group );

      if( hdr->length < 0x13 )
        break;
      sprintf( wrkValue, "%u", hdr->data[0x11] );
      addEntry( "Chassis Information", "Height", wrkValue, list, list_size, counter, group );
      sprintf( wrkValue, "%u", hdr->data[0x12] );
      addEntry( "Chassis Information", "Number Of Power Cords", wrkValue, list, list_size, counter, group );

      if( hdr->length < ( 0x16 + hdr->data[0x13] * hdr->data[0x14] ) )
        break;
      addEntry( "Chassis Information", "SKU Number", dmi_string( hdr, hdr->data[0x15 + hdr->data[0x13] * hdr->data[0x14] ] ), list, list_size, counter, group );

      break;

    case 4: // section 7.5
      if( hdr->length < 0x1A )
        break;

      addEntry( "Processor Information", "Socket Designation", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );

      if( !( hdr->data[0x18] & (1 << 6) ) ) // Socket is empty
        break;

      if( ( hdr->data[0x06] == 0xFE ) && ( hdr->length >= 0x2A ) )
        addEntry( "Processor Information", "Family", dmi_processor_family( WORD( hdr->data + 0x28 ) ), list, list_size, counter, group );
      else
        addEntry( "Processor Information", "Family", dmi_processor_family( hdr->data[0x06] ), list, list_size, counter, group );

      addEntry( "Processor Information", "Manufacturer", dmi_string( hdr, hdr->data[0x07] ), list, list_size, counter, group );
      sprintf( wrkValue, "0x%08llx", QWORD( hdr->data + 0x08 ) );
      addEntry( "Processor Information", "ID", wrkValue, list, list_size, counter, group );
      addEntry( "Processor Information", "Version", dmi_string( hdr, hdr->data[0x10] ), list, list_size, counter, group );
      addEntry( "Processor Information", "Status", processor_status[ hdr->data[0x18] & 0x07] , list, list_size, counter, group );

      if( hdr->length < 0x23 )
        break;
      addEntry( "Processor Information", "Serial Number", dmi_string( hdr, hdr->data[0x20] ), list, list_size, counter, group );
      addEntry( "Processor Information", "Asset Tag", dmi_string( hdr, hdr->data[0x21] ), list, list_size, counter, group );
      addEntry( "Processor Information", "Part Number", dmi_string( hdr, hdr->data[0x22] ), list, list_size, counter, group );

      if( hdr->length < 0x28 )
        break;
      sprintf( wrkValue, "%u", hdr->data[0x23] );
      addEntry( "Processor Information", "Core Count", wrkValue, list, list_size, counter, group );
      sprintf( wrkValue, "%u", hdr->data[0x24] );
      addEntry( "Processor Information", "Core Enabled", wrkValue, list, list_size, counter, group );
      sprintf( wrkValue, "%u", hdr->data[0x25] );
      addEntry( "Processor Information", "Thread Count", wrkValue, list, list_size, counter, group );

      break;

    case 5: // section 7.6
      //printf( "Memory Controller Information:\n" );
      break;  // Nothing here we are interested in right now

    case 6: // section 7.7
      if( hdr->length < 0x0C )
        break;
      addEntry( "Memory Module Information", "Socket Designation", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      sprintf( wrkValue, "%u ns", hdr->data[0x06] );
      addEntry( "Memory Module Information", "Current Speed", wrkValue, list, list_size, counter, group );
      dmi_memory_module_type( wrkValue, WORD( hdr->data + 0x07 ) );
      addEntry( "Memory Module Information", "Type", wrkValue, list, list_size, counter, group );
      dmi_memory_module_size( wrkValue, hdr->data[0x09] );
      addEntry( "Memory Module Information", "Installed Size", wrkValue, list, list_size, counter, group );
      dmi_memory_module_size( wrkValue, hdr->data[0x0A] );
      addEntry( "Memory Module Information", "Enabled Size", wrkValue, list, list_size, counter, group );
      dmi_memory_module_error( wrkValue, hdr->data[0x0B] );
      addEntry( "Memory Module Information", "Error Status", wrkValue, list, list_size, counter, group );

      break;

    case 7: // section 7.8
      //printf( "Cache Information:\n" );
      break;  // Nothing here we are interested in right now
    case 8: // section 7.9
      //printf( "Port Connector Information:\n" );
      break;  // Nothing here we are interested in right now
    case 9: // section 7.10
      //printf( "System Port Information:\n" );
      break;  // Nothing here we are interested in right now
    case 10: //section 7.11
      //printf( "On Board Devices Information:\n" );
      break;  // Nothing here we are interested in right now

    case 11: // section 7.12
      if( hdr->length < 0x05 )
        break;

      for( i = 1; i <= hdr->data[0x04]; i++ )
      {
        sprintf( wrkValue, "String %d", i );
        addEntry( "OEM Strings", wrkValue, dmi_string( hdr, i ), list, list_size, counter, group );
      }

      break;

    case 12: // section 7.13
      if( hdr->length < 0x05 )
        break;

      for( i = 1; i <= hdr->data[0x04]; i++ )
      {
        sprintf( wrkValue, "Option %d", i );
        addEntry( "System Configuration Options", wrkValue, dmi_string( hdr, i ), list, list_size, counter, group );
      }

      break;

    case 14: // section 7.15
      //printf( "Group Associations:\n" );
      break;  // Nothing here we are interested in right now

    case 15: // section 7.16
      if( hdr->length < 0x14 )
        break;

      dmi_event_log_status( wrkValue, hdr->data[0x0B] );
      addEntry( "System Event Log", "Status", wrkValue, list, list_size, counter, group );

      if( hdr->length < 0x17 )
        break;

      if( hdr->length < ( 0x17 + hdr->data[0x15] * hdr->data[0x16] ) )
        break;

      for( i = 0; i < hdr->data[0x15]; i++ )
      {
        if( hdr->data[0x16] >= 0x02 )
        {
          sprintf( wrkValue, "Descriptor %u", i + 1 );
          addEntry( "System Event Log", wrkValue, dmi_event_log_descriptor_type( hdr->data[ 0x17 + i * hdr->data[0x16] ] ), list, list_size, counter, group );
          sprintf( wrkValue, "Format %u", i + 1 );
          addEntry( "System Event Log", wrkValue, dmi_event_log_descriptor_format( hdr->data[ 0x17 + i * hdr->data[0x16] + 1 ] ), list, list_size, counter, group );
        }
      }

      break;

    case 16: // section 7.17
      //printf( "Physical Memory Array:\n" );
      break;  // Nothing here we are interested in right now

    case 17: // section 7.18
      if( hdr->length < 0x15 )
        break;

      if( hdr->length >= 0x20 && ( WORD( hdr->data + 0x0C ) == 0x7FFF ) )
        dmi_memory_device_extended_size( wrkValue, DWORD( hdr->data + 0x1C ) );
      else
        dmi_memory_device_size( wrkValue, WORD( hdr->data + 0x0C ) );
      addEntry( "Memory Device", "Size", wrkValue, list, list_size, counter, group );

      dmi_memory_device_set( wrkValue, hdr->data[0x0F] );
      addEntry( "Memory Device", "Set", wrkValue, list, list_size, counter, group );

      addEntry( "Memory Device", "Locator", dmi_string( hdr, hdr->data[0x10] ), list, list_size, counter, group );
      addEntry( "Memory Device", "Bank Locator", dmi_string( hdr, hdr->data[0x11] ), list, list_size, counter, group );
      addEntry( "Memory Device", "Type", dmi_memory_device_type( hdr->data[0x12] ), list, list_size, counter, group );
      dmi_memory_device_speed( wrkValue, WORD( hdr->data + 0x15 ) );
      addEntry( "Memory Device", "Speed", wrkValue, list, list_size, counter, group );

      if( hdr->length < 0x1B )
        break;
      addEntry( "Memory Device", "Manufacturer", dmi_string( hdr, hdr->data[0x17] ), list, list_size, counter, group );
      addEntry( "Memory Device", "Serial Number", dmi_string( hdr, hdr->data[0x18] ), list, list_size, counter, group );
      addEntry( "Memory Device", "Asset Tag", dmi_string( hdr, hdr->data[0x19] ), list, list_size, counter, group );
      addEntry( "Memory Device", "Part Number", dmi_string( hdr, hdr->data[0x1A] ), list, list_size, counter, group );

      break;

    case 18: // section 7.19
      if( hdr->length < 0x17 )
        break;
      addEntry( "32-bit Memory Error Information", "Type", dmi_memory_error_type( hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "32-bit Memory Error Information", "Granularity", dmi_memory_error_granularity( hdr->data[0x05] ), list, list_size, counter, group );
      addEntry( "32-bit Memory Error Information", "Operation", dmi_memory_error_operation( hdr->data[0x06] ), list, list_size, counter, group );
      dmi_memory_error_syndrome( wrkValue, DWORD( hdr->data + 0x07 ) );
      addEntry( "32-bit Memory Error Information", "Vendor Syndrome:", wrkValue, list, list_size, counter, group );
      dmi_32bit_memory_error_address( wrkValue, DWORD( hdr->data + 0x0B ) );
      addEntry( "32-bit Memory Error Information", "Memory Array Address:", wrkValue, list, list_size, counter, group );
      dmi_32bit_memory_error_address( wrkValue, DWORD( hdr->data + 0x0F ) );
      addEntry( "32-bit Memory Error Information", "Device Address:", wrkValue, list, list_size, counter, group );
      dmi_32bit_memory_error_address( wrkValue, DWORD( hdr->data + 0x13 ) );
      addEntry( "32-bit Memory Error Information", "Resolution:", wrkValue, list, list_size, counter, group );
      break;

    case 19: // section 7.20
      //printf( "Memory Array Mapped Address:\n" );
      break;  // Nothing here we are interested in right now
    case 20: // section 7.21
      //printf( "Memory Device Mapped Address:\n" );
      break;  // Nothing here we are interested in right now
    case 21: // section 7.22
      //printf( "Build-in Pointing Device:\n" );
      break;  // Nothing here we are interested in right now
    case 22: // section 7.23
      //printf( "Portable Battery:\n" );
      break;  // Nothing here we are interested in right now
    case 23: // section 7.24
      //printf( "System Reset:\n" );
      break;  // Nothing here we are interested in right now
    case 24: // section 7.25
      //printf( "Hardware Security:\n" );
      break;  // Nothing here we are interested in right now
    case 25: // section 7.26
      //printf( "System Power Controls:\n" );
      break;  // Nothing here we are interested in right now

    case 26: // section 7.27
      if( hdr->length < 0x14 )
        break;
      addEntry( "Voltage Probe", "Description", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "Voltage Probe", "Location", dmi_voltage_probe_location( hdr->data[0x05] & 0x1f ), list, list_size, counter, group );
      addEntry( "Voltage Probe", "Status", dmi_probe_status( hdr->data[0x05] >> 5 ), list, list_size, counter, group );
      dmi_voltage_probe_value( wrkValue, WORD( hdr->data + 0x06 ) );
      addEntry( "Voltage Probe", "Maximum Value", wrkValue, list, list_size, counter, group );
      dmi_voltage_probe_value( wrkValue, WORD( hdr->data + 0x08 ) );
      addEntry( "Voltage Probe", "Minimum Value", wrkValue, list, list_size, counter, group );
      dmi_voltage_probe_resolution( wrkValue, WORD( hdr->data + 0x0A ) );
      addEntry( "Voltage Probe", "Resolution", wrkValue, list, list_size, counter, group );
      dmi_voltage_probe_value( wrkValue, WORD( hdr->data + 0x0C ) );
      addEntry( "Voltage Probe", "Tolerance", wrkValue, list, list_size, counter, group );
      dmi_probe_accuracy( wrkValue, WORD( hdr->data + 0x0E ) );
      addEntry( "Voltage Probe", "Accuracy", wrkValue, list, list_size, counter, group );

      if( hdr->length < 0x16 )
        break;
      dmi_voltage_probe_value( wrkValue, WORD( hdr->data + 0x14 ) );
      addEntry( "Voltage Probe", "Nominal Value", wrkValue, list, list_size, counter, group );

      break;

    case 27: // section 7.28
      if( hdr->length < 0x0C )
        break;
      addEntry( "Cooling Device", "Type", dmi_cooling_device_type( hdr->data[0x06] & 0x1f ), list, list_size, counter, group );
      addEntry( "Cooling Device", "Status", dmi_probe_status( hdr->data[0x06] >> 5 ), list, list_size, counter, group );
      if( hdr->data[0x07] != 0x00 )
      {
        sprintf( wrkValue, "%u", hdr->data[0x07] );
        addEntry( "Cooling Device", "Cooling Unit Group", wrkValue, list, list_size, counter, group );
      }

      if( hdr->length < 0x0E )
        break;
      dmi_cooling_device_speed( wrkValue, WORD( hdr->data + 0x0C ) );
      addEntry( "Cooling Device", "Nominal Speed", wrkValue, list, list_size, counter, group );

      if( hdr->length < 0x0F )
        break;
      addEntry( "Cooling Device", "Description", dmi_string( hdr, hdr->data[0x0E] ), list, list_size, counter, group );

      break;

    case 28: // section 7.29
      if( hdr->length < 0x14 )
        break;
      addEntry( "Temperature Probe", "Description", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "Temperature Probe", "Location", dmi_temperature_probe_location( hdr->data[0x05] & 0x1F ), list, list_size, counter, group );
      addEntry( "Temperature Probe", "Status", dmi_probe_status( hdr->data[0x05] >> 5 ), list, list_size, counter, group );
      dmi_temperature_probe_value( wrkValue, WORD( hdr->data + 0x06 ) );
      addEntry( "Temperature Probe", "Maximum Value", wrkValue, list, list_size, counter, group );
      dmi_temperature_probe_value( wrkValue, WORD( hdr->data + 0x08 ) );
      addEntry( "Temperature Probe", "Minimum Value", wrkValue, list, list_size, counter, group );
      dmi_temperature_probe_resolution( wrkValue, WORD( hdr->data + 0x0A ) );
      addEntry( "Temperature Probe", "Resolution", wrkValue, list, list_size, counter, group );
      dmi_temperature_probe_value( wrkValue, WORD( hdr->data + 0x0C ) );
      addEntry( "Temperature Probe", "Tolerance", wrkValue, list, list_size, counter, group );
      dmi_probe_accuracy( wrkValue, WORD( hdr->data + 0x0E ) );
      addEntry( "Temperature Probe", "Accuracy", wrkValue, list, list_size, counter, group );

      if( hdr->length < 0x16 )
        break;
      dmi_temperature_probe_value( wrkValue, WORD( hdr->data + 0x14 ) );
      addEntry( "Temperature Probe", "Nominal Value", wrkValue, list, list_size, counter, group );
      break;

    case 29: // section 7.30
      if( hdr->length < 0x14 )
        break;
      addEntry( "Electrical Current Probe", "Description", dmi_string( hdr, hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "Electrical Current Probe", "Location", dmi_voltage_probe_location( hdr->data[5] & 0x1F ), list, list_size, counter, group );
      addEntry( "Electrical Current Probe", "Status", dmi_probe_status( hdr->data[0x05] >> 5 ), list, list_size, counter, group );
      dmi_current_probe_value( wrkValue, WORD( hdr->data + 0x06 ) );
      addEntry( "Electrical Current Probe", "Maximum Value", wrkValue, list, list_size, counter, group );
      dmi_current_probe_value( wrkValue, WORD( hdr->data + 0x08 ) );
      addEntry( "Electrical Current Probe", "Minimum Value", wrkValue, list, list_size, counter, group );
      dmi_current_probe_resolution( wrkValue, WORD( hdr->data + 0x0A ) );
      addEntry( "Electrical Current Probe", "Resolution", wrkValue, list, list_size, counter, group );
      dmi_current_probe_value( wrkValue, WORD( hdr->data + 0x0C ) );
      addEntry( "Electrical Current Probe", "Tolerance", wrkValue, list, list_size, counter, group );
      dmi_probe_accuracy( wrkValue, WORD( hdr->data + 0x0E ) );
      addEntry( "Electrical Current Probe", "Accuracy", wrkValue, list, list_size, counter, group );

      if( hdr->length < 0x16 )
        break;
      dmi_current_probe_value( wrkValue, WORD( hdr->data + 0x14 ) );
      addEntry( "Electrical Current Probe", "Nominal Value:", wrkValue, list, list_size, counter, group );
      break;

    case 30: // section 7.31
      //printf( "Out-of-band Remote Access:\n" );
      break;  // Nothing here we are interested in right now
    case 31: // section 7.32
      //printf( "Boot Integrity Services Entry Point:\n" );
      break;  // Nothing here we are interested in right now
    case 32: // section 7.33
      //printf( "System Boot Information:\n" );
      break;  // Nothing here we are interested in right now

    case 33: // section 7.34
      if( hdr->length < 0x1F )
        break;
      addEntry( "64-bit Memory Error Information", "Type", dmi_memory_error_type( hdr->data[0x04] ), list, list_size, counter, group );
      addEntry( "64-bit Memory Error Information", "Granularity", dmi_memory_error_granularity( hdr->data[0x05] ), list, list_size, counter, group );
      addEntry( "64-bit Memory Error Information", "Operation", dmi_memory_error_operation( hdr->data[0x06] ), list, list_size, counter, group );
      dmi_memory_error_syndrome( wrkValue, DWORD( hdr->data + 0x07 ) );
      addEntry( "64-bit Memory Error Information", "Vendor Syndrome", wrkValue, list, list_size, counter, group );
      dmi_64bit_memory_error_address( wrkValue, QWORD( hdr->data + 0x0B ) );
      addEntry( "64-bit Memory Error Information", "Memory Array Address", wrkValue, list, list_size, counter, group );
      dmi_64bit_memory_error_address( wrkValue, QWORD( hdr->data + 0x13 ) );
      addEntry( "64-bit Memory Error Information", "Device Address", wrkValue, list, list_size, counter, group );
      dmi_32bit_memory_error_address( wrkValue, DWORD( hdr->data + 0x1B ) );
      addEntry( "64-bit Memory Error Information", "Resolution", wrkValue, list, list_size, counter, group );
      break;

    case 34: // section 7.35
     //printf( "Managment Device:\n" );
      break;  // Nothing here we are interested in right now
    case 35: // section 7.36
      //printf( "Managment Device Component:\n" );
      break;  // Nothing here we are interested in right now
    case 36: // section 7.37
      //printf( "Managment Device Threshold Data:\n" );
      break;  // Nothing here we are interested in right now
    case 37: // section 7.38
      //printf( "Memory Channel:\n" );
      break;  // Nothing here we are interested in right now
    case 38: // section 7.39
      //printf( "IPMI Device Information:\n" );
      break;  // Nothing here we are interested in right now

    case 39: // section 7.40
      if( hdr->length < 0x10 )
        break;
      sprintf( wrkValue, "%u\n", hdr->data[0x04] );
      addEntry( "System Power Supply", "Power Unit Group", wrkValue, list, list_size, counter, group );
      addEntry( "System Power Supply", "Location", dmi_string( hdr, hdr->data[0x05] ), list, list_size, counter, group );

      if( !( WORD( hdr->data + 0x0E ) & ( 1 << 1 ) ) ) // not present
        break;

      addEntry( "System Power Supply", "Name", dmi_string( hdr, hdr->data[0x06] ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Manufacturer", dmi_string( hdr, hdr->data[0x07] ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Serial Number", dmi_string( hdr, hdr->data[0x08] ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Asset Tag", dmi_string( hdr, hdr->data[0x09] ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Model Part Number", dmi_string( hdr, hdr->data[0x0A] ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Revision", dmi_string( hdr, hdr->data[0x0B] ), list, list_size, counter, group );
      dmi_power_supply_power( wrkValue, WORD( hdr->data + 0x0C ) );
      addEntry( "System Power Supply", "Max Power Capacity", wrkValue, list, list_size, counter, group );
      addEntry( "System Power Supply", "Status", dmi_power_supply_status( ( WORD( hdr->data + 0x0E ) >> 7 ) & 0x07 ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Type", dmi_power_supply_type( ( WORD( hdr->data + 0x0E ) >> 10 ) & 0x0F ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Input Voltage Range Switching", dmi_power_supply_range_switching( ( WORD( hdr->data + 0x0E ) >> 3 ) & 0x0F ), list, list_size, counter, group );
      addEntry( "System Power Supply", "Plugged", WORD( hdr->data + 0x0E ) & ( 1 << 2 ) ? "No" : "Yes", list, list_size, counter, group );
      addEntry( "System Power Supply", "Hot Replaceable", WORD( hdr->data + 0x0E ) & ( 1 << 0 ) ? "Yes" : "No", list, list_size, counter, group );

      break;

    case 40: // section 7.41
      //printf( "Additional Information:\n" );
      break;  // Nothing here we are interested in right now
    case 41: // section 7.42
      //printf( "Onboard Device Extend Information:\n" );
      break;  // Nothing here we are interested in right now
    case 42: // section 7.43
      //printf( "Management Controller Host Interface:\n" );
      break;  // Nothing here we are interested in right now
    case 126: // section 7.44
      if( verbose >= 3 )
        fprintf( stderr, "Inactive\n" );
      break;
    case 127: // section 7.45
      if( verbose >= 3 )
        fprintf( stderr, "End of Table\n" );
      break;

    default:
      if( verbose >= 3 )
      {
        if( hdr->type >= 128 )
          fprintf( stderr, "Uknown OEM Type: %u\n", hdr->type );
        else
          fprintf( stderr, "Uknown Type: %u\n", hdr->type );
      }
  }
}

int readBlock( __u8 *buff, const size_t offset, const size_t length )
{
  int fd;
  ssize_t rc;

  fd = open( DEV_MEM_FILE, O_RDONLY | O_NONBLOCK );
  if( fd == -1 )
  {
    if( verbose )
      fprintf( stderr, "Error opening device '" DEV_MEM_FILE "', errno: %i\n", errno );
    return -1;
  }

  if( lseek( fd, offset, SEEK_SET ) == -1 )
  {
    if( verbose )
      fprintf( stderr, "Error with lseek, errno: %i\n", errno );

    close( fd );
    return -1;
  }

  rc = read( fd, buff, length );
  close( fd );

  if( rc == -1 )
  {
    if( verbose )
      fprintf( stderr, "Error with read, errno: %i\n", errno );

    return -1;
  }

  if( rc != (ssize_t) length )
  {
    if( verbose )
      fprintf( stderr, "Read stoped short, got %zi of %zi\n", rc, length );
    errno = EMSGSIZE;
    return -1;
  }

  return 0;
}

int dmi_table( const size_t base, const size_t length, const int count, const int version, struct dmi_entry list[], const int list_size, int *entry_counter, int *group_counter )
{
  __u8 *buff;
  __u8 *pos;
  struct dmi_header hdr;
  int i;
  int tmpLen;

  if( verbose >= 3 )
  {
    fprintf( stderr, "%i structures occupying %zi bytes starting at 0x%08zx.\n", count, length, base );
  }

  buff = malloc( length );
  if( readBlock( buff, base, length ) )
  {
    free( buff );
    return -1;
  }

  pos = buff;
  errno = 0;

  for( i = 0 ; ( i <= count ) && ( ( size_t )( pos - buff ) < length ) ; i++ )
  {
    to_dmi_header( &hdr, pos );

    if( hdr.length < 4 )
    {
      if( verbose )
        fprintf( stderr, "Invalid entry length: %i.\n  Table Broken Stopping.", hdr.length );
      errno = EBADE;
      break;
    }

    if( verbose >= 2 )
      fprintf( stderr, "%i: Handle: 0x%04x, Type: %i, Header Length: %i, Pos: %p\n", i, hdr.handle, hdr.type, hdr.length, pos );

    if( verbose >= 4 )
      dump_bytes( pos, hdr.length );

    dmi_decode( &hdr, version, list, list_size, entry_counter, *group_counter );

    tmpLen = 0;
    pos += hdr.length;
    while( ( ( pos[0] != 0 ) || ( pos[1] != 0 ) ) && ( ( size_t )( pos - buff ) < length ) )
    {
      pos++;
      tmpLen++;
    }

    pos += 2;

    if( verbose >= 4 )
      dump_bytes( pos - tmpLen - 2, tmpLen + 2 );

    (*group_counter)++;
  }

  if( i != count )
  {
    if( verbose )
      fprintf( stderr, "Expecting %i entries, got %i", count, i );
    errno = EBADE;
  }

  free( buff );

  if( errno )
    return -1;
  else
    return 0;
}
