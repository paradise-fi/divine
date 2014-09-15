// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <string>
#include <wibble/sys/thread.h>
#include <divine/utility/withreport.h>

#ifndef DIVINE_SYSINFO_H
#define DIVINE_SYSINFO_H

namespace divine {
namespace sysinfo {

struct Data;

struct Info : WithReport { /* singleton */
    Info();

    static void start();
    void stop() const;
    void update() const ;

    std::string architecture() const;
    uint64_t peakVmSize() const;

    double userTime() const;
    double systemTime() const;
    double wallTime() const;

    static Data *data;
    static void init();

    std::vector< ReportLine > report() const override;
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

}

#endif
