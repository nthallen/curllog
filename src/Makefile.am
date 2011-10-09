lib_LTLIBRARIES = libcurllog.la
libcurllog_la_LDFLAGS = -version-info 0:0:0 $(XML2LIBS) $(CURLLIBS)
libcurllog_la_SOURCES = Selectee.cc Selector.cc Timeout.cc \
                  curl_form.cc curl_multi.cc curl_obj.cc \
                  curl_select.cc
nobase_include_HEADERS = curllog/Selector.h curllog/Timeout.h \
                  curllog/curl_obj.h curllog/curl_select.h
AM_CXXFLAGS = $(XML2CFLAGS)
