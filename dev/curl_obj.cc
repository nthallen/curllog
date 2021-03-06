#include <sys/stat.h>
#include <libxml/HTMLtree.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
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
    fp = curl_obj::create_html_log( fname, "Transaction Log" );
    fprintf( fp, "<table id=\"TransIdx\">\n<tr><th>Start Time</th><th>Dur</th>"
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
    fprintf( fp, "</table>\n" );
    curl_obj::close_html_log(fp);
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
    atexit(&Cleanup);
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
  // req_data_log = NULL;
  req_hdr_log = NULL;
  parse_requested = 1;
  parsing = 0;
  parser = NULL;
  method = "GET";
  nl_assert(handle != 0);
  if ( curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &swrite_data) )
    nl_error(3, "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed\n");
  if ( curl_easy_setopt(handle, CURLOPT_WRITEDATA, this ) )
    nl_error(3, "curl_easy_setopt(CURLOPT_WRITEDATA) failed\n");
  if ( curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L ) )
    nl_error(3, "curl_easy_setopt(CURLOPT_SSL_VERIFYPEER) failed\n");
  if ( curl_easy_setopt(handle, CURLOPT_PRIVATE, this ) )
    nl_error(3, "curl_easy_setopt(CURLOPT_PRIVATE) failed\n");
}

curl_obj::~curl_obj() {
  if ( req_url ) free((void *)req_url);
  if ( parser ) {
    if ( parser->myDoc ) {
      xmlFreeDoc(parser->myDoc);
      parser->myDoc = 0;
    }
    htmlFreeParserCtxt(parser);
    parser = 0;
  }
  if (handle != 0) {
    curl_easy_cleanup(handle);
  }
}

/** This is the uninteresting default version that does nothing */
size_t curl_obj::write_data(char *ptr, size_t size, size_t nmemb) {
  return size*nmemb;
}

size_t curl_obj::log_write_data(char *ptr, size_t size, size_t nmemb) {
  size_t rv;
  if ( llvl >= CT_LOG_BODIES || parse_requested ) {
    if ( req_nbytes == 0 ) {
      char *ctype;
      CURLcode cc;
      cc = curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &ctype);
      if ( cc == 0 ) {
        if ( ctype != NULL && strncmp( ctype, "text/html", 9 ) == 0 ) {
          if ( parser ) {
            htmlCtxtReset(parser);
            htmlFreeParserCtxt(parser);
          }
          // Set up parser context
          parser = htmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL, XML_CHAR_ENCODING_NONE);
          nl_assert(parser);
          parsing = 1;
        } else if (ctype == NULL) {
          nl_error( 1, "No Content-type found" );
        } else {
          nl_error( 1, "Unexpected Content-type: '%s'", ctype );
        }
      }
    }
    if ( parsing ) {
      htmlParseChunk(parser, ptr, size*nmemb, 0);
    }
  }
  req_nbytes += size*nmemb;
  rv = write_data(ptr, size, nmemb);
  return rv;
}

size_t curl_obj::swrite_data(char *ptr, size_t size, size_t nmemb, void *userdata) {
  curl_obj *co = (curl_obj *)userdata;
  return co->log_write_data(ptr, size, nmemb);
}

/**
 * This is the CURLOPT_DEBUGFUNCTION non-static invocation.
 */
void curl_obj::debug_info( curl_infotype type, char *s, size_t size ) {
  if ( req_hdr_log ) {
    const char *typestr, *class_str;
    unsigned i;
    switch (type) {
      case CURLINFO_TEXT: typestr = "*"; class_str = "info"; break;
      case CURLINFO_HEADER_OUT: typestr = "&gt;"; class_str = "reqhdr"; break;
      case CURLINFO_HEADER_IN: typestr = "&lt;"; class_str = "resphdr"; break;
      case CURLINFO_DATA_IN: return;
      case CURLINFO_DATA_OUT: return;
      case CURLINFO_SSL_DATA_IN: return;
      case CURLINFO_SSL_DATA_OUT: return;
      default: typestr = "?"; class_str = "unknown"; break;
    }
    for ( i = 0; i < size; ) {
      unsigned j;
      int inlen, outlen, rv;
      unsigned char escaped[256];

      for ( j = i; j < size; ++j ) {
        if ( s[j] == '\r' || s[j] == '\n' ) break;
      }
      inlen = j-i;
      outlen = 255;
      rv = htmlEncodeEntities( escaped, &outlen, (unsigned char *)(s+i), &inlen, 0 );
      if ( rv ) {
        inlen = j-i;
        nl_error( 2, "htmlEncodeEntities() choked on '%*.*s'", inlen, inlen, s+i );
        return;
      }
      escaped[outlen] = '\0';
      fprintf( req_hdr_log, "<tr class=\"%s\"><td>%s</td><td>%s</td></tr>\n",
              class_str, typestr, escaped );
      while ( j < size && ( s[j] == '\r' || s[j] == '\n') ) ++j;
      i = j;
    }
  }
}

