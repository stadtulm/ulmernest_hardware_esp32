/**
 * Cyclic Redundancy Check
 * Nuki SL BLE communication uses CRC to validate data.
 */

#ifndef CRC_CCITT_H
#define CRC_CCITT_H

#include <Arduino.h>
#include <FastCRC.h>
#include "Helper.h"

/**
 * Calculate CRC value for a message to be sent.
 *
 * @param buffer Message data
 * @param buffer_len Number of Bytes in the message data
 */
uint16_t crc_ccitt (uint8_t*, size_t);

/**
 * Validate recieved data.
 *
 * @param buffer Recieved data to be validated
 * @param buffer_len Number of recieved Bytes
 */
bool crc_validate (uint8_t*, size_t);

#endif