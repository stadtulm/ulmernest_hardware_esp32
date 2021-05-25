#ifndef LOCK_STATES_H
#define LOCK_STATES_H

enum class lock_states : unsigned char
{
    uncalibrated        = 0x00,
    locked              = 0x01,
    unlocking           = 0x02,
    unlocked            = 0x03,
    locking             = 0x04,
    unlatched           = 0x05,
    unlocked_lock_n_go  = 0x06,
    unlatching          = 0x07,
    calibration         = 0xFC,
    boot_run            = 0xFD,
    motor_blocked       = 0xFE,
    undefined           = 0xFF
};

#endif