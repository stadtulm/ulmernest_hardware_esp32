#ifndef GENERAL_ERROR_H
#define GENERAL_ERROR_H

enum class general_error : unsigned char
{
  BAD_CRC = 0xFD, // CRC of received command is invalid
  BAD_LENGTH = 0xFE, // Length of retrieved command payload does not match expected length
  UNKNOWN = 0xFF // Used if no other error code matches
};

#endif