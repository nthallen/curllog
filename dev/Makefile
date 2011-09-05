CFLAGS = -Wall -g
CXXFLAGS = -Wall -g
LDFLAGS =
LDLIBS = -L /usr/lib $(shell curl-config --libs) -L /usr/local/lib -l nort

curltest : curltest.o curl_obj.o
	$(LINK.cc) -o curltest curltest.o curl_obj.o $(LDLIBS)
curltest.o : curltest.cc


