#ifndef DOOR_SENSOR_STATES_H
#define DOOR_SENSOR_STATES_H

enum class door_sensor_state : unsigned char
{
    unavailable         = 0x00,
    deactivated         = 0x01,
    door_closed         = 0x02,
    door_open           = 0x03,
    door_state_unknown  = 0x04,
    calibrating         = 0x05
};

#endif