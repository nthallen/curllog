/**
 * \file curl_select.h
 */
#ifndef CURL_SELECT_H_INCLUDED
#define CURL_SELECT_H_INCLUDED

#include <deque>
#include "curl_obj.h"
#include "Selector.h"

class curl_multi_obj;

class curl_multi : public Selector {
  public:
    curl_multi();
    ~curl_multi();
    int socket_action(int fd, int ev_bitmask );
    inline CURLMcode multi_add(CURL *handle) { return curl_multi_add_handle(multi, handle); }
    static curl_multi *getInstance();
    static int static_socket_function(CURL *easy, curl_socket_t s,
                  int what, void *userp, void *socketp);
    static int static_timer_function(CURLM *multi, long timeout_ms, void *userp);
  protected:
    CURLM *multi;
    long timeout_msec;
  private:
    int running_handles;
    static curl_multi *single;
    Timeout to;
    int ProcessTimeout();
    Timeout *GetTimeout();
    int socket_function(CURL *easy, curl_socket_t s,
                  int what, void *socketp);
    static void Cleanup();
};

// Need to subclass Selectee for curl sockets
class curl_selectee : public Selectee {
  public:
    curl_selectee(int fd, int flags, curl_multi_obj *co_in);
    int ProcessData(int flag);
    
    curl_multi_obj *co;
};

class Transaction {
  public:
    curl_multi_obj *co;
    const char *desc;
    Transaction(curl_multi_obj*,const char *);
    virtual ~Transaction();
    virtual int take_next_step(CURLcode) = 0;
};

class curl_multi_obj : public curl_obj {
  public:
    curl_multi_obj();
    void enqueue_transaction(Transaction *);
    void dequeue_transaction();
    void multi_add(const char *);
    int take_next_step(CURLcode);
    int socket_action(int fd, int ev_bitmask );
    void event_loop();
  private:
    curl_multi *multi;
    const char *req_desc;
    std::deque<Transaction*> Transactions;
};

#endif
