// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <string>
#include <wibble/sys/thread.h>

#ifndef DIVINE_SYSINFO_H
#define DIVINE_SYSINFO_H

namespace divine {
namespace sysinfo {

struct Data;

struct Info { /* singleton */
    Info();

    void start();
    void stop();
    void update();

    std::string architecture();
    int peakVmSize();

    double userTime();
    double systemTime();
    double wallTime();

    static Data *data;
};

struct ResourceLimit : wibble::exception::Generic {
    ResourceLimit( std::string x ) : wibble::exception::Generic( x ) {}
    std::string desc() const throw () { return "Resource exhausted"; }
};

struct ResourceGuard : wibble::sys::Thread {
    uint64_t memory; /* in kB */
    uint64_t time; /* in seconds */
    void *main();
    ResourceGuard();
};

}

std::ostream &operator<<( std::ostream &o, sysinfo::Info i );

}

#endif
