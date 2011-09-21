CFLAGS = -Wall -g
CXXFLAGS = -Wall -g
LDFLAGS =
LDLIBS = -L /usr/lib $(shell curl-config --libs) -lxml2 -L /usr/local/lib -l nort
CPPFLAGS = -I /usr/include/libxml2

curltest : curltest.o curl_obj.o curl_form.o
	$(LINK.cc) -o curltest curltest.o curl_obj.o curl_form.o $(LDLIBS)
curltest.o : curltest.cc

xmltest : xmltest.c
