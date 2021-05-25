#include "Helper.h"

void print_hex (const uint8_t* buffer, size_t size)
{
  for(size_t i = 0; i < size; ++i)
  {
    if (debug) Serial.print(buffer[i] < 16 ? "0" : "");
    if (debug) Serial.print(buffer[i], HEX);
    if (debug) Serial.print(" ");
  }
  if (debug) Serial.println();
}