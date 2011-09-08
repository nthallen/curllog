#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "curl_obj.h"
#include "nl_assert.h"

const char *curl_global::trans_dir = "trans";
curl_global *curl_global::single;

curl_global::curl_global() {
  struct stat sbuf;
  char tbuf[80];
  FILE *fp;
  curl_global_init(CURL_GLOBAL_SSL);
  atexit(&Cleanup);
  snprintf(tbuf,80,"%s/.next", trans_dir);
  trans_file = strdup(tbuf);
  if ( stat( trans_dir, &sbuf ) == 0 ) {
    if ( ! S_ISDIR(sbuf.st_mode) ) {
      nl_error( 3, "FATAL: Cannot create transaction directory %s\n", trans_dir );
    }
  } else if ( mkdir( trans_dir, 0666 ) == -1 ) {
    nl_error( 3, "FATAL: Error creating transaction directory %s: %s\n",
        trans_dir, strerror(errno) );
  }
  fp = fopen( trans_file, "r" );
  if ( fp != 0 ) {
    char buf[80];
    if ( fgets( buf, 80, fp ) && isdigit(buf[0]) ) {
      next_trans = atoi(buf);
    } else {
      nl_error(3, "FATAL: Error reading next transation file %s\n", trans_file );
    }
    fclose(fp);
  } else {
    next_trans = 0;
  }
}

curl_global::~curl_global() {
  if ( next_trans > 0 ) {
    FILE *fp, *ifp;
    char fname[80];
    snprintf(fname, 80, "%s/index.html", trans_dir);
    fp = fopen(fname, "w");
    if ( fp == NULL ) nl_error(3, "FATAL: Unable to open index '%s'\n", fname );
    fprintf( fp, "%s",
      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n"
      "  \"http://www.w3.org/TR/html4/strict.dtd\">\n"
      "<HTML>\n"
      "<HEAD>\n"
      "  <TITLE>Transaction Log</TITLE>\n"
      "</HEAD>\n"
      "<BODY>\n" );
    fprintf( fp, "<h1>Transaction Log</h1>\n" );
    fprintf( fp, "<table>\n<tr><th>Start Time</th><th>Dur</th>"
        "<th>Transaction</th><th>Status</th></tr>\n" );
    while ( --next_trans >= 0 ) {
      snprintf( fname, 80, "%s/%02d/summary.txt", trans_dir, next_trans );
      ifp = fopen( fname, "r" );
      if ( ifp == NULL ) {
        snprintf( fname, 80, "%s/%02d.summary.txt", trans_dir, next_trans );
        ifp = fopen( fname, "r" );
      }
      if ( ifp != NULL ) {
        for (;;) {
          char buf[256];
          int n = fread( buf, 1, 256, ifp );
          if ( n > 0 ) {
            fwrite( buf, 1, n, fp );
          } else break;
        }
        fclose(ifp);
      } else {
        fprintf(fp, "<tr><td colspan=\"4\">Missing</td></tr>\n" );
      }
    }
  }
  curl_global_cleanup();
}

void curl_global::Cleanup() {
  delete single;
  single = 0;
}

curl_global *curl_global::getInstance() {
  if (single == 0) {
    single = new curl_global();
  }
  return single;
}

int curl_global::next_transaction() {
  int this_trans = next_trans++;
  FILE *fp = fopen( trans_file, "w" );
  if ( fp ) {
    fprintf( fp, "%d\n", next_trans );
    fclose(fp);
  } else {
    nl_error( 3, "FATAL: Error opening %s for write: %s\n", trans_file, strerror(errno) );
  }
  return this_trans;
}

