#ifndef PAIRING_ERROR_H
#define PAIRING_ERROR_H

enum class pairing_error : unsigned char
{
  NOT_PAIRING = 0x10, // Returned if public key is being requested via request data command, but the Smart Lock is not in pairing mode
  BAD_AUTHENTICATOR = 0x11, // Returned if the received authenticator does not match the own calculated authenticator
  BAD_PARAMETER = 0x12, // Returned if a provided parameter is outside of its valid range
  MAX_USER = 0x13 // Returned if the maximum number of users has been reached
};

#endif