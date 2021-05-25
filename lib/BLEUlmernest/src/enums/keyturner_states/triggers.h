#ifndef TRIGGERS_H
#define TRIGGERS_H

enum class triggers : unsigned char
{
    system      = 0x00, // via bluetooth command
    manual      = 0x01, // key from outside, inside wheel
    button      = 0x02, // button press
    automatic   = 0x03, // i.e. timer
    auto_lock   = 0x06 // auto relock of the smartlock
};

#endif