curl_obj::curl_obj() {
  global = curl_global::getInstance();
  handle = curl_easy_init();
  llvl = CT_LOG_SUMMARIES;
  transaction_started = 0;
  trans_desc = NULL;
  req_num = 0;
  req_status = 0;
  req_url = NULL;
  req_summary = NULL;
  req_data_log = NULL;
  req_hdr_log = NULL;
  nl_assert(handle != 0);
  if ( curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &swrite_data) )
    nl_error(3, "curl_easy_setup(CURLOPT_WRITEFUNCTION) failed\n");
  if ( curl_easy_setopt(handle, CURLOPT_WRITEDATA, this ) )
    nl_error(3, "curl_easy_setup(CURLOPT_WRITEDATA) failed\n");
  //if ( curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, &swrite_header) )
  //  nl_error(3, "curl_easy_setup(CURLOPT_HEADERFUNCTION) failed\n");
  //if ( curl_easy_setopt(handle, CURLOPT_HEADERDATA, this ) )
  //  nl_error(3, "curl_easy_setup(CURLOPT_HEADERDATA) failed\n");
}

curl_obj::~curl_obj() {
  if ( req_url ) free((void *)req_url);
  if (handle != 0) {
    curl_easy_cleanup(handle);
  }
}

/** This is the uninteresting default version that does nothing */
size_t curl_obj::write_data(char *ptr, size_t size, size_t nmemb) {
  return size*nmemb;
}

size_t curl_obj::log_write_data(char *ptr, size_t size, size_t nmemb) {
  size_t rv = write_data(ptr, size, nmemb);
  if ( llvl >= CT_LOG_BODIES ) {
    fwrite( ptr, size, nmemb, req_data_log );
  }
  return rv;
}

size_t curl_obj::swrite_data(char *ptr, size_t size, size_t nmemb, void *userdata) {
  curl_obj *co = (curl_obj *)userdata;
  return co->log_write_data(ptr, size, nmemb);
}

/** virtual function if app needs to see headers */
size_t curl_obj::write_header(char *ptr, size_t size, size_t nmemb) {
  return size*nmemb;
}

/** Parses the headers looking for status, maybe more, and logs headers as necessary */
size_t curl_obj::log_write_header(char *ptr, size_t size, size_t nmemb) {
  size_t len = size*nmemb;
  size_t colon;
  const char *hdr, *val;
  size_t hdrlen, vallen;
  while ( len > 0 && isspace(ptr[len-1]) ) --len;
  for ( colon = 0; colon < len && ptr[colon] != ':'; ++colon ) ;
  if ( colon == len ) {
    // could be a blank line or 'HTTP/[\d.]+ \d+ .*'
    if ( len > 0 ) {
      if ( strncmp(ptr, "HTTP/", 5) == 0 ) {
        for (colon = 5; colon < len && !isspace(ptr[colon]); ++colon);
        hdr = ptr;
        hdrlen = colon;
        if ( ! isspace(ptr[colon]) )
          nl_error(1, "Expected space after HTTP/" );
        while (isspace(ptr[colon]) && colon < len) ++colon;
        val = ptr+colon;
        vallen = len - colon;
        if ( colon < len && isdigit(ptr[colon]) ) {
          req_status = atoi(val);
        } else {
          nl_error(1, "Expected digit after HTTP/" );
        }
      } else {
        nl_error(1, "Unrecognized header: %*.*s", len, len, ptr);
        hdr = "";
        hdrlen = 0;
        val = ptr;
        vallen = len;
      }
    }
  } else {
    hdr = ptr;
    hdrlen = colon++;
    while (colon < len && isspace(ptr[colon]) ) ++colon;
    val = ptr+colon;
    vallen = len - colon;
  }
  if ( len > 0 && llvl >= CT_LOG_HEADERS ) {
    fprintf( req_hdr_log, "<tr><td>%*.*s</td><td>%*.*s</td></tr>\n",
      hdrlen, hdrlen, hdr, vallen, vallen, val );
  }
  return write_header(ptr, size, nmemb);
}

size_t curl_obj::swrite_header(char *ptr, size_t size, size_t nmemb, void *userdata) {
  curl_obj *co = (curl_obj *)userdata;
  return co->log_write_header(ptr, size, nmemb);
}

