#include "curl_select.h"
#include "nl_assert.h"

#define HOST "http://localhost"
// #define HOST "https://act.arlington.ma.us"

class MySchedule;
typedef int (MySchedule::*MyScheduleReq)(CURLcode);

class MySchedule : public Transaction {
  public:
    MySchedule(curl_multi_obj *co, const char *trans_desc);
    int take_next_step(CURLcode);
  private:
    MyScheduleReq next_step;
    int Req0(CURLcode);
    int Req1(CURLcode);
    int Req2(CURLcode);
};

int main( int argc, char **argv ) {
  curl_multi_obj co;
  // curl_form *form;
  co.set_log_level( CT_LOG_BODIES );
  co.enqueue_transaction(new MySchedule( &co, "Get a form and submit it" ));
  co.event_loop();
  co.transaction_end();
  return 0;
}

MySchedule::MySchedule(curl_multi_obj *co_in, const char *trans_desc)
    : Transaction(co_in, trans_desc ) {
  next_step = &MySchedule::Req0;
}

int MySchedule::take_next_step(CURLcode code) {
  return (this->*next_step)(code);
}

int MySchedule::Req0(CURLcode code ) {
  nl_assert( code == 0 );
  co->set_url( HOST "/cgi-bin/ACTdb/MySchedule?Proj_ID=330");
  next_step = &MySchedule::Req1;
  co->multi_add( "Get empty form" );  
  return 0;
}

int MySchedule::Req1(CURLcode code ) {
  if ( code == 0 ) {
    curl_form *form;
    form = co->find_form(1);
    if ( form ) {
      form->checkbox( "Chars", "3137", 1 );
      form->submit_setup("_submit", "My Schedule");
      next_step = &MySchedule::Req2;
      co->multi_add("Schedule for Celeb: Fan");
      return 0;
    }
  }
  co->dequeue_transaction();
  return 1;
}

int MySchedule::Req2(CURLcode code ) {
  co->dequeue_transaction();
  return 1;
}
