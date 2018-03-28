#ifndef _DMI_H
#define _DMI_H

#define DMI_ENTRY_TYPE_LEN 50
#define DMI_ENTRY_NAME_LEN 50
#define DMI_ENTRY_VALUE_LEN 150

struct dmi_entry
{
  int group;
  char type[DMI_ENTRY_TYPE_LEN];
  char name[DMI_ENTRY_NAME_LEN];
  char value[DMI_ENTRY_VALUE_LEN];
};

// returns -1 for error other wise the number of enteries avaiable, note this may be bigger than count
// list_size -> number of entries allocated to list
int getDMIList( struct dmi_entry list[], const int list_size );

#endif
