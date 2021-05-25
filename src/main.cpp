#include <Arduino.h>
#include <map>
#include <rom/rtc.h>


/*****************************
 * Task watchdog timer (wdt)
 *****************************/

#include <esp_task_wdt.h>
#define WDT_TIMEOUT_SECONDS 30


/******************************************
 * Serial Communication with Raspberry Pi
 ******************************************/

#include "SerialCommHelper.h"
const uint8_t SLEEP_RASPBERRY_PIN = (13);


/*****************
 * BLE Ulmernest
 *****************/

#include "BLEUlmernest.h"
#include "enums/lock_actions.h"
#include "enums/keyturner_states/lock_states.h"


/***********
 * LoRaWan
 ***********/

#include "LoRa.h"
#include <CayenneLPP.h>
uint64_t hourly_timer = 0;
const uint32_t hourly_interval = 3600000; // milliseconds
bool sent_last_reset_reason = false;


/************************
 * VeDirectFrameHandler
 ************************/

#include <HardwareSerial.h>
#include "VeDirectFrameHandler.h"
#define VE_RX (4)
#define VE_TX (2)
VeDirectFrameHandler ve_handler;
HardwareSerial ve_Serial(2);
// could lead to some problems if activated
bool ve_exec = false;
uint64_t ve_time = 0;
const uint16_t ve_interval = 1000; // milliseconds
// compansate for missed intervalls. increment on non-recording intervalls. reset on recorded intervall and lora tx
uint8_t ve_intervals_missed = 0;
std::vector<int32_t> ve_load_energy { 0 };
// VeDirect labels
const char yield_today[]         = "H20",
           max_power_today[]     = "H21",
           yield_yesterday[]     = "H22",
           max_power_yesterday[] = "H23";
bool ve_no_serial_error = true;


/*********
 * Tasks
 *********/

TaskHandle_t ve_task_handle;
TaskHandle_t sleep_raspberry_task;


/************************
 * Function delcaration
 ************************/

/**
 * main.cpp implementation of BLE Ulmernest lock_action
 *
 * @param action Action command code for Nuki SL
 *
 * @return  Returns -1 if an error occured,
 *          otherwise return the resulting keyturner lock state returned from Nuki SL
 */
int lock_action (unsigned char);

// main.cpp implementation for getting data
const unsigned char* _get_data (unsigned char);

// main.cpp implementation for setting data
void _set_data(unsigned char, unsigned char*);

// check if data has been set to a value
bool has_data (unsigned char);

// get previous data value. Intended use is for comparison with current data value
const unsigned char* _get_prev_data (unsigned char);

// set current data value as previous. Use before new value is assigned
void _set_prev_data (unsigned char);

/**
 * Use to compare prev data value with current data value.
 *
 * @param parameter_code  Byte code of specific data
 * @param min_difference  Minimum difference threshold for comparison.
 *                        Default Value is 0. Always return true if min_difference is set to less than 0.
 *
 * @return  Returns true if the current value is lesser or greater than the previous value by min_difference or more.
 *          Also returns true if min_difference is set to less than 0.
 *          Return false if the current value is NOT lesser or greater than the previous value by min_difference or more,
 *          or if neither the current nor the previous data hold any value.
 */
bool different_from_prev (unsigned char parameter_code, int8_t min_difference);

// Read the incomming VeDirect protocol and make the data available
void read_ve_data ();

// Task running a loop of read_ve_data()
void ve_task (void*);

// Get the number of locking actions done by Nuki SL in the last LoRa interval
int check_lock_action_count ();

/**
 * Helper function to convert Nuki SL datetime timestamp to seconds for simpler comparison.
 * Subtracts 2000 from the year for a more managable output value.
 *
 * @param datetime Nuki SL datetime bytes
 *
 * @return Seconds converted from datetime timestamp
 */
uint32_t datetime_to_sec (uint8_t* datetime);

// Turn off Raspberry Pi: Send Serial command, wait and invert SLEEP_RASPBERRY_PIN.
void sleep_raspberry();

// Run sleep_raspberry() as a task.
void init_sleep_raspberry(void* p);

// Turn on Raspberry Pi: Invert SLEEP_RASPBERRY_PIN
void wake_raspberry();


/*****************
 * Data Handling
 *****************/

SerialComm_Helper serial_comm(Serial);

// uint8_t parameter_code, std::vector<uint8_t> data
std::map<unsigned char, std::vector<unsigned char>> map_data;

// uint8_t parameter_code, std::vector<uint8_t> data
std::map<unsigned char, std::vector<unsigned char>> prev_sent_data;

unsigned char exec_state = 99, prev_exec_state = 99, err_code = 0;
signed int door_counter = 0, lock_counter, motion_counter, light_switch_counter;

