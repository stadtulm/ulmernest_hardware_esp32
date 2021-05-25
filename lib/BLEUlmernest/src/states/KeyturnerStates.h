#ifndef KEYTURNERSTATES_H
#define KEYTURNERSTATES_H

typedef struct
{
    unsigned char nuki_state;
    unsigned char lock_state;
    unsigned char trigger;
    unsigned char current_time[7];
    signed short int timezone_offset;
    unsigned char critical_battery_state;
    unsigned char config_update_count;
    unsigned char lock_n_go_timer;
    unsigned char last_lock_action;
    unsigned char last_lock_action_trigger;
    unsigned char last_lock_action_completion_status;
    unsigned char door_sensor_state;
    unsigned short nightmode_active;
    unsigned char accessory_battery_state;
} KeyturnerStates;

#endif