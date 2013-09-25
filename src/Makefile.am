lib_LTLIBRARIES = libcurllog.la

if HAVE_TMPP
SEL_SRCS =
SEL_INC =
SEL_CPPFLAGS =
else
SEL_SRCS = Selectee.cc Selector.cc Timeout.cc
SEL_INC = Selector/Selector.h Selector/Timeout.h
SEL_CPPFLAGS = -I Selector
endif

libcurllog_la_LDFLAGS = -version-info 0:0:0 $(XML2LIBS) $(CURLLIBS)
libcurllog_la_SOURCES = $(SEL_SRCS) \
                  curl_form.cc curl_multi.cc curl_obj.cc \
                  curl_select.cc
libcurllog_ladir = $(datadir)/huarp
libcurllog_la_DATA = curllog.css
includedir = ${prefix}/include/curllog
include_HEADERS = $(SEL_INC) \
                  curllog/curl_obj.h curllog/curl_select.h
AM_CXXFLAGS = $(XML2CFLAGS)
AM_CPPFLAGS = -DCSS_SRC_DIR=$(libcurllog_ladir) $(SEL_CPPFLAGS)