// Data formating for LoRa
CayenneLPP lpp(51);


/*********
 * Setup
 *********/

void setup ()
{
  // Start serial port
  Serial.begin(115200);
  while (!Serial);
  if (!debug) Serial.setDebugOutput(0);
  if (debug) Serial.println("Serial begin");

  // Setup GPIOs
  pinMode(SLEEP_RASPBERRY_PIN, OUTPUT);

  // Power Raspberry Pi
  wake_raspberry();

  // BLE Ulmernest initiation
  BLEUlmernest::init("nest_esp32_99");

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();
  // Start job (sending automatically starts OTAA too)
  do_send(&sendjob);

  // init watchdog timer and add setup+loop
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL);

  // VeDirect Serial
  ve_Serial.begin(19200, SERIAL_8N1, VE_RX, VE_TX);
  while (!ve_Serial);
  if (debug) Serial.println("ve_Serial begin");

  xTaskCreatePinnedToCore(ve_task, "ve_task", 1024, (void*) 1, 1, &ve_task_handle, 0);
}


/********
 * Loop
 ********/

void loop() {
  if (ve_exec) read_ve_data();
  serial_comm.loop();
  os_runloop_once();

  esp_task_wdt_reset();
}


/************************
 * Function definitions
 ************************/

/**
 * main.cpp implementation of BLE Ulmernest lock_action
 */
int lock_action (unsigned char action)
{
  int status = -1;

  if (1) // impl check door state
  {
    status = BLEUlmernest::lock_action(action);
  }
  else // door not closed so retract bolt > unlock
  {
    if (debug) Serial.println(" ! door sensor: door not closed or unknown state");
    if (BLEUlmernest::get_keytuerner_states().lock_state == (unsigned char)lock_states::locked)
    {
      status = BLEUlmernest::lock_action((unsigned char)enum_lock_action::unlock);
    }
  }

  if (status == -1)
  {
    if (debug) Serial.println(" ! lock_action: something went wrong");
    BLEUlmernest::read_keyturner_state();
    serial_comm.update_lock(BLEUlmernest::get_keytuerner_states().lock_state);
  }
  else
  {
    if (debug)
    {
      Serial.print(" - lock_action: final status ");
      Serial.println(status);
    }
    serial_comm.update_lock(status);
  }
  return status;
}

/**
 * main.cpp implementation for getting data
 */
const unsigned char* _get_data (unsigned char parameter_code)
{
  // Check if key of parameter_code has been assigned a value
  if (map_data.count(parameter_code) != 0)
  {
    return map_data[parameter_code].data();
  }
  else
  {
     if (debug)
    {
      Serial.print(" ! parameter code ");
      Serial.print(parameter_code, 16);
      Serial.println(": not part of map_data");
    }
    return nullptr;
  }
}

/**
 * main.cpp implementation for setting data
 */
void _set_data (unsigned char _parameter_code, unsigned char* data)
{
  std::vector<unsigned char> v;
  for (size_t i = 0; i < parameter_size.find(_parameter_code)->second; i++) v.push_back(data[i]);

  // Check if parameter already has been assigned a value
  // if not create a new key-value-pair
  // otherwise replace the existing value
  if (map_data.count(_parameter_code) == 0)
  {
    map_data.insert(std::make_pair(_parameter_code, v));
    if (debug)
    {
      // Serial.printf(" - 0x%x new data entry ", _parameter_code);
      Serial.print(" - 0x");
      Serial.print(_parameter_code, HEX);
      Serial.print(" new data entry ");
      print_hex(_get_data(_parameter_code), parameter_size.find(_parameter_code)->second);
    }
  }
  else
  {
    map_data[_parameter_code] = v;
    if (debug >= 2)
    {
      // Serial.printf(" - 0x%x update data entry ", _parameter_code);
      Serial.print(" - 0x");
      Serial.print(_parameter_code, HEX);
      Serial.print(" update data entry ");
      print_hex(_get_data(_parameter_code), parameter_size.find(_parameter_code)->second);
    }
  }

  // For certain parameters, also increment a counter variable on change
  switch (_parameter_code)
  {
  case (unsigned char)parameter_code::door:
    door_counter++;
    break;

  case (unsigned char)parameter_code::motion:
    motion_counter++;
    break;

  case (unsigned char)parameter_code::light_switch:
    light_switch_counter++;
    break;

  default:
    break;
  }
}

/**
 * check if data has been set to a value
 */
bool has_data (unsigned char parameter_code)
{
  return map_data.count(parameter_code) != 0;
}

/**
 * get previous data value. Intended use is for comparison with current data value
 */
