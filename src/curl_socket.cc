#include "curllog/curl_obj.h"

curl_socket::curl_socket( int fd_in, curl_obj *co_in) {
  fd = fd_in;
  co = co_in;
}

bool operator<( const curl_socket& a, const curl_socket& b ) {
  return a.fd < b.fd;
}
