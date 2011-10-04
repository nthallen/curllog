CFLAGS = -Wall -g
CXXFLAGS = -Wall -g
LDFLAGS =
LDLIBS = -L /usr/lib $(shell curl-config --libs) -lxml2 -L /usr/local/lib -l nort
CPPFLAGS = -I /usr/include/libxml2
CTOBJ = curltest.o curl_obj.o curl_form.o
CTMOBJ = curltestmulti.o curl_obj.o curl_form.o curl_multi.o curl_select.o Selectee.o Selector.o Timeout.o

curltestmulti : $(CTMOBJ)
	$(LINK.cc) -o curltestmulti $(CTMOBJ) $(LDLIBS)

curltest : $(CTOBJ)
	$(LINK.cc) -o curltest $(CTOBJ) $(LDLIBS)

xmltest : xmltest.c