const unsigned char* _get_prev_data (unsigned char parameter_code)
{
  // Check if key of parameter_code has been assigned a value
  if (prev_sent_data.count(parameter_code) != 0)
  {
    return prev_sent_data[parameter_code].data();
  }
  else
  {
     if (debug)
    {
      Serial.print(" ! parameter code ");
      Serial.print(parameter_code, 16);
      Serial.println(": not part of prev_sent_data");
    }
    return nullptr;
  }
}

/**
 * set current data value as previous. Use before new value is assigned
 */
void _set_prev_data (unsigned char _parameter_code)
{
  std::vector<unsigned char> v;
  const unsigned char* data = _get_data(_parameter_code);
  for (size_t i = 0; i < parameter_size.find(_parameter_code)->second; i++) v.push_back(data[i]);

  if (prev_sent_data.count(_parameter_code) == 0)
  {
    if (debug) Serial.printf(" - 0x%x new prev data entry ", _parameter_code);
    prev_sent_data.insert(std::make_pair(_parameter_code, v));
    print_hex(_get_data(_parameter_code), parameter_size.find(_parameter_code)->second);
  }
  else
  {
    if (debug >= 2) Serial.printf(" - 0x%x update prev data entry ", _parameter_code);
    prev_sent_data[_parameter_code] = v;
    print_hex(_get_data(_parameter_code), parameter_size.find(_parameter_code)->second);
  }
}

/**
 * Use to compare prev data value with current data value.
 */
bool different_from_prev (unsigned char parameter_code, int8_t min_difference = 0)
{
  // Check if key of parameter_code for current data has been assigned a value
  if (map_data.count(parameter_code) == 0)
  {
    if (debug) Serial.println(" - different_from_prev: no current data; return false");
    return false;
  }
  // Check if key of parameter_code for previous data has been assigned a value
  if (prev_sent_data.count(parameter_code) == 0)
  {
    if (debug) Serial.println(" - different_from_prev: has current data, but no previous; return true");
    return true;
  }

  int16_t p = prev_sent_data[parameter_code][0] << 8 | prev_sent_data[parameter_code][1];
  int16_t d = map_data[parameter_code][0] << 8 | map_data[parameter_code][1];
  if (debug)
  {
    Serial.printf(" - previous data: %d; current data: %d; min. difference: %d\n", p, d, min_difference);
  }

  // Compare current and previous
  if (d < p - min_difference)
  {
    if (debug) Serial.printf(" - current data %d is smaller than previous data %d by more than %d\n", d, p, min_difference);
    return true;
  }
  else if (d > p + min_difference)
  {
    if (debug) Serial.printf(" - current data %d is greater than previous data %d by more than %d\n", d, p, min_difference);
    return true;
  }
  // Return true if min_difference < 0
  else if (min_difference < 0)
  {
    if (debug) Serial.println(" - minimum difference has a negativ value; current value always be sent");
    return true;
  }
  else return false;
}

/**
 * Read the incomming VeDirect protocol and make the data available
 */
void read_ve_data ()
{
  // Initially clear serial rx queue to prevent buffer overflow error in VeDirectFrameHandler
  if (ve_time == 0) ve_Serial.flush();

  // Check for 1 second interval and if lmic library has joined the LoRa network
  if (millis() - ve_time >= ve_interval && lmic_is_joined)
  {
    ve_time = millis();

    // Check for new serial data
    if (!ve_Serial.available() )
    {
      // Prevent debug spam
      if (ve_no_serial_error)
      {
        ve_no_serial_error = false;
        if (debug)
        {
          Serial.println(" ! no VeDirect serial message for this intervall");
        }
      }

      // account for missed frames
      ve_intervals_missed++;
      return;
    }
    else
    {
      // reset no serial error flag
      ve_no_serial_error = true;
    }

    // read serial buffer and apply VeDirectFrameHanlder
    while (ve_Serial.available())
    {
      ve_handler.rxData(ve_Serial.read());
    }
    if (debug >= 2)
    {
      Serial.print("\n + VeDirect read data: ");
      for (size_t i = 0; i < ve_handler.veEnd; i++)
      {
        Serial.print(ve_handler.veName[i]);
        Serial.print(" ");
      }
      Serial.println();
    }

    // Get the recieved data from the frame handler
    int16_t mV = 0, mA = 0;
    for (size_t i = 0; i < ve_handler.veEnd; i++)
    {
      if (String(ve_handler.veName[i]).compareTo("V") == 0)
      {
        mV = std::strtoul(ve_handler.veValue[i], NULL, 10);
        uint8_t data[2] = { (mV >> 8), mV };
        _set_data((unsigned char)parameter_code::mppt_battery_volt, data);
        // if (debug)
        // {
        //   Serial.print("   mV: ");
        //   Serial.println(mV);
        // }
      }
      else if (String(ve_handler.veName[i]).compareTo("IL") == 0)
      {
        mA = std::strtoul(ve_handler.veValue[i], NULL, 10);
        // if (debug)
        // {
        //   Serial.print("   load mA: ");
        //   Serial.println(mA);
        // }
      }
      else if (String(ve_handler.veName[i]).compareTo(String(yield_today)) == 0)
      {
        uint16_t ve_value = std::strtoul(ve_handler.veValue[i], NULL, 10);
        uint8_t data[2] = { (ve_value >> 8), ve_value };
        _set_data((unsigned char)parameter_code::PV_yield, data);
        // if (debug)
        // {
        //   Serial.print("   yield today: ");
        //   Serial.println(mA);
        // }
      }
      else if (String(ve_handler.veName[i]).compareTo(String(max_power_today)) == 0)
      {}
      else if (String(ve_handler.veName[i]).compareTo(String(yield_yesterday)) == 0)
      {}
      else if (String(ve_handler.veName[i]).compareTo(String(max_power_yesterday)) == 0)
      {}
    }

    ve_load_energy[ve_load_energy.size() - 1] += (mV * mA / 1000) * (1 + ve_intervals_missed);
    ve_intervals_missed = 0;

    // if (debug)
    // {
    //   Serial.print("   ");
    //   Serial.print(ve_load_energy[ve_load_energy.size() - 1]);
    //   Serial.println(" mJ since last TX_INTERVAL");
    // }
  }
  else
  {
    // ve_Serial.flush();
  }
}

