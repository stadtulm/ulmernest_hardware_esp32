#include <Arduino.h>
#include <BLERemoteCharacteristic.h>
#include "BLEUlmernest.h"
#include "CRC-CCITT.h"
#include "Schluesselbund.h"
#include "Helper.h"
#include "Debug.h"

#ifndef BOTE_H
#define BOTE_H

#include "enums/Cmd.h"
#include "enums/pairing_states.h"

class Bote
{
private:
  std::vector<uint8_t> botschaft;
  std::vector<uint8_t> antwort;

  /*******************
   * Object pointers
   *******************/

  static Bote* pBote;
  Schluesselbund* pBund;


  /*******************
   * Private Methods
   *******************/

  /**
   * Recieve data from a indication callback and make it available.
   *
   * @param pBote Pointer to Bote() Object
   * @param pData Pointer to data bytes
   * @param len Number fo data bytes
   * @param keepData Defaults to false. Set to true to keep previously recieved data stored in vector<uint8_t> botschaft
   */
  static void receive (Bote*, uint8_t*, size_t, bool);

  /**
   * Recieve encrypted data from a indication callback and make it available.
   *
   * @param pBote Pointer to Bote() Object
   * @param pData Pointer to data bytes
   * @param len Number fo data bytes
   * @param keepData Defaults to false. Set to true to keep previously recieved data stored in vector<uint8_t> botschaft
   */
  static void receive_crypto (Bote*, uint8_t*, size_t, bool);

public:
  /***************
   * Constructor
   ***************/

  Bote();
  Bote(Schluesselbund* pBund);
  Bote(Bote& bote);


  /******************
   * Getter, Setter
   ******************/

  void set_pBund (Schluesselbund);
  void set_pBund (Schluesselbund*);

  const std::vector<uint8_t> get_botschaft ();
  const std::vector<uint8_t> get_antwort ();
  void get_antwort (uint8_t* out);


  /******************
   * Public Methods
   ******************/

  /**
   * Append a command to the pending message.
   *
   * @param command A 16-Bit value of a command. Use enums/Cmd.h for easy access to commands.
   */
  Bote& command (uint16_t command);
  /**
   * Append a number of bytes to the pending message.
   *
   * @param data Data bytes
   * @param len Number of data bytes
   */
  Bote& data (uint8_t* data, size_t len);
  /**
   * Append a number of bytes to the pending message.
   *
   * @param data Data bytes (const)
   * @param len Number of data bytes
   */
  Bote& data (const uint8_t* data, size_t len);
  /**
   * Send the pending message plain to a BLE device's characteristic.
   *
   * @param pRemoteCharacteristic Pointer to the target BLE device's characteristic
   * @param and_reset Default false; Set true to reset the message after it was sent.
   */
  Bote& send (BLERemoteCharacteristic* pRemoteCharacteristic, bool and_reset);
  /**
   * Send the pending message encrypted to a BLE device's characteristic.
   *
   * @param pRemoteCharacteristic Pointer to the target BLE device's characteristic
   * @param pBund Pointer to an instance of Schluesselbund to use encryption
   */
  Bote& send_cipher (BLERemoteCharacteristic*, Schluesselbund*);
  /**
   * Reset the pending message.
   */
  Bote& reset ();
  void register_indicate ();

  /**
   * Notify Callbacks
   * A number of callbacks used when notifications or indications from a BLE device are incoming.
   */

  static void notifyCallback (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void notifyCallback_receive_pk (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void notifyCallback_challenge (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void notifyCallback_challenge_auth (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void notifyCallback_get_auth_id (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void notifyCallback_confirm_auth_id (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void notifyCallback_crypto (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void notifyCallback_req_challenge (BLERemoteCharacteristic*, uint8_t*, size_t, bool);
};

#endif // BOTE_H