/**
 * This is the CURLOPT_DEBUGFUNCTION
 */
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
  if ( curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L) )
    nl_error(3, "FATAL: curl_easy_setopt(CURLOPT_HTTPGET) failed" );
  method = "GET";
}

void curl_obj::set_postfields(const char *text, int tsize ) {
  if (curl_easy_setopt(handle, CURLOPT_POSTFIELDS, text) ||
      curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, tsize ) )
    nl_error(3, "curl_easy_setopt() failed on POSTFIELDS or POSTFIELDSIZE" );
  method = "POST";
}

void curl_obj::perform_setup() {
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
      "<table id=\"Headers\">\n<tr><th>Type</th><th>Text</th></tr>\n" );
  } else if ( curl_easy_setopt(handle, CURLOPT_VERBOSE, 0) ) {
    nl_error( 3, "FATAL: error clearing verbose" );
  }
  req_nbytes = 0;
  req_start = time(NULL);
}

void curl_obj::perform_cleanup(const char *req_desc, CURLcode code) {
  req_end = time(NULL);
  if ( code != 0 )
     nl_error(1, "WARN: curl_easy_perform()/multi op failed\n");
 { long resp_code;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &resp_code);
    req_status = resp_code;
  }
  if (llvl >= CT_LOG_HEADERS) {
    // Close the response header table
    fprintf( req_hdr_log, "</table>\n" );
    req_hdr_log = close_html_log(req_hdr_log);
  }
  if (llvl >= CT_LOG_TRANSACTIONS) {
    // Write request summary to req_summary
    print_times( req_summary, req_start, req_end );
    if ( llvl >= CT_LOG_HEADERS ) {
      fprintf( req_summary,
        "<td><a href=\"req%02d_hdrs.html\">%s</a></td>",
        req_num, method );
    } else {
      fprintf( req_summary, "<td>%s</td>", method );
    }
    if (req_url)
      fprintf( req_summary, "<td><a href=\"%s\">%s</a></td>", req_url, req_url );
    else fprintf( req_summary, "<td></td>" );
    if ( llvl >= CT_LOG_BODIES && parsing ) {
      char fname[80];
      snprintf( fname, 80, "%s/%02d/req%02d_body.html",
        global->trans_dir, trans_num, req_num );
      add_base( xmlDocGetRootElement(parser->myDoc), req_url );
      htmlSaveFile(fname, parser->myDoc);
      fprintf( req_summary, "<td><a href=\"req%02d_body.html\">%s</a></td>", req_num, req_desc );
    } else {
      fprintf( req_summary, "<td>%s</td>", req_desc );
    }
    fprintf( req_summary, "<td>%d</td></tr>\n", req_status ); // Use status of last request
  }
  ++req_num;
}