/**
 * Task running a loop of read_ve_data()
 */
void ve_task (void* param)
{
  // register watchdog timer
  esp_task_wdt_add(NULL);
  for (;;)
  {
    read_ve_data();
    esp_task_wdt_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/**
 * Get the number of locking actions done by Nuki SL in the last LoRa interval
 */
int check_lock_action_count ()
{
  std::vector<std::vector<uint8_t>> logs;
  // within time to last lora transmission
  bool in_time_frame = true;
  int lock_action_count = 0;

  // read keyturner state to get the current timestamp from Nuki SL
  if (BLEUlmernest::read_keyturner_state() != 0)
  {
    if (debug) {
      Serial.println(" ! could not read keyturner state");
      return 0;
    }
  }
  unsigned char* datetime = BLEUlmernest::get_keytuerner_states().current_time;
  unsigned char log_datetime[7] = {0};
  // calculate current secs from current datetime
  uint32_t current_secs = datetime_to_sec(datetime);
  uint32_t log_secs;

  if (debug)
  {
    Serial.print(" - datetime ");
    print_hex(datetime, 7);
    Serial.print(" - currently ");
    Serial.print(current_secs);
    Serial.println(" secs");
  }

  uint16_t start_index = 0, count = 5, logs_available = 0;
  while (in_time_frame)
  {
    if (debug)
    {
      Serial.print(" # request log entries at start index ");
      Serial.print(start_index);
      Serial.print(" and count ");
      Serial.println(count);
    }
    // get log entries from Nuki SL
    logs = BLEUlmernest::req_log_entries(start_index, count, logs_available);

    if (logs.size() == 0)
    {
      if (debug)
      {
        Serial.println(" ! no logs");
      }
      in_time_frame = false;
      // return 0;
    }

    for (std::vector<std::vector<uint8_t>>::size_type i = 0; i < logs.size(); i++)
    {
      // get the log timestamp
      for (size_t j = 0; j < 7; j++) log_datetime[j] = logs[i].data()[j + 10];
      if (debug)
      {
        Serial.print(" - log datetime ");
        print_hex(log_datetime, 7);
      }

      // calculate secs for a log from the datetime of the log
      log_secs = datetime_to_sec(log_datetime);

      if (debug)
      {
        Serial.print(" - log ");
        Serial.print((uint32_t)0x00000000 | logs[i].data()[6] | logs[i].data()[7] << 8 | logs[i].data()[8] << 16 | logs[i].data()[9] << 24);
        Serial.print(" with ");
        Serial.print(log_secs);
        Serial.print(" secs of type ");
        Serial.println(logs[i].data()[53]);

        Serial.printf(" - difference %d - %d = %d\n", current_secs, log_secs, current_secs - log_secs);
      }

      if (current_secs - log_secs <= TX_INTERVAL && current_secs - log_secs > 0)
      {
        // check if the log entry is a lock action, then increment lock_action_counter
        if (logs[i].data()[53] == 0x02) lock_action_count++;
      }
      else
      {
        // break out of while loop and for loop
        in_time_frame = false;
        i = logs.size();
      }
    }
    // setup for next loop
    // clear logs
    logs.clear();
    // set the log that will be the starting point for the next iteration
    if (start_index == 0) start_index = logs_available - count;
    else start_index -= count;
    delay(2);
    // esp_task_wdt_reset();
  }
  if (debug)
  {
    Serial.print(" - lock action count: ");
    Serial.println(lock_action_count);
  }
  return lock_action_count;
}

/**
 * Helper function to convert Nuki SL datetime timestamp to seconds for simpler comparison
 */
uint32_t datetime_to_sec (uint8_t* datetime)
{
  uint32_t secs = 0;
  uint16_t year = datetime[0] | datetime[1] << 8;
  uint8_t month = datetime[2], day = datetime[3], hour = datetime[4], min = datetime[5], sec = datetime[6];

  // Get secs from seconds, minutes, hours, and days of the month
  // Subtracts one form day of the month, since it's accounted for by seconds, minutes and hours.
  secs = sec + min * 60 + hour * 3600 + (day - 1) * 86400;
  if (month > 1)
  {
    // 31-day months: Every one in two months until the 8th month, where an additional 31 day month is introduced
    secs += month / 2 * 2678400;
    if (month > 8) secs +=  2678400;

    // febuary 28 or 29 days
    if (month > 2)
    {
      secs += 2419200;
      // every 4 years, except every turn of a century if it is not dividable by 400
      if (year % 4 == 0 && !(year % 100 == 0) && year % 400 == 0) secs += 86400;
    }

    // 30-day months
    // every other month before 8th, exclude current month, subtract one to account for february
    if (month < 8) secs += ( ( (month - 1) / 2 ) - 1 ) * 2592000;
    // all 30-day months before the 8th month
    else secs += 5184000;
    if (month > 9) secs += 2592000;
    if (month > 11) secs += 2592000;
  }
  // subtracting 2000 years to get a simpler seconds value to work with
  secs += (year - 2000) * 31536000;
  // account for leap years
  secs += (year - 2001) / 4 * 86400;

  return secs;
}


/**************************************************
 * Implementation SerialComm_Helper class methods
 **************************************************/

/**
 * Implemente setting data for SerialComm_Helper
 */
void SerialComm_Helper::set_data (unsigned char parameter_code, unsigned char* data)
{
  _set_data(parameter_code, data);
}

/**
 * Implemente getting data for SerialComm_Helper
 */
const unsigned char* SerialComm_Helper::get_data (unsigned char parameter_code)
{
  return _get_data(parameter_code);
}

/**
 * Implemente setting state for SerialComm_Helper
 */
void SerialComm_Helper::set_state (unsigned char state)
{
  exec_state = state;
  tx_update_state(state);
}

/**
 * Implemente getting state for SerialComm_Helper
 */
const unsigned char SerialComm_Helper::get_state ()
{
  return exec_state;
}

/**
 * Implemente updating lock for SerialComm_Helper
 */
void SerialComm_Helper::update_lock ()
{
  BLEUlmernest::read_keyturner_state();
  unsigned char lock_state = BLEUlmernest::get_keytuerner_states().lock_state;
  _set_data((unsigned char)parameter_code::lock, &lock_state);
}

/**
 * Implemente unlock command for SerialComm_Helper
 */
void SerialComm_Helper::unlock_on_serial_cmd ()
{
  int status = lock_action((unsigned char)enum_lock_action::unlock);
  if (status != 0) err_code = status;
}

/**
 * Implemente lock command for SerialComm_Helper
 */
void SerialComm_Helper::lock_on_serial_cmd ()
{
  int status = lock_action((unsigned char)enum_lock_action::lock);
  if (status != 0) err_code = status;
}

/**
 * Implemente toggling VeDirectFrameHandler execution flag for SerialComm_Helper
 */
void SerialComm_Helper::ve_exec_toggle_serial_cmd (uint8_t data)
{
  if (data > 0)
  {
    ve_exec = true;
    if (debug)
    {
      Serial.println(" # ve_exec_toggle_serial_cmd: VeDirect handler is now enabled");
    }
  }
  else if (data == 0)
  {
    ve_exec = false;
    if (debug)
    {
      Serial.println(" # ve_exec_toggle_serial_cmd: VeDirect handler is now disabled");
    }
  }
  else
  {
    if (debug)
    {
      Serial.print(" ! ve_exec_toggle_serial_cmd: unexpected value ");
      Serial.println(data);
    }
    return;
  }
}

/**
 * Implemente wiping non-volatile storage for SerialComm_Helper
 */
void SerialComm_Helper::wipe_storage_on_serial_cmd ()
{
  BLEUlmernest::get_Bund()->wipe_storage();
}


/*********************************
 * Implementation LoRa functions
 *********************************/

/**
 * Assamble and return bytes for LoRa transmission
 */
uint8_t* lora_queue ()
{
  // reset Cayenne LPP object
  lpp.reset();

  // check for hourly transmissions
  bool hourly;
  if (millis() - hourly_timer > hourly_interval)
  {
    hourly = true;
    hourly_timer = millis();
  }
  else
  {
    hourly = false;
  }

  // check for serial LoRa message
  if (serial_comm.get_lora_msg_size() > 0)
  {
    unsigned char* m = serial_comm.get_lora_msg();
    serial_comm.lora_msg_clear();
    return m;
  }

  // 0 - Debug
  // if (debug) lpp.addDigitalInput(0, 0xFF);

  /**
   * 0 - Fehler
   *
   * Reset Reason
   * After connection to LoRa send the reset reason for both cores.
   * Since there are 16 possible reset reasons both can be containted in a single Byte.
   * The values for the reset reasons range from 1 to 16, so to accomedate them as
   * a single digit hex value the value has to have one subtracted.
   * This has to be accounted for on the recieving end!
   */
  if (lmic_is_joined && !sent_last_reset_reason)
  {
    uint8_t core_0 = rtc_get_reset_reason(0);
    uint8_t core_1 = rtc_get_reset_reason(1);
    uint8_t reset_reason_payload = 0x00 | (core_0 -1) | ((core_1 -1) << 4);
    lpp.addDigitalInput(0, reset_reason_payload);
    sent_last_reset_reason = true;
  }
  // if (err_code != 0)
  // {
  //   lpp.addDigitalInput(0, err_code);
  //   err_code = 0;
  // }

  /**
   * 1 - execution state
   * 8 Bit: state code
   */
  if (exec_state != 99 && exec_state != prev_exec_state)
  {
    lpp.addDigitalInput(1, exec_state);
    prev_exec_state = exec_state;
  }

  /**
   * 2 - Temperature outside
   * 16 Bit: 0.1 °C Signed MSB
   */
  if (has_data((unsigned char)parameter_code::temp_outside) &&
      different_from_prev((unsigned char)parameter_code::temp_outside, -1))
  {
    int16_t d = _get_data((unsigned char)parameter_code::temp_outside)[0] << 8 |
                _get_data((unsigned char)parameter_code::temp_outside)[1];
    float temp = (float)d / 10.0f;
    lpp.addTemperature(2, temp);
    // _set_prev_data((unsigned char)parameter_code::temp_outside);
    if (debug)
    {
      Serial.println();
      Serial.println(d, 16);
      Serial.print("lpp add temp outside ");
      Serial.println(temp);
    }
  }

  /**
   * 3 - Temperature inside
   * 16 Bit: 0.1 °C Signed MSB
   */
  if (has_data((unsigned char)parameter_code::temp_inside) &&
      different_from_prev((unsigned char)parameter_code::temp_inside, -1))
  {
    int16_t d = _get_data((unsigned char)parameter_code::temp_inside)[0] << 8 |
                _get_data((unsigned char)parameter_code::temp_inside)[1];
    float temp = (float)d / 10.0f;
    lpp.addTemperature(3, temp);
    // _set_prev_data((unsigned char)parameter_code::temp_inside);
    if (debug)
    {
      Serial.println();
      Serial.println(d, 16);
      Serial.print("lpp add temp inside ");
      Serial.println(temp);
    }
  }

  /**
   * 4 - Relative Humidity
   * 8 Bit: in % (0.5% steps)
   */
  if (has_data((unsigned char)parameter_code::humidity_inside) &&
      different_from_prev((unsigned char)parameter_code::humidity_inside, 4)) // min diff 2%
  {
    uint16_t rh = _get_data((unsigned char)parameter_code::humidity_inside)[0];
    lpp.addRelativeHumidity(4, (float)rh / 2.0f);
    // _set_prev_data((unsigned char)parameter_code::humidity_inside);
    if (debug)
    {
      Serial.print("lpp add humidity inside ");
      Serial.println((float)rh / 2.0f);
    }
  }

  /**
   * 5 - Door
   * door status open/closed
   */
  if (has_data((unsigned char)parameter_code::door) &&
      different_from_prev((unsigned char)parameter_code::door))
  {
    lpp.addDigitalInput(5, _get_data((unsigned char)parameter_code::door)[0]);
    // _set_prev_data((unsigned char)parameter_code::door);
    if (debug)
    {
      Serial.print("lpp add door ");
      Serial.println(_get_data((unsigned char)parameter_code::door)[0]);
    }
  }

  /**
   * 6 - Lock
   * Nuki lock state
   */
  if (has_data((unsigned char)parameter_code::lock) &&
      different_from_prev((unsigned char)parameter_code::lock))
  {
    uint8_t lock_state = _get_data((unsigned char)parameter_code::lock)[0];
    // if (lock_state != (unsigned char)lock_states::unlocked ||
    //   lock_state != (unsigned char)lock_states::unlocking ||
    //   lock_state != (unsigned char)lock_states::locked ||
    //   lock_state != (unsigned char)lock_states::locking)
    {
      lpp.addDigitalInput(6, lock_state);
      // _set_prev_data((unsigned char)parameter_code::lock);
      if (debug)
      {
        Serial.print("lpp add lock ");
        Serial.println(_get_data((unsigned char)parameter_code::lock)[0]);
      }
    }
  }

/**
 * 7 - smoke detector
 */
  if (has_data((unsigned char)parameter_code::smoke_detector) &&
      different_from_prev((unsigned char)parameter_code::smoke_detector))
  {
    lpp.addDigitalInput(7, _get_data((unsigned char)parameter_code::smoke_detector)[0]);
    // _set_prev_data((unsigned char)parameter_code::smoke_detector);
    if (debug)
    {
      Serial.print("lpp add smoke detector ");
      Serial.println(_get_data((unsigned char)parameter_code::smoke_detector)[0]);
    }
  }

  /**
   * 8 - Battery Voltage
   * Battery voltagae
   */
  if (has_data((unsigned char)parameter_code::battery_volt) &&
      different_from_prev((unsigned char)parameter_code::battery_volt))
  {
    uint16_t d = _get_data((unsigned char)parameter_code::battery_volt)[0] << 8 |
                 _get_data((unsigned char)parameter_code::battery_volt)[1];
    lpp.addAnalogInput(8, (float)d / 100.0);
    // _set_prev_data((unsigned char)parameter_code::battery_volt);
    if (debug)
    {
      Serial.print("battery voltage ");
      Serial.println(d);
    }
  }

  /**
   * 9 - Door counter
   * MSB last known state | 7B count of state changes since last TX_INTERVAL
   */
  if (door_counter != 0)
  {
    uint8_t bits = door_counter > 0b01111111 ? 0b01111111 : door_counter;
    uint8_t door_state = 0;
    if (has_data((unsigned char)parameter_code::door) &&
        different_from_prev((unsigned char)parameter_code::door))
    {
      door_state = _get_data((unsigned char)parameter_code::door)[0];
    }
    bits = door_state > 0 ? 0b10000000 | bits : bits;
    lpp.addDigitalInput(9, bits);
    if (debug)
    {
      Serial.print("lpp add door 0b");
      Serial.println(bits, BIN);
    }
  }

  /**
   * 10 - Lock counter
   * MSB last known state | 7B count state changes since last TX_INTERVAL
   */
  if (lmic_is_joined) lock_counter = check_lock_action_count();
  if (lock_counter > 0)
  {
    uint8_t bits = lock_counter > 0b01111111 ? 0b01111111 : (uint8_t)lock_counter;

    uint8_t lock_state = 0;
    if (has_data((unsigned char)parameter_code::lock))
    {
      lock_state = _get_data((unsigned char)parameter_code::lock)[0];
    }
    else
    {
      if (BLEUlmernest::read_keyturner_state() == 0) lock_state = BLEUlmernest::get_keytuerner_states().lock_state;
    }
    bits = lock_state == (uint8_t)lock_states::unlocked ? 0b10000000 | bits : bits;
    lpp.addDigitalInput(10, bits);
    if (debug)
    {
      Serial.print("lpp add lock 0b");
      Serial.println(bits, BIN);
    }
  }

  /**
   * 11 - Motion Counter
   * 8 bit: state changes since last TX_INTERVAL
   */
  if (motion_counter > 0)
  {
    lpp.addDigitalInput(11, motion_counter);
    motion_counter = 0;
  }

  /**
   * 12 - Light Switch Counter
   * 8 bit: state changes since last TX_INTERVAL
   */
  if (light_switch_counter > 0)
  {
    lpp.addDigitalInput(12, light_switch_counter);
    light_switch_counter = 0;
  }

  /**
   * 13 - MPPT Battery Voltage, hourly
   * 16 Bit: singed floating number; 0.01 V
   */
  if (hourly &&
      has_data((unsigned char)parameter_code::mppt_battery_volt) &&
      different_from_prev((unsigned char)parameter_code::mppt_battery_volt))
  {
    uint16_t d = _get_data((unsigned char)parameter_code::mppt_battery_volt)[0] << 8 |
                 _get_data((unsigned char)parameter_code::mppt_battery_volt)[1];
    lpp.addAnalogInput(13, d);
    // _set_prev_data((unsigned char)parameter_code::mppt_battery_volt);
    if (debug)
    {
      Serial.print("mppt battery voltage ");
      Serial.println(d);
    }
  }

  /**
   * 14 - Load Power, hourly
   * 16 Bit: singed floating number; 0.01 mWh
   */
  if (hourly)
  {
    double milli_watt_hours = 0;
    for (int32_t energy : ve_load_energy)
    {
      // mJ -> mWh with TX_INTERVAL sec out of an hour (3600 sec): / TX_INTERVAL/3600
      milli_watt_hours += ((double)energy / (double)TX_INTERVAL / 3600.0);
    }
    if (debug)
    {
      Serial.print(" + energy used by load in milli watt hours: ");
      Serial.println(milli_watt_hours, 2);
    }
    ve_load_energy.clear();

    lpp.addAnalogInput(14, milli_watt_hours);
  }
  // append new element to add next cycle or after clear
  ve_load_energy.push_back(0);

  /**
   * 15 - PV yield, hourly
   * 16 Bit: singed floating number; 0.01 kWh
   */
  if (hourly &&
      has_data((unsigned char)parameter_code::PV_yield) &&
      different_from_prev((unsigned char)parameter_code::PV_yield))
  {
    uint16_t d = _get_data((unsigned char)parameter_code::PV_yield)[0] << 8 |
                 _get_data((unsigned char)parameter_code::PV_yield)[1];
    lpp.addAnalogInput(15, d);
    // _set_prev_data((unsigned char)parameter_code::PV_yield);
    if (debug)
    {
      Serial.print("PV yield today ");
      Serial.println(d);
    }
  }
  return lpp.getBuffer();
}

/**
 * @return Number of bytes for lora_queue
 */
size_t lora_queue_len()
{
  return lpp.getSize();
}

/**
 * Interprete bytes recieved from a downlink.
 * @param data Data bytes
 * @param len Number of data bytes
 */
void parse_downlink (unsigned char* data, size_t len)
{
  if (debug)
  {
    Serial.print(" - downlink:");
    for (size_t i = 0; i < len; i++)
    {
      Serial.print(" 0x");
      if (data[i] <= 16) Serial.print("0");
      Serial.print(data[i], HEX);
    }
    Serial.println();
  }

  size_t i = 0;
  while (i < len)
  {
    switch (data[i++])
    {
    case 0x01: // change exec state
      if (debug) Serial.println(" - 0x01: change exec state");
      serial_comm.set_state(data[i++]);
      break;

    case 0x04: // unlock door
      if (debug) Serial.println(" - 0x04: unlock door");
      lock_action((unsigned char)enum_lock_action::unlock);
      break;

    case 0x40: // lock door
      if (debug) Serial.println(" - 0x40: lock door");
      lock_action((unsigned char)enum_lock_action::lock);
      break;

      case 0x06: // sleep raspberry
      if (debug) Serial.println(" - 0x06: sleep_raspberry");
      if (data[i++] == 0xFF) sleep_raspberry();
      else if (debug) Serial.println(" ! 2nd byte not 0xFF");
      break;

    case 0x60: // wake raspberry
      if (debug) Serial.println(" - 0x06: wake_raspberry");
      if (data[i++] == 0xFF) wake_raspberry();
      else if (debug) Serial.println(" ! 2nd byte not 0xFF");
      break;

    case 0x07: // restart esp32
      if (debug) Serial.println(" - 0x07: esp_restart");
      if (data[i++] == 0xFF) esp_restart();
      else if (debug) Serial.println(" ! 2nd byte not 0xFF");
      break;

    default:
      if (debug)
      {
        Serial.print(" ! unknown downlink value ");
        Serial.print(data[i], HEX);
        Serial.print(" on index ");
        Serial.println(i);
      }
      break;
    }
  }
}

/**
 * Shut down Raspberry Pi
 */
void sleep_raspberry ()
{
  if (debug) Serial.println(" # sleep raspberry in 20 seconds");
  // Send shutdown command to Raspberry Pi
  serial_comm.tx_sleep_raspberry();
  // Create task to turn off power to Raspberry Pi in 20 seconds
  xTaskCreatePinnedToCore(init_sleep_raspberry, "init_sleep_raspberry", 512, NULL, 1, &sleep_raspberry_task, 0);
}

/**
 * Task function to turn off power to Raspberry Pi after 20 seconds
 */
void init_sleep_raspberry(void* p)
{
  // wait 20 sec
  vTaskDelay(20000 / portTICK_PERIOD_MS);

  // set pin high
  digitalWrite(SLEEP_RASPBERRY_PIN, LOW);
  if (debug)
  {
    // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    Serial.print(" # sleep raspberry: set pin ");
    Serial.print(SLEEP_RASPBERRY_PIN);
    Serial.println(" to low");
  }

  // Delete this task
  vTaskDelete(NULL);
}

/**
 * Power on Raspberry Pi
 */
void wake_raspberry ()
{
  if (debug)
  {
    Serial.print(" # wake raspberry: set pin ");
    Serial.print(SLEEP_RASPBERRY_PIN);
    Serial.println(" to high");
  }
  digitalWrite(SLEEP_RASPBERRY_PIN, HIGH);
}
