#ifndef NUKI_STATEs_H
#define NUKI_STATEs_H

enum class nuki_states : unsigned char
{
    uninitialized       = 0x00,
    pairing_mode        = 0x01,
    door_mode           = 0x02,
    maintenance_mode    = 0x04
};

#endif