void curl_obj::perform(const char *req_desc) {
  perform_setup();
  perform_cleanup(req_desc, curl_easy_perform(handle));
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
      req_summary = create_html_log( tdir, "Transaction %d Request Summary", trans_num);
      fprintf( req_summary, "%s",
        "<p>[<a href=\"../index.html\">Transaction Index</a>]</p>\n"
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

FILE *curl_obj::create_html_log( const char *fname, const char *title, ... ) {
  FILE *fp;
  va_list arg;
  char ftitle[80];
  const char *s;

  va_start(arg, title);
  vsnprintf(ftitle, 79, title, arg );
  va_end(arg);
  fp = fopen(fname, "w");
  if ( fp == NULL ) nl_error(3, "FATAL: Unable to open html log '%s'\n", fname );
  fprintf( fp, "%s%s%s",
    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n"
    "  \"http://www.w3.org/TR/html4/strict.dtd\">\n"
    "<html>\n"
    "<head>\n"
    "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
    "  <title>", ftitle, "</title>\n"
    "  <link href=\"" );
  for ( s = fname; *s; ++s ) {
    if ( *s == '/' )
      fprintf( fp, "../" );
  }
  fprintf( fp,
    "curltest.css\" rel=\"stylesheet\" type=\"text/css\">\n"
    "</head>\n"
    "<body>\n" );
  fprintf( fp, "<h1>%s</h1>\n", ftitle );
  return fp;
}

FILE *curl_obj::close_html_log(FILE *fp) {
  fprintf( fp, "%s",
    "</body>\n</html>\n" );
  return NULL;
}

void curl_obj::print_times( FILE *fp, time_t stime, time_t etime ) {
  char tbuf[80];
  double dur = difftime( etime, stime );
  strftime(tbuf, 80, "%H:%M:%S", localtime(&stime) );
  fprintf( fp, "<tr><td>%s</td><td>%.0lf</td>", tbuf, dur );
}

/** Recursively looks for <head> node and adds a <base> element.
  * @return non-zero when the deed is done.
  */
int curl_obj::add_base( xmlNodePtr xp, const char *url ) {
  for ( ; xp != NULL; xp = xp->next ) {
    if ( xp->name != NULL && stricmp((const char *)xp->name, "head") == 0 ) {
      xmlNodePtr bp = xmlNewDocNode( xp->doc, NULL, (const xmlChar *)"base", NULL );
      xmlNewProp(bp, (const xmlChar *)"href", (const xmlChar *)url);
      if ( xp->children ) {
        xmlAddPrevSibling(xp->children, bp);
      } else {
        xmlAddChild(xp, bp);
      }
      return 1;
    }
    if ( add_base( xp->children, url ) )
      return 1;
  }
  return 0;
}

xmlNodePtr curl_obj::get_parse_tree() {
  return parser ? xmlDocGetRootElement(parser->myDoc) : NULL;
}

curl_form *curl_obj::find_form_int( xmlNodePtr xp, int n ) {
  curl_form *rv;
  for ( ; xp != NULL; xp = xp->next ) {
    if ( xp->name != NULL && stricmp((const char *)xp->name, "form") == 0 ) {
      if ( --n <= 0 ) return new curl_form( this, xp );
      // Don't need to go through form's children.
    } else {
      rv = find_form_int( xp->children, n );
      if ( rv ) return rv;
    }
  }
  return NULL;
}

const char *curl_obj::relative_url( const char *href ) {
  // If href begins with a protocol (e.g. http:) then use the whole thing,
  // else get the protocol from base.
  // If href starts with /, then use the rest of it,
  // otherwise copy base up until the last slasy, then copy the rest
  // of href.
  static char *rel_url = 0;
  static int rel_url_size = 0;
  const char *s, *endpt;
  int new_size, base_size;
  
  if (href == 0 || *href == '\0') {
    if ( req_url == 0 || *req_url == '\0' )
      nl_error( 3, "No base URL defined in relative_url" );
    return (const char *)req_url;
  }
  if (isalpha(*href)) {
    for ( s = href; isalpha(*s); ++s );
    if ( *s == ':' ) return href;
  }
  s = req_url;
  endpt = s;
  if (isalpha(*s)) {
    while (isalpha(*s)) ++s;
    if ( *s == ':' )
      endpt = ++s;
  }
  if ( endpt == req_url )
    nl_error(3, "Base URL '%s' has no protocol", req_url);
  if ( href[0] != '/' || href[1] != '/' ) {
    // Need to get host from base URL
    if ( s[0] != '/' || s[1] != '/' )
      nl_error(3, "Base URL '%s' has no host", req_url);
    s += 2;
    if ( *s == '\0' || *s == '/' )
      nl_error(3, "Base URL '%s' has degenerate host", req_url);
    while ( *s != '\0' && *s != '/' ) ++s;
    endpt = s;
  }
  if ( *href != '/' ) {
    while ( *s != '\0' ) {
      if ( *s++ == '/' ) {
        endpt = s;
      }
    }
  }
  // Now I'll take req_url up to but not including endpt and all of href
  base_size = endpt-req_url;
  new_size = base_size + strlen(href) + 1;
  if ( rel_url_size < new_size ) {
    rel_url = (char *)realloc( rel_url, new_size );
    rel_url_size = new_size;
    if ( rel_url == NULL )
      nl_error(4, "Out of memory in relative_url()" );
  }
  strncpy( rel_url, req_url, base_size );
  strcpy( rel_url+base_size, href );
  return (const char *)rel_url;
}

curl_form *curl_obj::find_form(int n) {
  return parser ? find_form_int(xmlDocGetRootElement(parser->myDoc), n) : NULL;
}
