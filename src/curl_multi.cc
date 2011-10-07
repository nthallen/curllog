#include "curllog/curl_select.h"
#include "nortlib.h"
#include "nl_assert.h"

curl_multi *curl_multi::single;

curl_multi::curl_multi() {
  curl_global *glbl = curl_global::getInstance(); // must be initialized before curl_multi_init()
  glbl = glbl;
  timeout_msec = -1;
  running_handles = 0;
  multi = curl_multi_init();
  if ( curl_multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, &static_socket_function) != CURLM_OK )
    nl_error( 3, "CURLMOPT_SOCKETFUNCTION failed" );
  if ( curl_multi_setopt(multi, CURLMOPT_SOCKETDATA, this) != CURLM_OK )
    nl_error( 3, "CURLMOPT_SOCKETDATA failed" );
  if ( curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, &static_timer_function) != CURLM_OK )
    nl_error( 3, "CURLMOPT_TIMERFUNCTION failed" );
  if ( curl_multi_setopt(multi, CURLMOPT_TIMERDATA, this) != CURLM_OK )
    nl_error( 3, "CURLMOPT_TIMERDATA failed" );
}

curl_multi::~curl_multi() {
  CURLMcode rv = curl_multi_cleanup(multi);
  if ( rv != CURLM_OK )
    nl_error(2, "curl_multi_cleanup() returned %d", rv );
  multi = NULL;
}

int curl_multi::static_socket_function(CURL *easy, curl_socket_t s,
      int what, void *userp, void *socketp) {
  curl_multi *mp = (curl_multi *)userp;
  return mp->socket_function(easy, s, what, socketp);
}

int curl_multi::static_timer_function(CURLM *multi, long timeout_ms, void *userp) {
  curl_multi *mt = (curl_multi *)userp;
  mt->timeout_msec = timeout_ms;
  return 0;
}

curl_multi *curl_multi::getInstance() {
  if (single == 0) {
    single = new curl_multi();
    atexit(&Cleanup);
  }
  return single;
}

void curl_multi::Cleanup() {
  if ( single != 0 ) {
    delete(single);
    single = 0;
  }
}

int curl_multi::ProcessTimeout() {
  return socket_action( CURL_SOCKET_TIMEOUT, 0 );
}

Timeout *curl_multi::GetTimeout() {
  if ( timeout_msec < 0 ) {
    to.Set(6, 0); // Recommended maximum
  } else {
    to.Set(0, timeout_msec);
  }
  return &to;
}

int curl_multi::socket_action(int fd, int ev_bitmask ) {
  int ready_to_quit = 0;
  int new_running_handles = running_handles;
  CURLMcode rv = curl_multi_socket_action( multi, fd, ev_bitmask, &new_running_handles );
  if ( rv != CURLM_OK )
    nl_error( 4, "Error %d from curl_multi_socket_action()", rv );
  if ( new_running_handles < running_handles ) {
    int msgs_in_queue;
    CURLMsg *msg;
    for (;;) {
      msg = curl_multi_info_read( multi, &msgs_in_queue );
      if ( msg == NULL ) {
        break;
      } else if ( msg->msg == CURLMSG_DONE ) {
        CURL *easy = msg->easy_handle;
        CURLcode code = msg->data.result;
        //CURLMcode rv;
        curl_multi_obj *co;
        rv = curl_multi_remove_handle(multi, easy);
        if ( rv != CURLM_OK )
          nl_error(4, "Error %d from curl_multi_remove_handle()", rv);
        if ( curl_easy_getinfo(easy, CURLINFO_PRIVATE, (char *)&co) == CURLE_OK && co != NULL) {
          if ( co->take_next_step(code) )
            ready_to_quit = 1;
        } else {
          nl_error(4, "Error getting curl_obj from easy_handle" );
        }
      }
    }
  }
  running_handles = new_running_handles;
  return ready_to_quit;
}

int curl_multi::socket_function(CURL *easy, curl_socket_t s,
      int what, void *socketp) {
  curl_multi_obj *co;
  int flags = 0;
  // Extract the curl_obj pointer from the easy handle
  curl_easy_getinfo(easy, CURLINFO_PRIVATE, (char *)&co);
  if ( co == NULL )
    nl_error( 4, "No curl_obj found for socket in curl_multi::socket_function()" );
  switch (what) {
    case CURL_POLL_NONE: // Add socket to the list of sockets
      flags = 0;
      break;
    case CURL_POLL_IN: // Note that it wants to read
      flags = Sel_Read | Sel_Except;
      break;
    case CURL_POLL_OUT: // Note that it wants to write
      flags = Sel_Write | Sel_Except;
      break;
    case CURL_POLL_INOUT:  // Note that it wants to read & write
      flags = Sel_Read | Sel_Write | Sel_Except;
      break;
    case CURL_POLL_REMOVE: // Remove it from the list
      delete_child( s );
      return 0; // Callback must return 0
    default:
      nl_error(4, "Bad what value in curl_multi::socket_function()");
  }
  if ( update_flags( s, flags ) ) {
    curl_selectee *P;
    P = new curl_selectee( s, flags, co );
    add_child(P);
  }
  return 0; // Callback must return 0
}

Transaction::Transaction(TransactionStep *F_in, const char *desc_in) {
  F = F_in;
  desc = desc_in;
}

curl_multi_obj::curl_multi_obj() {
  multi = curl_multi::getInstance();
  next_step = 0;
}

void curl_multi_obj::enqueue_transaction(TransactionStep *F_in, const char *desc_in) {
  Transactions.push_back(Transaction(F_in, desc_in));
  if ( ! transaction_started ) dequeue_transaction();
}

void curl_multi_obj::dequeue_transaction() {
  if ( transaction_started ) Transactions.pop_front();
  if ( !Transactions.empty() ) {
    Transaction T = Transactions.front();
    transaction_start(T.desc);
    T.F(this, CURLE_OK);
  }
}

void curl_multi_obj::multi_add(TransactionStep *F_in, const char *desc) {
  CURLMcode rv;
  req_desc = desc;
  next_step = F_in;
  perform_setup();
  rv = multi->multi_add(handle);
  if (rv != CURLM_OK )
    nl_error( 3, "curl_multi_add_handle() failed: %s", curl_multi_strerror(rv) );
}

int curl_multi_obj::take_next_step(CURLcode code) {
  TransactionStep *F;
  nl_assert(next_step != 0 && req_desc != 0);
  perform_cleanup(req_desc, code);
  F = next_step;
  next_step = 0;
  req_desc = 0;
  return F(this, code);
}

void curl_multi_obj::event_loop() {
  multi->event_loop();
}

int curl_multi_obj::socket_action(int fd, int ev_bitmask ) {
  return multi->socket_action(fd, ev_bitmask);
}
