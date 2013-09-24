#include "config.h"
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
  #ifdef CURLMOPT_TIMERFUNCTION
    if ( curl_multi_setopt(multi, CURLMOPT_TIMERFUNCTION, &static_timer_function) != CURLM_OK )
      nl_error( 3, "CURLMOPT_TIMERFUNCTION failed" );
    if ( curl_multi_setopt(multi, CURLMOPT_TIMERDATA, this) != CURLM_OK )
      nl_error( 3, "CURLMOPT_TIMERDATA failed" );
  #endif
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

#ifdef CURLMOPT_TIMERFUNCTION
  int curl_multi::static_timer_function(CURLM *multi, long timeout_ms, void *userp) {
    curl_multi *mt = (curl_multi *)userp;
    mt->timeout_msec = timeout_ms;
    return 0;
  }
#endif

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
  #ifndef CURLMOPT_TIMERFUNCTION
    if ( curl_multi_timeout(multi, &timeout_msec) != CURLM_OK )
      nl_error(4, "Unexpected error from curl_multi_timeout()" );
  #endif
  
  struct timespec now;
  int whole_secs;

  rv = clock_gettime(CLOCK_REALTIME, &now);
  if ( rv == -1 )
    nl_error(3, "Error from clock_gettime(): '%s'", strerror(errno) );
  }
  if (timeout_msec < 0) {
    now.tv_sec += 6;
  } else {
    now.tv_nsec += timeout_msec*1000000L;
    whole_secs = now.tv_nsec/1000000000L;
    now.tv_sec += whole_secs;
    now.tv_nsec -= whole_secs*1000000000L;
  }
  to.Set(now.tv_sec, now.tv_nsec/1000000L);
  return &to;
}

int curl_multi::socket_action(int fd, int ev_bitmask ) {
  CURLMcode rv;
  int ready_to_quit = 0;
  int new_running_handles = running_handles;
  #if HAVE_CURL_MULTI_SOCKET_ACTION
    rv = curl_multi_socket_action( multi, fd, ev_bitmask, &new_running_handles );
    if ( rv != CURLM_OK )
      nl_error( 4, "Error %d (%s) from curl_multi_socket_action()", rv, curl_multi_strerror(rv) );
  #else
    do {
      rv = curl_multi_perform( multi, &new_running_handles );
      if ( rv != CURLM_OK && rv != CURLM_CALL_MULTI_PERFORM)
	nl_error( 4, "Error %d (%s) from curl_multi_socket_action()", rv, curl_multi_strerror(rv ) );
    } while (rv == CURLM_CALL_MULTI_PERFORM);
  #endif
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

Transaction::Transaction(curl_multi_obj *co_in, const char *desc_in) {
  co = co_in;
  desc = desc_in;
}

Transaction::~Transaction() {}

curl_multi_obj::curl_multi_obj() {
  multi = curl_multi::getInstance();
}

void curl_multi_obj::enqueue_transaction(Transaction *T) {
  Transactions.push_back(T);
  if ( ! transaction_started ) dequeue_transaction();
}

void curl_multi_obj::dequeue_transaction() {
  if ( transaction_started ) {
    transaction_end();
    delete Transactions.front();
    Transactions.pop_front();
  }
  if ( !Transactions.empty() ) {
    Transaction *T = Transactions.front();
    transaction_start(T->desc);
    T->take_next_step(CURLE_OK);
    // T->F(this, CURLE_OK, T); // Or something!
  }
}

void curl_multi_obj::multi_add(const char *desc) {
  CURLMcode rv;
  req_desc = desc;
  perform_setup();
  rv = multi->multi_add(handle);
  if (rv != CURLM_OK )
    nl_error( 3, "curl_multi_add_handle() failed: %s", curl_multi_strerror(rv) );
}

int curl_multi_obj::take_next_step(CURLcode code) {
  nl_assert(req_desc != 0);
  perform_cleanup(req_desc, code);
  req_desc = 0;
  return Transactions.front()->take_next_step(code);
}

void curl_multi_obj::event_loop() {
  multi->event_loop();
}

int curl_multi_obj::socket_action(int fd, int ev_bitmask ) {
  return multi->socket_action(fd, ev_bitmask);
}
