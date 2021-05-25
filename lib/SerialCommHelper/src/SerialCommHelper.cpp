#include "SerialCommHelper.h"

#include <CayenneLPP.h>

/***************
 * Constructor
 ***************/

SerialComm_Helper::SerialComm_Helper (HardwareSerial& serial)
{
  s = &serial;
  cmd_buffer = 0;
  data_bytes_buffer = 0;
}


/******************
 * Getter, Setter
 ******************/

unsigned char* SerialComm_Helper::get_lora_msg ()
{
  return lora_msg.data();
}

const size_t SerialComm_Helper::get_lora_msg_size ()
{
  return lora_msg.size();
}


/******************
 * Public Methods
 ******************/

void SerialComm_Helper::loop ()
{
  if (s)
  {
    rx();
    tx();
  }
  else
  {
    if (debug) Serial.println("SerialComm_Helper::loop: Serial not running!");
  }
}

/**
 * Request data for a parameter from Raspberry Pi
 * @param parameter_code Code of the parameter to request
 */
void SerialComm_Helper::request_data (unsigned char parameter_code)
{
  tx_request_data((unsigned char)parameter_code);
}

/**
 * Update door state with Raspberry Pi
 * @param door_state Door state value to be updated
 */
void SerialComm_Helper::update_door (unsigned char door_state)
{
  set_data((unsigned char)parameter_code::door, &door_state);
  tx_update_data((unsigned char)parameter_code::door);
}

/**
 * Update lock state with Raspberry Pi
 * @param lock_state lock state value to be updated
 */
void SerialComm_Helper::update_lock (unsigned char lock_state)
{
  set_data((unsigned char)parameter_code::lock, &lock_state);
  tx_update_data((unsigned char)parameter_code::lock);
  if (debug) Serial.println(" update_lock() done");
}

/**
 * Clear the contents of lora_msg
 */
void SerialComm_Helper::lora_msg_clear ()
{
  lora_msg.clear();
}

/**
 * Send Raspberry Pi the command to prepare for shutdown
 */
void SerialComm_Helper::tx_sleep_raspberry ()
{
  tx_add_to_queue((unsigned char)cmd_code::prep_for_sleep);
  tx_add_to_queue(0x00);
}


/*******************
 * Private methods
 *******************/

/**
 * Handle Serial RX
 */
void SerialComm_Helper::rx ()
{
  cmd_buffer = 0;
  data_bytes_buffer = 0;

  if (!Serial)
  {
    return;
  }

  // Check if Serial buffer has data
  if (Serial.available())
  {
    // 1) Try to read command byte
    Serial.readBytes(&cmd_buffer, CMD_BYTES);

    // Escape on cmd_buffer zero
    if (cmd_buffer == 0)
    {
      return;
    }

    // 2) Try to read the number of bytes for data_buffer
    Serial.readBytes(&data_bytes_buffer, RX_DATA_BYTES);
    if (data_bytes_buffer > 0)
    {
      // 3) If data_bytes_buffer > 0 try to read data bytes
      Serial.readBytes(data_buffer, data_bytes_buffer);
    }

    if (debug)
    {
      Serial.print("cmd: ");
      Serial.print(cmd_buffer);
      Serial.print("; length: ");
      Serial.print(data_bytes_buffer);
      Serial.print("; data: ");
      for(size_t i = 0; i < data_bytes_buffer; ++i)
      {
        if (debug) Serial.print(data_buffer[i] < 16 ? "0" : "");
        if (debug) Serial.print(data_buffer[i], HEX);
        if (debug) Serial.print(" ");
      }
      Serial.println();
    }

    // Try to find a matching command code
    switch (cmd_buffer)
    {
      case (int)cmd_code::request_data:
        rx_request_data();
        break;

      case (int)cmd_code::response_data:
        rx_response_data();
        break;

      case (int)cmd_code::response_state:
        rx_response_state();
        break;

      case (int)cmd_code::update_data:
        rx_update_data();
        break;

      case (int)cmd_code::unlock:
        rx_unlock();
        break;

      case (int)cmd_code::lock:
        rx_lock();
        break;

      case (int)cmd_code::lora_msg:
        rx_lora_msg();
        break;

      case (int)cmd_code::esp_restart:
        rx_esp_restart();
        break;

      case (int)cmd_code::ve_exec_toggle:
        rx_ve_exec_state();
        break;

      case (int)cmd_code::wipe_storage:
        rx_wipe_storage();
        break;

      default:
        if (debug) Serial.println(" ! rx(): not a valid cmd!");
        return;
        break;
    }
  }
  else
  {
    return;
  }

  rx();
}

