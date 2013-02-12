/**
 * \file Selectee.cc
 */
#include "Selector.h"

Selectee::Selectee(int fd_in, int flag) {
  flags = flag;
  fd = fd_in;
}

Timeout *Selectee::GetTimeout() {
  return NULL;
}
