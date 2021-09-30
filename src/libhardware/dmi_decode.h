#ifndef _UTIL_H
#define _UTIL_H

#include <sys/types.h>
#include <linux/types.h>

#include "dmi.h"

#define DEV_MEM_FILE "/dev/mem"

#define WORD(x)  (__u16) ( *(const __u16 *) (x) )
#define DWORD(x) (__u32) ( *(const __u32 *) (x) )
#define QWORD(x)         ( *(const __u64 *) (x) )
#define dmiQWORD(x)      ( *(const dmiu64 *) (x) )

#define ARRAY_SIZE(x) ( sizeof(x) / sizeof( (x)[0] ) )

struct dmi_header
{
  __u8 type;
  __u8 length;
  __u16 handle;
  __u8 *data;
};

int readBlock( __u8 *buff, const size_t offset, const size_t length );

int dmi_table( const size_t base, const size_t length, const int count, const int version, struct dmi_entry list[], const int list_size, int *entry_counter, int *group_counter );


#endif
