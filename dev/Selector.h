/**
 * \file Selector.h
 */
#ifndef SELECTOR_H_INCLUDED
#define SELECTOR_H_INCLUDED

#include <map>
#include <time.h>
#include "Timeout.h"

class Selector;

class Selectee {
  public:
    Selectee(int fd_in, int flag);
    /**
     * @return non-zero if we should quit
     */
    virtual int ProcessData(int flag) = 0;
    virtual Timeout *GetTimeout();

    int fd;
    int flags;
    Selector *Stor;
};

typedef std::map<int,Selectee *> SelecteeMap;

class Selector {
  public:
    static const int Sel_Read = 1;
    static const int Sel_Write = 2;
    static const int Sel_Except = 4;
    static const int Sel_Timeout = 8;
    Selector();
    void add_child(Selectee *P);
    void delete_child(int fd_in);
    void update_flags(int fd_in, int flag);
    void event_loop();
  private:
    SelecteeMap S;
    virtual int ProcessTimeout();
    virtual Timeout *GetTimeout();
};

#endif
