#ifndef SERIALCOMMHELPER_H
#define SERIALCOMMHELPER_H

#include <Arduino.h>
#include <map>
#include <esp_task_wdt.h>
#include "DataStructure.h"
#include "Helper.h"

// Number of bytes for a command code
#define CMD_BYTES 1
// Number of data bytes read at a time
#define RX_DATA_BYTES 1


class SerialComm_Helper
{
public:
  /***************
   * Constructor
   ***************/

  SerialComm_Helper () : s(&Serial) {};
  SerialComm_Helper (HardwareSerial&);


  /*******************
   * Getter + Setter
   *******************/

  unsigned char* get_lora_msg ();
  const size_t get_lora_msg_size ();


  /******************
   * Public Methods
   ******************/

  /**
   * Execute rx() and tx() in main.cpp loop()
   */
  void loop ();

  /**
   * Request data from Raspberry Pi
   * @param parameter_code Code for the value to be requested.
   */
  void request_data (unsigned char);

  /**
   * Send update to Rapberry Pi for the door state
   * @param door_state The door state to send to Raspberry Pi
   */
  void update_door (unsigned char);

  /**
   * Send update to Rapberry Pi for the lock state
   * @param lock_state The lock state to send to Raspberry Pi
   */
  void update_lock (unsigned char);

  /**
   * Clear the contents of lora_msg
   */
  void lora_msg_clear ();

  /**
   * Send Raspberry Pi the command to prepare for shutdown
   */
  void tx_sleep_raspberry ();


  /**********************************
   * Externally implemented Methods
   **********************************/

  /**
   * Access to data storage
   */

  void set_data (unsigned char, unsigned char*);
  const unsigned char* get_data (unsigned char);
  void set_state (unsigned char);
  const unsigned char get_state ();

  /**
   * Make the functionality to update the lock available for SerialComm_Helper
   */
  void update_lock();

  /**
   * Implement the functionality to unlock with a serial command
   */
  void unlock_on_serial_cmd ();

  /**
   * Implement the functionality to lock with a serial command
   */
  void lock_on_serial_cmd ();

  /**
   * Implement the functionality to toggle the execution of VeDirectFrameHandler with a serial command
   */
  void ve_exec_toggle_serial_cmd (uint8_t);

  /**
   * Implement the functionality to wipe non-volatile memory with a serial command
   */
  void wipe_storage_on_serial_cmd ();

private:
  HardwareSerial* s;
  unsigned char cmd_buffer, data_bytes_buffer;
  unsigned char data_buffer[200];
  std::vector<unsigned char> tx_queue, queue_req_params, queue_res_params, await_res_params, lora_msg;


  /******************
   * Public Methods
   ******************/

  /**
   * Handle Serial RX
   */

  void rx ();
  void rx_request_data ();
  void rx_response_data ();
  void rx_response_state ();
  void rx_update_data ();
  void rx_unlock ();
  void rx_lock ();
  void rx_lora_msg ();
  void rx_esp_restart ();
  void rx_ve_exec_state ();
  void rx_wipe_storage ();

  /**
   * Hanlde Serial TX
   */

  void tx ();
  void tx_request_data (unsigned char);
  void tx_response_data ();
  void tx_request_state ();
  void tx_update_data (unsigned char);
  void tx_update_state (unsigned char new_state);

  /**
   * Queue elments for a TX
   */

  void tx_add_to_queue (const unsigned char);
  void tx_add_to_queue (const unsigned char*, const char);
  char tx_queue_response (const unsigned char);
};

#endif