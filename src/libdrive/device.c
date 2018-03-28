#include "device.h"

int cmdTimeout = 10;  // seconds

int setTimeout( int timeout )
{
  int oldTimeout = cmdTimeout;
  cmdTimeout = timeout;
  return oldTimeout;
}
