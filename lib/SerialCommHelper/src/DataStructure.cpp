#include "DataStructure.h"
#include <map>

/**
 * Number of bytes used to store parameter values.
 */
std::map<unsigned char, unsigned char> parameter_size
{
  { (unsigned char)parameter_code::temp_outside,         2 },
  { (unsigned char)parameter_code::temp_inside,          2 },
  { (unsigned char)parameter_code::humidity_inside,      1 },
  { (unsigned char)parameter_code::door,                 1 },
  { (unsigned char)parameter_code::lock,                 1 },
  { (unsigned char)parameter_code::smoke_detector,       1 },
  { (unsigned char)parameter_code::smoke_detector_reset, 1 },
  { (unsigned char)parameter_code::motion,               1 },
  { (unsigned char)parameter_code::light,                1 },
  { (unsigned char)parameter_code::light_switch,         1 },
  { (unsigned char)parameter_code::battery_volt,         2 },
  { (unsigned char)parameter_code::fan_state,            1 },
  { (unsigned char)parameter_code::mppt_battery_volt,    2 },
  { (unsigned char)parameter_code::mppt_load_energy,     2 },
  { (unsigned char)parameter_code::PV_yield,             2 },
  { (unsigned char)parameter_code::states_bit_field,     1 }
};