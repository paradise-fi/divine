// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <iomanip>
#include <sstream>

#include <wibble/sys/thread.h>
#include <wibble/regexp.h>
#include <divine/config.h>
#include <divine/version.h>

#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/time.h>

#ifndef DIVINE_REPORT_H
#define DIVINE_REPORT_H

namespace divine {

struct Result
{
    enum R { Yes, No, Unknown };
    R ltlPropertyHolds, fullyExplored;
    uint64_t visited, expanded, deadlocks, errors;
    Result() :
        ltlPropertyHolds( Unknown ), fullyExplored( Unknown ),
        visited( 0 ), expanded( 0 ), deadlocks( 0 ), errors( 0 )
    {}
};

std::ostream &operator<<( std::ostream &o, Result::R v )
{
    return o << (v == Result::Unknown ? "-" :
                 (v == Result::Yes ? "Yes" : "No" ) );
}

struct Report : wibble::sys::Thread
{
    Config &config;
    struct timeval tv;
    struct rusage usage;
    Result res;

    int sig;
    bool m_finished;

    Report( Config &c ) : config( c ),
                          sig( 0 ),
                          m_finished( false )
    {
        gettimeofday(&tv, NULL);
        // getrusage( RUSAGE_SELF, &usage );
    }

    void *main() {
        if ( !config.report() )
            return 0;
        while ( true ) {
            sleep( 1 );
            if ( m_finished )
                return 0;
        }
    }

    void signal( int s ) 
    {
        m_finished = false;
        sig = s;
    }

    void finished( Result r ) 
    {
        m_finished = true;
        res = r;
    }

    struct timeval zeroTime() {
        struct timeval r = { 0, 0 };
        return r;
    }

    double userTime() {
        return interval( zeroTime(), usage.ru_utime );
    }

    double systemTime() {
        return interval( zeroTime(), usage.ru_stime );
    }

    double interval( struct timeval from, struct timeval to ) {
        return to.tv_sec - from.tv_sec +
            double(to.tv_usec - from.tv_usec) / 1000000;
    }

    int vmSize() {
        int vmsz = 0;
        std::stringstream file;
        file << "/proc/" << uint64_t( getpid() ) << "/status";
        wibble::ERegexp r( "VmPeak:[\t ]*([0-9]+)", 2 );
        if ( matchLine( file.str(), r ) ) {
            vmsz = atoi( r[1].c_str() );
        }
        return vmsz;
    }

    std::string architecture() {
        wibble::ERegexp r( "model name[\t ]*: (.+)", 2 );
        if ( matchLine( "/proc/cpuinfo", r ) )
            return r[1];
        return "";
    }

    bool matchLine( std::string file, wibble::ERegexp &r ) {
        std::string line;
        std::ifstream f( file.c_str() );
        while ( !f.eof() ) {
            std::getline( f, line );
            if ( r.match( line ) )
                return true;
        }
        return false;
    }

    void final( std::ostream &o ) {
        if ( !config.report() )
            return;

        struct timeval now;
        gettimeofday(&now, NULL);
        getrusage( RUSAGE_SELF, &usage );

        config.dump( o );
        o << "User-Time: " << userTime() << std::endl;
        o << "System-Time: " << systemTime() << std::endl;
        o << "Wall-Time: " << interval( tv, now ) << std::endl;
        o << "Termination-Signal: " << sig << std::endl;
        o << "Memory-Used: " << vmSize() << std::endl;
        o << "Finished: " << (m_finished ? "Yes" : "No") << std::endl;
        o << "Full-State-Space: " << res.fullyExplored << std::endl;
        o << "LTL-Property-Holds: " << res.ltlPropertyHolds << std::endl;
        o << "Deadlock-Count: " << res.deadlocks << std::endl;
        o << "Error-State-Count: " << res.errors << std::endl;
        o << "States-Visited: " << res.visited << std::endl;
        o << "State-Expansions: " << res.expanded << std::endl;
        o << "Pointer-Width: " << 8 * sizeof( void* ) << std::endl;
        o << "Version: " << DIVINE_VERSION << std::endl;
        o << "Build-Date: " << DIVINE_BUILD_DATE << std::endl;
        o << "Architecture: " << architecture() << std::endl;
    }
};

}

#endif
