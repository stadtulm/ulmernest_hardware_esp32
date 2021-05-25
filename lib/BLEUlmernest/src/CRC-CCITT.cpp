#include "CRC-CCITT.h"

// #define DEBUG_CRC

FastCRC16 fastcrc16;

uint16_t crc_ccitt (uint8_t* buffer, size_t buffer_len)
{
  #ifdef DEBUG_CRC
  if (debug) Serial.println(" # Calculate CRC CCITT redundancy");
  #endif

  uint16_t crc = fastcrc16.ccitt(buffer, buffer_len);

  #ifdef DEBUG_CRC
  if (debug) Serial.print(" - Buffer <");
  print_hex(buffer, buffer_len);
  if (debug) Serial.print("> with length of ");
  if (debug) Serial.println(buffer_len);
  if (debug) Serial.print(" - Calculated redundancy: ");
  if (debug) Serial.println(crc, HEX);
  #endif

  return crc;
}

bool crc_validate (uint8_t* buffer, size_t buffer_len)
{
  uint16_t crc = (buffer[buffer_len - 1] << 8) | (buffer[buffer_len - 2] & 0xff);
  uint16_t validate = crc_ccitt(buffer, buffer_len - 2);

  #ifdef DEBUG_CRC
  print_hex(buffer, buffer_len);
  print_hex(buffer, buffer_len - 2);
  if (debug) Serial.print("crc: 0x");
  if (debug) Serial.print(crc, HEX);
  if (debug) Serial.print(", validation: 0x");
  if (debug) Serial.println(validate, HEX);
  #endif

  if (crc == validate)
  {
    #ifdef DEBUG_CRC
    if (debug) Serial.println("crc validation success");
    #endif
    return true;
  }
  else
  {
    if (debug) Serial.println("crc validation failure");
    return false;
  }
}