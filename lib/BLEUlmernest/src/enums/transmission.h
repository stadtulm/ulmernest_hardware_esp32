#ifndef TRANSMISSION_H
#define TRANSMISSION_H

enum class transmission
{
  t_idle,
  t_await_rx,
  t_rx_success,
  t_challenge,
  t_done,
  t_failed
};

#endif // TRANSMISSION_H
