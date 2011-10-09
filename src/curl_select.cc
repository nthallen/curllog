/**
  * \file curl_select.cc
  */
#include "config.h"
#include "curllog/curl_select.h"

curl_selectee::curl_selectee(int fd, int flags, curl_multi_obj *co_in) : Selectee(fd, flags) {
  co = co_in;
}

/**
 * \returns non-zero if the event loop should terminate.
 */
int curl_selectee::ProcessData(int flag) {
  int ev_bitmask = 0;
  #if HAVE_CURL_MULTI_SOCKET_ACTION
    if ( flag & Selector::Sel_Read) ev_bitmask |= CURL_CSELECT_IN;
    if ( flag & Selector::Sel_Write) ev_bitmask |= CURL_CSELECT_OUT;
    if ( flag & Selector::Sel_Except) ev_bitmask |= CURL_CSELECT_ERR;
  #endif
  return co->socket_action(fd, ev_bitmask);
}