void curl_obj::debug_info( curl_infotype type, char *s, size_t size ) {
  if ( req_hdr_log ) {
    const char *typestr;
    switch (type) {
      case CURLINFO_TEXT: typestr = "*"; break;
      case CURLINFO_HEADER_OUT: typestr = "&gt;"; break;
      case CURLINFO_HEADER_IN: typestr = "&lt;"; break;
      case CURLINFO_DATA_IN: return;
      case CURLINFO_DATA_OUT: return;
      default: typestr = "?"; break;
    }
    fprintf( req_hdr_log, "<tr><td>%s</td><td><pre>", typestr );
    fwrite( s, 1, size, req_hdr_log );
    fprintf( req_hdr_log, "</pre></td></tr>\n" );
    fflush( req_hdr_log );
  }
}

int curl_obj::debug_callback(CURL *handle, curl_infotype type, char *str, size_t size, void *userdata) {
  curl_obj *co = (curl_obj *)userdata;
  co->debug_info(type, str, size);
  return 0;
}

void curl_obj::set_log_level(curl_log_level_t lvl) {
  nl_assert(!transaction_started);
  llvl = lvl;
}

void curl_obj::set_url(const char *url) {
  if ( curl_easy_setopt(handle, CURLOPT_URL, url) )
    nl_error(3, "FATAL: curl_easy_setopt(CURLOPT_URL, '%s') failed", url);
  if ( req_url ) {
    free((void*)req_url);
  }
  req_url = strdup(url);
}

void curl_obj::perform(const char *req_desc) {
  if (llvl >= CT_LOG_HEADERS) {
    char fname[80];
    char title[80];
    if ( curl_easy_setopt( handle, CURLOPT_DEBUGFUNCTION, &debug_callback ) ||
         curl_easy_setopt( handle, CURLOPT_DEBUGDATA, this ) ||
         curl_easy_setopt( handle, CURLOPT_VERBOSE, 1 ) )
      nl_error( 3, "FATAL: curl_easy_setopt(CURLOPT_DEBUGFUNCTION or _VERBOSE) failed" );
    snprintf( fname, 80, "%s/%02d/req%02d_hdrs.html",
      global->trans_dir, trans_num, req_num );
    snprintf( title, 80, "Request Headers %02d/%02d", trans_num, req_num );
    req_hdr_log = create_html_log( fname, title );
    fprintf( req_hdr_log,
      "<p>[<a href=\"../index.html\">index</a>] "
      "[<a href=\"index.html\">transaction</a>] ");
    if ( llvl >= CT_LOG_BODIES ) {
      fprintf( req_hdr_log, 
        "[<a href=\"req%02d_body.html\">body</a>]</p>\n",
        req_num );
    } else {
      fprintf( req_hdr_log, "[body]</p>\n" );
    }
    fprintf( req_hdr_log,
      "<table>\n<tr><th>Type</th><th>Text</th></tr>\n" );
    fflush( req_hdr_log );
  } else if ( curl_easy_setopt(handle, CURLOPT_VERBOSE, 0) ) {
    nl_error( 3, "FATAL: error clearing verbose" );
  }
  if (llvl >= CT_LOG_BODIES) {
    char fname[80];
    snprintf( fname, 80, "%s/%02d/req%02d_body.html",
      global->trans_dir, trans_num, req_num );
    req_data_log = fopen( fname, "w" );
    nl_assert( req_data_log != NULL );
  }
  req_start = time(NULL);
  if ( curl_easy_perform(handle) )
    nl_error(3, "FATAL: curl_easy_perform() failed\n");
  req_end = time(NULL);
  if (llvl >= CT_LOG_HEADERS) {
    // Close the response header table
    fprintf( req_hdr_log, "</table>\n" );
    req_hdr_log = close_html_log(req_hdr_log);
  }
  if (llvl >= CT_LOG_BODIES) {
    fclose( req_data_log );
  }
  if (llvl >= CT_LOG_TRANSACTIONS) {
    const char *req_type = "GET";
    // Write request summary to req_summary
    print_times( req_summary, req_start, req_end );
    if ( llvl >= CT_LOG_HEADERS ) {
      fprintf( req_summary,
        "<td><a href=\"req%02d_hdrs.html\">%s</a></td>",
        req_num, req_type );
    } else {
      fprintf( req_summary, "<td>%s</td>", req_type );
    }
    if (req_url)
      fprintf( req_summary, "<td><a href=\"%s\">%s</a></td>", req_url, req_url );
    else fprintf( req_summary, "<td></td>" );
    if ( llvl >= CT_LOG_BODIES )
      fprintf( req_summary, "<td><a href=\"req%02d_body.html\">%s</a></td>", req_num, req_desc );
    else
      fprintf( req_summary, "<td>%s</td>", req_desc );
    fprintf( req_summary, "<td>%d</td></tr>\n", req_status ); // Use status of last request
  }
  ++req_num;
}

