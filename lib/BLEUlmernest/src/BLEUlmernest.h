#ifndef BLEULMERNEST_H
#define BLEULMERNEST_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEAdvertisedDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <endian.h>
#include <esp_task_wdt.h>
#include "Bote.h"
#include "Schluesselbund.h"
#include "enums/lock_actions.h"
#include "enums/pairing_states.h"
#include "enums/keyturner_states/nuki_states.h"
#include "enums/transmission.h"
#include "states/KeyturnerStates.h"
#include "Helper.h"
#include "Debug.h"

#ifndef debug
#define debug 1
#endif

#define uuid_usdio "a92ee202-5501-11e4-916c-0800200c9a66"

#define SCAN_TIME_SEC 5

#ifndef SCAN_MAX_TRYS
// SCAN_MAX_TRYS: 1 to 255; 0 unlimited
#define SCAN_MAX_TRYS 1
#endif

class BLEUlmernest : public BLEDevice
{
private:
  static BLEUUID uuid_pairing_service;
  static BLEUUID uuid_pairing_characteristic;
  static BLEUUID uuid_service;
  static BLEUUID uuid_user_specific_dio_characteristic;

  static Bote* pBote;
  static Schluesselbund* pBund;
  static std::vector<BLEAdvertisedDevice> matched_devices;
  static BLEAdvertisedDevice device;
  static std::vector<BLEAdvertisedDevice> scan_results;
  static BLEAdvertisedDevice* pNuki;
  static BLEScan* pBLEScan;
  static BLEClient* pClient;
  static BLERemoteService* pRemoteService;
  static BLERemoteCharacteristic* pRemoteCharacteristic;
  static BLERemoteService* pUserService;
  static BLERemoteCharacteristic* pUSDIO;
  static char stored_address[18];
  static int current_state;

  static KeyturnerStates keyturner_states;


  /*******************
   * Private Methods
   *******************/

  // Called when BLE device connects or disconnects
  class ClientCallbacks_Connect : public BLEClientCallbacks
  {
    void onConnect(BLEClient* pclient)
    {
      if (debug) Serial.println("-> onConnect");
    }
    void onDisconnect(BLEClient* pclient)
    {
      if (debug) Serial.println("-> onDisconnect");
    }
  };

  /**
   * Scan for BLE devices.
   * @return All advertised BLE Devices as vector<BLEAdvertisedDevice>
   */
  static std::vector<BLEAdvertisedDevice> scan ();

  /**
   * Establish initial connection with a desired BLE Device
   *
   * @return A pointer to the connected BLE client.
   */
  static BLEClient* initial_connect ();

  /**
   * Try to pair with a BLE device.
   * Requires the target device to be in pairing mode.
   * Also requires the results from a scan().
   *
   * @return true - pairing successful; false - pariring failed, see error message for more information.
   */
  static bool pair ();


public:
  /***************
   * Constructor
   ***************/
  BLEUlmernest();


  /*******************
   * Getter & Setter
   *******************/
  static Bote* get_Bote();
  static Schluesselbund* get_Bund();
  static int get_current_state();
  static void set_current_state(int pairing_state);
  BLERemoteCharacteristic* get_RemoteCharacteristic ();
  static KeyturnerStates get_keytuerner_states ();


  /******************
   * Public Methods
   ******************/

  /**
   * Method to initialize all reuired elements to operate the BLE functionality.
   *
   * @param device_name Name to identify the created BLE client.
   *
   * @return  true:   The initialization finished successfully.
   *          fasle:  BLE client could not be established.
   */
  static bool init (std::__cxx11::string device_name);

  /**
   * Connect to user specific funtionality of a BLE device.
   * Requires a successful pairing.
   *
   * @return -1 - an error occured; otherwise 0 on connection
   */
  static int connect_user_specific ();

  /**
   * Write to a BLE characteristic.
   *
   * @param data Bytes to be written.
   * @param len Number of Bytes to be written.
   * @param response Defaults to false. Set to true if a response is required.
   */
  static void write (uint8_t* data, size_t len, bool response);

  /**
   * Requests keyturner state from Nuki SL.
   */
  static int read_keyturner_state ();

  /**
   * Requests Nuki SL to do a specific lock action.
   *
   * @param lock_action Different possible actions specified in enums/lock_actions.h
   *
   * @return Either returns a Nuki SL lock state as a positive value or -1 if the connection failed.
   */
  static int lock_action (uint8_t action);

  /**
   * Request log entries from Nuki SL
   * Use to find any interactions with the Nuki SL not sent via this BLE client.
   * For details of usage check Nuki SL BLE API documentation.
   *
   * @param start_index specific index of the log to begin the request with. 0 will start at the very begining of either direction.
   * @param count Number of logs to request.
   * @param out_logs_available pointer of a variable to store the number of logs available.
   * @param order Order of requested logs. Defautls to 0x01 which will result in the order begining from the most recent log. 0x00 will return the oldest log entry first.
   */
  static std::vector<std::vector<uint8_t>> req_log_entries (uint32_t start_index, uint16_t count, uint16_t &out_logs_available, uint8_t order = 0x01);
};

#endif // BLEULMERNEST_H