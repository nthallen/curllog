#ifndef CURL_OBJ_H_INCLUDED
#define CURL_OBJ_H_INCLUDED
//#include <string>
#include "curl/curl.h"

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
    CURL *handle;
    curl_global *global;
    curl_log_level_t llvl;
    const char *trans_desc;
    int trans_num;
    int req_num;
    int req_status;
    int transaction_started;
    time_t trans_start, trans_end;
    time_t req_start, req_end;
    FILE *req_summary;
    FILE *req_data_log;
    FILE *req_hdr_log;
    /** Handle the data in an application-specific way */
    virtual size_t write_data(char *ptr, size_t size, size_t nmemb);
    /** Log the data and then call write_data() */
    size_t log_write_data(char *ptr, size_t size, size_t nmemb);
    /** Calls log_write_data() */
    static size_t swrite_data(char *ptr, size_t size, size_t nmemb, void *userdata);
    /** Log the header */
    virtual size_t write_header(char *ptr, size_t size, size_t nmemb);
    /** Parse and log the header and then call write_header() */
    size_t log_write_header(char *ptr, size_t size, size_t nmemb);
    /** Calls log_header() */
    static size_t swrite_header(char *ptr, size_t size, size_t nmemb, void *userdata);
    FILE *create_html_log( const char *fname, const char *title );
    FILE *close_html_log(FILE *fp);
    void print_times( FILE *fp, time_t stime, time_t etime );
  public:
    curl_obj();
    ~curl_obj();
    void set_log_level(curl_log_level_t lvl);
    void set_url(const char *url);
    void perform(const char *req_desc);
    void transaction_start(const char *desc);
    void transaction_end();
};

#endif