void curl_obj::transaction_start(const char *desc) {
  nl_assert(transaction_started == 0);
  if ( llvl >= CT_LOG_SUMMARIES ) {
    trans_num = global->next_transaction();
    if ( llvl >= CT_LOG_TRANSACTIONS ) {
      // Create the transaction directory
      char tdir[80];
      int n = snprintf(tdir, 80, "%s/%02d", global->trans_dir, trans_num );
      nl_assert( n < 80 );
      if (mkdir( tdir, 0666) == -1 )
        nl_error(3, "FATAL: Error creating transaction directory %s\n", tdir);
      snprintf(tdir+n, 80-n, "/index.html" );
      req_summary = create_html_log( tdir, "Transaction Request Summary");
      fprintf( req_summary, "%s",
        "<table>\n<tr><th>Start</th><th>Dur</th>"
        "<th>Method</th><th>URL</th><th>Description</th><th>status</th></tr>\n" );
    }
  }
  trans_desc = strdup(desc);
  transaction_started = 1;
  req_num = 0;
  trans_start = time(NULL);
}

void curl_obj::transaction_end() {
  trans_end = time(NULL);
  if ( llvl >= CT_LOG_TRANSACTIONS ) {
    fprintf( req_summary, "</table>\n" );
    req_summary = close_html_log(req_summary);
  }
  if ( llvl >= CT_LOG_SUMMARIES ) {
    FILE *fp;
    char fname[80];
    snprintf( fname, 80, "%s/%02d%csummary.txt", global->trans_dir, trans_num,
      (llvl > CT_LOG_SUMMARIES) ? '/' : '.');
    fp = fopen( fname, "w" );
    nl_assert(fp != NULL);
    print_times( fp, trans_start, trans_end );
    if ( llvl >= CT_LOG_TRANSACTIONS )
      fprintf( fp, "<td><a href=\"%02d/index.html\">%s</a></td>", trans_num, trans_desc );
    else
      fprintf( fp, "<td>%s</td>", trans_desc );
    fprintf( fp, "<td>%d</td></tr>\n", req_status ); // Use status of last request
    fclose(fp);
  }
  transaction_started = 0;
}

FILE *curl_obj::create_html_log( const char *fname, const char *title ) {
  FILE *fp = fopen(fname, "w");
  if ( fp == NULL ) nl_error(3, "FATAL: Unable to open html log '%s'\n", fname );
  fprintf( fp, "%s%s%s",
    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n"
    "  \"http://www.w3.org/TR/html4/strict.dtd\">\n"
    "<HTML>\n"
    "<HEAD>\n"
    "  <TITLE>", title, "</TITLE>\n"
    "</HEAD>\n"
    "<BODY>\n" );
  fprintf( fp, "<h1>%s</h1>\n", title );
  return fp;
}

FILE *curl_obj::close_html_log(FILE *fp) {
  fprintf( fp, "%s",
    "</BODY>\n</HTML>\n" );
  return NULL;
}

void curl_obj::print_times( FILE *fp, time_t stime, time_t etime ) {
  char tbuf[80];
  double dur = difftime( etime, stime );
  strftime(tbuf, 80, "%H:%M:%S", localtime(&stime) );
  fprintf( fp, "<tr><td>%s</td><td>%.0lf</td>", tbuf, dur );
}