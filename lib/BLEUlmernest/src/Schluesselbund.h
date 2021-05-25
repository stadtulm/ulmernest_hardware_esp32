/**
 * Generate, store and use credentials
 */

#ifndef SCHLUESSELBUND_H
#define SCHLUESSELBUND_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include <sodium/crypto_box.h>
#include <sodium/crypto_scalarmult_curve25519.h>
#include <sodium/crypto_core_hsalsa20.h>
#include <sodium/crypto_auth_hmacsha256.h>
#include <sodium/crypto_secretbox.h>
#include <sodium/utils.h>
#include "Helper.h"
#include "Debug.h"

const size_t KEY_COUNT = 3;
const size_t KEY_LENGTH = 32;

class Schluesselbund
{
private:
  Preferences esp_storage;
  uint8_t device_name[32];
  uint32_t auth_id;
  uint8_t device_bound_uuid[16];

  uint8_t public_key[KEY_LENGTH];
  uint8_t secret_key[KEY_LENGTH];
  uint8_t sl_public_key[KEY_LENGTH];
  uint8_t* keys[KEY_COUNT] = { public_key, secret_key, sl_public_key };
  unsigned char dh_key[KEY_LENGTH];
  unsigned char shared_secret[KEY_LENGTH];


  /*******************
   * Private Methods
   *******************/

  /**
   * Save key in non-volatile memory
   * @param name Name of the key
   * @param key Key bytes
   * @param len Number of key bytes
   * @return TRUE if the key was stored successfully; FALSE if something went wrong
   */
  bool store_key (const char* name, uint8_t* key, size_t len);

public:
  /***************
   * Constructor
   ***************/

  Schluesselbund ();


  /******************
   * Getter, Setter
   ******************/

  const uint8_t* get_device_name ();
  const uint32_t get_auth_id ();
  const uint8_t* get_auth_id_le ();
  void set_auth_id (uint32_t);
  const uint8_t* get_device_bound_uuid ();        // not in use
  void set_device_bound_uuid (const uint8_t*);    // not in use
  const uint8_t* get_public_key ();
  const uint8_t* get_sl_public_key();
  void set_sl_public_key(uint8_t*, size_t);

  /**
   * Get the address of a paired Nuki SL from non-volatile memory
   */
  size_t get_address (char* stored_address);

  /**
   * Put the address of a paired Nuki SL in non-volatile memory
   */
  void store_address (char* address_to_remember);


  /******************
   * Public Methods
   ******************/

  /**
   * Initialize Schluesselbund
   *
   * @param name Device name to compare to reference in stored data
   */
  void init(std::__cxx11::string name);

  /**
   * Generate a new public key and secret key
   */
  void generate_keypair ();

  /**
   * Calculation for Diffie–Hellman key exchange
   */
  void calc_dh_key ();

  /**
   * Calculate shared secret
   */
  void calc_shared_secret ();

  /**
   * Calculate authorization authentication
   */
  void calc_auth (const unsigned char* challenge, size_t len, unsigned char* hash);

  /**
   * Load keys from non-volatile memory
   */
  void grab_keys ();

  /**
   * Store this devices public key, secret key, and Nuki SL public key from RAM in non-volatile memory
   */
  void store_keys ();

  /**
   * Remove credentials from non-volatile memory
   */
  void clear_credentials ();

  /**
   * Create cipher for a message
   * @param cipher Pointer to memory for cipher output
   * @param botschaft Message to be encrypted
   * @param length Number of message bytes
   * @param nonce_out Pointer to memory for nonce output
   */
  void seal (unsigned char* cipher, const unsigned char* botschaft, size_t length, unsigned char* nonce_out);

  /**
   * Get the message from a cipher
   * @param botschaft Pointer to memory for message output
   * @param cipher Cipher to be decrypted
   * @param length Number of cipher bytes
   * @param nonce Nonce in
   */
  void open (unsigned char* botschaft, const unsigned char* cipher, size_t length, unsigned char* nonce);

  /**
   * Wipe memory
   * @param buffer Memory to be wiped
   * @param len Number of bytes to wipe
   */
  void wipe(void*, size_t);

  /**
   * Wipe non-volatile memory
   */
  void wipe_storage();
};

#endif