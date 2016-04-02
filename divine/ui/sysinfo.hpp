// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <string>
#include <stdexcept>
#include <divine/utility/withreport.h>

#include <bricks/brick-shmem>

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
    uint64_t vmSize() const;
    uint64_t peakResidentMemSize() const;
    uint64_t residentMemSize() const;

    double userTime() const;
    double systemTime() const;
    double wallTime() const;

    static Data *data;
    static void init();

    std::vector< ReportLine > report() const override;
};

struct ResourceLimit : std::runtime_error {
    ResourceLimit( std::string x ) : std::runtime_error( "Resource exhausted: " + x ) {}
};

struct ResourceGuard : brick::shmem::Thread {
    uint64_t memory; /* in kB */
    uint64_t time; /* in seconds */
    void main();
    ResourceGuard();
};

}

}

#endif