/**
 * Recieve a request from Raspberry Pi and prepare a response
 */
void SerialComm_Helper::rx_request_data ()
{
  if (debug) Serial.println(" + rx_request_data()");
  if (data_bytes_buffer <= 0) return;
  for (size_t i = 0; i < data_bytes_buffer; i++)
  {
    if (data_buffer[i] == (unsigned char)parameter_code::lock)
    {
      update_lock();
    }
    if (tx_queue_response(data_buffer[i]) != 0)
    {
      if (debug) Serial.printf(" ! no data for parameter %x\n", data_buffer[i]);
    }
  }
}

/**
 * Recieve response from Raspberry Pi with data previously requested
 */
void SerialComm_Helper::rx_response_data ()
{
  if (debug) Serial.println(" + rx_response_data()");
  if (data_bytes_buffer <= 0) return;
  size_t i = 0, j = 0;
  unsigned char parameter_code, data_size;
  unsigned char* data = { 0 };
  while (i < data_bytes_buffer)
  {
    parameter_code = await_res_params.data()[i++];
    data_size = parameter_size.find((unsigned char)parameter_code)->second;
    std::copy(data_buffer + j, data_buffer + j + data_size, data);
    j += data_size;
    set_data((unsigned char)parameter_code, data);
  }
  esp_task_wdt_reset();
  await_res_params.clear();
}

/**
 * Recieve a respone for state from Raspbarry Pi that was previously requested
 */
void SerialComm_Helper::rx_response_state ()
{
  if (debug) Serial.println(" + rx_request_state()");
  if (data_bytes_buffer != 1) return;
  set_state(data_buffer[0]);
}

/**
 * Recieve updated data from Raspberry Pi
 */
void SerialComm_Helper::rx_update_data ()
{
  if (data_bytes_buffer < 2) return;
  unsigned char parameter_code = data_buffer[0];
  size_t data_len = data_bytes_buffer - 1;
  unsigned char data[data_len] = { 0 };
  std::copy(data_buffer + 1, data_buffer + data_bytes_buffer, data);
  set_data((unsigned char)parameter_code, data);
  if (debug)
  {
    Serial.print(" + rx_update_data() ");
    int16_t v = 0;
    for (size_t i = 0; i < data_len; i++) v = v | data[i] << (8 * (data_len - 1 - i));
    Serial.println(v, HEX);
  }
}

/**
 * Recieve an unlock command form Raspberry Pi
 */
void SerialComm_Helper::rx_unlock ()
{
  if (debug) Serial.println(" + rx_unlock()");
  unlock_on_serial_cmd();
}

/**
 * Recieve a lock command form Raspberry Pi
 */
void SerialComm_Helper::rx_lock ()
{
  if (debug) Serial.println(" + rx_lock()");
  lock_on_serial_cmd();
}

/**
 * Recieve a message to be send via LoRa from Raspberry Pi
 */
void SerialComm_Helper::rx_lora_msg ()
{
  if (debug) Serial.println(" + rx_lora_msg()");
  if (data_bytes_buffer <= 0) return;
  lora_msg.clear();
  lora_msg.assign(data_buffer, data_buffer + data_bytes_buffer);
}

/**
 * Recieve a restart command form Raspberry Pi
 */
void SerialComm_Helper::rx_esp_restart ()
{
  if (debug) Serial.println(" + rx_esp_restart()");
  if (data_bytes_buffer != 0x01)
  {
   if (debug)
    {
      Serial.print(" ! incompatible data lenght: ");
      Serial.println(data_bytes_buffer);
    }
    return;
  }
  if (data_buffer[0] != 0xFF)
  {
    if (debug)
    {
      Serial.print(" ! data byte not 0xFF but ");
      Serial.println(data_buffer[0]);
    }
    return;
  }
  esp_restart();
}

/**
 * Recieve a command form Raspberry Pi to toggle the VeDirectFrameHandler execution flag
 */
void SerialComm_Helper::rx_ve_exec_state ()
{
  if (debug) Serial.println(" + rx_ve_exec_state()");
  if (data_bytes_buffer != (unsigned char)0x01)
  {
    if (debug)
    {
      Serial.print(" ! incompatible data lenght: ");
      Serial.println(data_bytes_buffer);
    }
    return;
  }
  ve_exec_toggle_serial_cmd(data_buffer[0]);
}

/**
 * Recieve a command form Raspberry Pi to wipe non-volatile storage
 */
