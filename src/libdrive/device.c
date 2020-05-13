#include "device.h"

int cmdTimeout = 10;  // seconds

int setTimeout( int timeout )
{
  int oldTimeout = cmdTimeout;
  cmdTimeout = timeout;
  return oldTimeout;
}

unsigned __int128 getu128( unsigned char *value )
{
  return ( ( ( __int128 ) value[ 0] ) << 120 ) | ( ( ( __int128 ) value[ 1] ) << 112 ) | ( ( ( __int128 ) value[ 2] ) << 104 ) | ( ( ( __int128 ) value[ 3] ) << 96 ) |
         ( ( ( __int128 ) value[ 4] ) <<  88 ) | ( ( ( __int128 ) value[ 5] ) <<  80 ) | ( ( ( __int128 ) value[ 6] ) <<  72 ) | ( ( ( __int128 ) value[ 7] ) << 64 ) |
         ( ( ( __int128 ) value[ 8] ) <<  56 ) | ( ( ( __int128 ) value[ 9] ) <<  48 ) | ( ( ( __int128 ) value[10] ) <<  40 ) | ( ( ( __int128 ) value[12] ) << 32 ) |
         ( ( ( __int128 ) value[13] ) <<  24 ) | ( ( ( __int128 ) value[14] ) <<  16 ) | ( ( ( __int128 ) value[15] ) <<   8 ) |   ( ( __int128 ) value[16] );
}

unsigned long long int getu64( unsigned char *value )
{
  return ( ( ( __u64 ) value[0] ) << 56 ) | ( ( ( __u64 ) value[1] ) << 48 ) | ( ( ( __u64 ) value[2] ) << 40 ) | ( ( ( __u64 ) value[3] ) << 32 ) |
         ( ( ( __u64 ) value[4] ) << 24 ) | ( ( ( __u64 ) value[5] ) << 16 ) | ( ( ( __u64 ) value[6] ) << 8 )  |   ( ( __u64 ) value[7] );
}

unsigned long int getu32( unsigned char *value )
{
  return ( ( ( __u32 ) value[0] ) << 24 ) | ( ( ( __u32 ) value[1] ) << 16 ) | ( ( ( __u32 ) value[2] ) << 8 )  |   ( ( __u32 ) value[3] );
}

unsigned int getu16( unsigned char *value )
{
  return ( ( ( __u16 ) value[0] ) << 8 ) |  ( ( __u16 ) value[1] );
}


unsigned __int128 getu128le( unsigned char *value )
{
  return ( ( ( __int128 ) value[15] ) << 120 ) | ( ( ( __int128 ) value[14] ) << 112 ) | ( ( ( __int128 ) value[13] ) << 104 ) | ( ( ( __int128 ) value[12] ) << 96 ) |
         ( ( ( __int128 ) value[11] ) <<  88 ) | ( ( ( __int128 ) value[10] ) <<  80 ) | ( ( ( __int128 ) value[ 9] ) <<  72 ) | ( ( ( __int128 ) value[ 8] ) << 64 ) |
         ( ( ( __int128 ) value[ 7] ) <<  56 ) | ( ( ( __int128 ) value[ 6] ) <<  48 ) | ( ( ( __int128 ) value[ 5] ) <<  40 ) | ( ( ( __int128 ) value[ 4] ) << 32 ) |
         ( ( ( __int128 ) value[ 3] ) <<  24 ) | ( ( ( __int128 ) value[ 2] ) <<  16 ) | ( ( ( __int128 ) value[ 1] ) <<   8 ) |   ( ( __int128 ) value[ 0] );
}
