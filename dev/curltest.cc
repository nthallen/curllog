#include "curl_obj.h"

int main( int argc, char **argv ) {
  curl_obj co;
  curl_form *form;
  co.set_log_level( CT_LOG_BODIES );
  co.transaction_start("Get a form and submit it");
  co.set_url("http://localhost/cgi-bin/ACTdb/Login?nxtPI=%2FAdmin");
  co.perform("Description");
  // Now I want to find the form, identify the action and method and all the input fields
  // I then want to modify one or two and submit the form
  form = co.find_form(1);
  if ( form ) {
    form->set( "Username", "" );
    form->set( "Password", "" );
    form->submit("Login to database", "_submit", "Login");
  }
  co.transaction_end();
  return 0;
}
