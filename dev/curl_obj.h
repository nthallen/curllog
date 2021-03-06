#ifndef CURL_OBJ_H_INCLUDED
#define CURL_OBJ_H_INCLUDED
//#include <string>
#include <libxml/HTMLparser.h>
#include "curl/curl.h"

class curl_form;
class curl_multi;
class curl_socket;

class curl_global {
  private:
    curl_global();
    ~curl_global();
    static curl_global *single;
    static void Cleanup();
    int next_trans;
    const char *trans_file;
  public:
    static const char *trans_dir;
    static curl_global *getInstance();
    int next_transaction();
};

enum curl_log_level_t { CT_LOG_NOTHING,
  CT_LOG_SUMMARIES, CT_LOG_TRANSACTIONS,
  CT_LOG_HEADERS, CT_LOG_BODIES };

class curl_obj {
  private:
    curl_log_level_t llvl;
    const char *req_url;
    const char *trans_desc;
    int trans_num;
    int req_num;
    int req_status;
    int req_nbytes;
    int parse_requested;
    int parsing;
    time_t trans_start, trans_end;
    time_t req_start, req_end;
    FILE *req_summary;
    FILE *req_hdr_log;
    /** Parsing context when parsing is requested or bodies are being logged.
        If non-zero, it must be freed via htmlFreeParserCtxt().
        Also parser->myDoc if non-zero must be free with xmlFreeDoc().
        I will set these to zero whenever freed.
      */
    htmlParserCtxtPtr parser;
    /** Handle the data in an application-specific way */
    virtual size_t write_data(char *ptr, size_t size, size_t nmemb);
    /** Log the data and then call write_data() */
    size_t log_write_data(char *ptr, size_t size, size_t nmemb);
    /** Calls log_write_data() */
    static size_t swrite_data(char *ptr, size_t size, size_t nmemb, void *userdata);
    /** Method for logging header info */
    void debug_info( curl_infotype type, char *s, size_t size );
    /** C callback for debug header info. Calls debug_info() */
    static int debug_callback(CURL *, curl_infotype, char *, size_t, void *);
    void print_times( FILE *fp, time_t stime, time_t etime );
    int add_base( xmlNodePtr xp, const char *url );
    curl_form *find_form_int( xmlNodePtr xp, int n );
  protected:
    CURL *handle;
    curl_global *global;
    int transaction_started;
    void perform_setup();
    void perform_cleanup(const char *, CURLcode);
  public:
    const char *method;
    curl_obj();
    ~curl_obj();
    void set_log_level(curl_log_level_t lvl);
    void set_url(const char *url);
    void set_postfields(const char *text, int field_size);
    inline void parse_request(int requested) { parse_requested = requested; }
    void perform(const char *req_desc);
    void transaction_start(const char *desc);
    void transaction_end();
    curl_form *find_form(int n);
    xmlNodePtr get_parse_tree();
    const char *relative_url( const char *href );
    static FILE *create_html_log( const char *fname, const char *title, ... );
    static FILE *close_html_log(FILE *fp);
  friend class curl_form;
};

class curl_form {
  private:
    curl_obj *co;
    xmlNodePtr form;
    char *submit_buf;
    int submit_buf_size;
    int submit_size;
    int need_amp;
    void realloc_submit();
    void append_to_submit( const char *text );
    void quote_to_submit( const char *text );
    void append_pair_to_submit( const char *nm, const char *val );
    int set( xmlNodePtr xp, const char *name, const char *value );
    int checkbox(  xmlNodePtr xp, const char *name, const char *value, int checked );
    void submit_int(xmlNodePtr xp);
  public:
    curl_form(curl_obj *co, xmlNodePtr top);
    ~curl_form();
    void set( const char *name, const char *value );
    void checkbox( const char *name, const char *value, int checked );
    void submit_setup( const char *name, const char *value );
    void submit( const char *desc, const char *name, const char *value );
};

#endif
