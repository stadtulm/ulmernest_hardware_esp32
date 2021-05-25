#ifndef DATASTRUCTURE_H
#define DATASTRUCTURE_H

#include <map>

/**
 * Serial command codes
 */
enum class cmd_code : unsigned char
{
  request_data    = 0x01,
  response_data   = 0x10,
  request_state   = 0x02,
  response_state  = 0x20,
  update_data     = 0x03,
  update_state    = 0x13,
  unlock          = 0x04,
  lock            = 0x40,
  lora_msg        = 0x11,
  prep_for_sleep  = 0x06,
  esp_restart     = 0x07,
  ve_exec_toggle  = 0x08,
  wipe_storage    = 0x09
};

/**
 * Parameter codes
 */
enum class parameter_code : char
{
  temp_outside = 0x01,
  temp_inside,
  humidity_inside,
  door,
  lock,
  motion,
  smoke_detector,
  smoke_detector_reset,
  light,
  light_switch,
  battery_volt,
  fan_state,
  mppt_battery_volt,
  mppt_load_energy,
  PV_yield,
  states_bit_field
};

enum class states_bitmask : unsigned char
{
  door                  = 1 << 7,
  lock                  = 1 << 6,
  smoke_detector        = 1 << 5,
  smoke_detector_reset  = 1 << 4,
  motion                = 1 << 3,
  light                 = 1 << 2,
  light_switch          = 1 << 1,
  fan                   = 1
};

extern std::map<unsigned char, unsigned char> parameter_size;

#endif