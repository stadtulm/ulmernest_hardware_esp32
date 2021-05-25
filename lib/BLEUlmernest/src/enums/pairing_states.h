#ifndef PAIRING_STATE_H
#define PAIRING_STATE_H

enum class pairing_state
{
  idle,
  get_pk,
  get_pk_cont,
  send_pk,
  calc_shared_secret,
  challenge,
  challenge_cont,
  challenge_auth,
  conf_auth_id,
  done,
  failed
};

#endif // PAIRING_STATE_H
