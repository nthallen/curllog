#include "curl_obj.h"

int main( int argc, char **argv ) {
  curl_obj co;
  curl_form *form;
  co.set_log_level( CT_LOG_BODIES );
  co.transaction_start("Get a form and submit it");
  co.set_url("https://act.arlington.ma.us/cgi-bin/ACTdb/MySchedule?Proj_ID=330");
  co.perform("Description");
  // Now I want to find the form, identify the action and method and all the input fields
  // I then want to modify one or two and submit the form
  form = co.find_form(1);
  if ( form ) {
    form->checkbox( "Chars", "3137", 1 );
    form->submit("Schedule for Celeb: Fan", "_submit", "My Schedule");
  }
  co.transaction_end();
  return 0;
}
