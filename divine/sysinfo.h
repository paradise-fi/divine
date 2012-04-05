// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <string>

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

}

std::ostream &operator<<( std::ostream &o, sysinfo::Info i );

}

#endif
