#include "curl_obj.h"

int main( int argc, char **argv ) {
  curl_obj co;
  co.set_log_level( CT_LOG_BODIES );
  co.transaction_start("Get single page");
  co.set_url("http://localhost/"); // die on failure
  co.perform("Description");
  co.transaction_end();
  return 0;
}