void SerialComm_Helper::rx_wipe_storage ()
{
  if (debug) Serial.println(" + rx_wipe_storage()");
  if (data_bytes_buffer != 0x01)
  {
   if (debug)
    {
      Serial.print(" ! incompatible data lenght: ");
      Serial.println(data_bytes_buffer);
    }
    return;
  }
  if (data_buffer[0] != 0xFF)
  {
    if (debug)
    {
      Serial.print(" ! data byte not 0xFF but ");
      Serial.println(data_buffer[0]);
    }
    return;
  }
  wipe_storage_on_serial_cmd();
}


/**
 * Hanlde Serial TX
 */
void SerialComm_Helper::tx ()
{
  // Check for queued requests
  if (queue_req_params.size() > 0)
  {
    // append request command code
    tx_queue.push_back((unsigned char)cmd_code::request_data);
    // append number of requested parameters
    tx_queue.push_back(queue_req_params.size());
    // append the parameters to be requested
    tx_queue.insert(tx_queue.end(), queue_req_params.begin(), queue_req_params.end());
    // set the parameters that are expected from a response
    await_res_params = queue_req_params;
    queue_req_params.clear();
  }

  // check for queued responses
  if (queue_res_params.size() > 0)
  {
    // append response command code
    tx_queue.push_back((unsigned char)cmd_code::response_data);
    // append number of response parameters
    tx_queue.push_back(queue_res_params.size());
    // append the parameter values to response with
    tx_queue.insert(tx_queue.end(), queue_res_params.begin(), queue_res_params.end());
    queue_res_params.clear();
  }

  // check if tx_queue is populated
  if (tx_queue.size() <= 0) return;
  // append terminating byte
  tx_queue.push_back(0x00);
  Serial.write(tx_queue.data(), tx_queue.size());
  if (debug)
  {
    Serial.print("\t");
    print_hex(tx_queue.data(), tx_queue.size());
    Serial.println(" tx() done");
  }
  tx_queue.clear();
}

/**
 * Add a parameter to the next request
 */
void SerialComm_Helper::tx_request_data (unsigned char parameter_code)
{
  queue_req_params.push_back((unsigned char)parameter_code);
}

/**
 * Request the current state from Raspberry Pi
 */
void SerialComm_Helper::tx_request_state ()
{
  tx_queue.push_back((const unsigned char)cmd_code::request_state);
  tx_queue.push_back(0);
}

/**
 * Send an updated parameter to Raspberry Pi
 */
void SerialComm_Helper::tx_update_data (unsigned char parameter_code)
{
  unsigned char data_size = parameter_size.find((unsigned char)parameter_code)->second;
  const unsigned char* data = get_data((unsigned char)parameter_code);
  if (data == nullptr) return;
  tx_queue.push_back((const unsigned char)cmd_code::update_data);
  tx_queue.push_back(data_size + 1);
  tx_queue.push_back((unsigned char)parameter_code);
  tx_queue.insert(tx_queue.end(), data, data + data_size);
}

/**
 * Send an state update to Raspberry Pi
 */
void SerialComm_Helper::tx_update_state (unsigned char new_state)
{
  tx_queue.push_back((const unsigned char)cmd_code::update_state);
  tx_queue.push_back(1);
  tx_queue.push_back(new_state);
}

/**
 * Append byte to tx queue
 * @param data Data byte
 */
void SerialComm_Helper::tx_add_to_queue (const unsigned char data)
{
  tx_queue.push_back(data);
}

/**
 * Append number of bytes to tx queue
 * @param data Data bytes
 * @param data_bytes Number of data bytes
 */
void SerialComm_Helper::tx_add_to_queue (const unsigned char* data, const char data_bytes)
{
  tx_queue.insert(tx_queue.end(), data, data + data_bytes);
}

/**
 * Append a response for a request from Raspberry Pi
 * @param parameter_code The requested parameter
 * @return 0 response has been added successfully; -1 if data for the parameter was not available
 */
char SerialComm_Helper::tx_queue_response (unsigned char parameter_code)
{
  const unsigned char* data = get_data((unsigned char)parameter_code);
  unsigned char size = parameter_size.find((unsigned char)parameter_code)->second;
  size_t i = 0;
  queue_res_params.push_back((unsigned char)parameter_code);
  if (data == nullptr)
  {
    unsigned char d[size];
    memset(d, 0xFF, size);
    while (i < size) queue_res_params.push_back(d[i++]);
    return -1;
  }
  while (i < size) queue_res_params.push_back(data[i++]);
  return 0;
}