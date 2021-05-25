#ifndef LOCKACTION_H
#define LOCKACTION_H

enum class enum_lock_action : unsigned char
{
  unlock = 0x01,
  lock = 0x02,
  unlatch = 0x03,
  lock_and_go = 0x04,
  lock_and_go_unlatch = 0x05,
  full_lock = 0x06,
  fob_action_1 = 0x81,
  fob_action_2 = 0x82,
  fob_action_3 = 0x83
};

#endif
