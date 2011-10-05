#include "curl_select.h"
#include "nl_assert.h"

#define HOST "http://localhost"
// #define HOST "https://act.arlington.ma.us"

TransactionStep Req0, Req1, Req2;

int main( int argc, char **argv ) {
  curl_multi_obj co;
  // curl_form *form;
  co.set_log_level( CT_LOG_BODIES );
  co.enqueue_transaction(&Req0, "Get a form and submit it");
  co.event_loop();
  co.transaction_end();
  return 0;
}

int Req0(curl_multi_obj *co, CURLcode code ) {
  nl_assert( code == 0 );
  co->set_url( HOST "/cgi-bin/ACTdb/MySchedule?Proj_ID=330");
  co->multi_add( &Req1, "Get empty form" );  
  return 0;
}

int Req1(curl_multi_obj *co, CURLcode code ) {
  if ( code == 0 ) {
    curl_form *form;
    form = co->find_form(1);
    if ( form ) {
      form->checkbox( "Chars", "3137", 1 );
      form->submit_setup("_submit", "My Schedule");
      co->multi_add(&Req2, "Schedule for Celeb: Fan");
      return 0;
    }
  }
  co->dequeue_transaction();
  return 1;
}

int Req2(curl_multi_obj *co, CURLcode code ) {
  co->dequeue_transaction();
  return 1;
}
