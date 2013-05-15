// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <iomanip>
#include <wibble/string.h>

#include <divine/toolkit/hashset.h>
#include <divine/utility/statistics.h>
#include <divine/utility/output.h>

#ifdef POSIX
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace divine {

NoStatistics NoStatistics::_global;

#ifdef POSIX
void TrackStatistics::busy( int id ) {
    struct rusage usage;
    if ( !getrusage( RUSAGE_THREAD, &usage ) ) {
        thread( id ).cputime = usage.ru_utime.tv_sec +
                               usage.ru_utime.tv_usec / 1000000;
    }
}
#else
void TrackStatistics::busy( int id ) {}
#endif

void TrackStatistics::matrix( std::ostream &o, int (*what)(int, int) ) {
    for ( int i = 0; size_t( i ) < threads.size(); ++i ) {
        int sum = 0;
        for ( int j = 0; size_t( j ) < threads.size(); ++j )
            printv( o, 9, what( thread( i ).sent[ j ], thread( j ).received[ i ] ), &sum );
        if ( !gnuplot )
            printv( o, 10, sum, 0 );
        o << std::endl;
    }
}

void TrackStatistics::printv( std::ostream &o, int width, int v, int *sum ) {
    o << " " << std::setw( width ) << v;
    if ( sum )
        *sum += v;
}

void TrackStatistics::label( std::ostream &o, std::string text, bool d ) {
    if ( gnuplot )
        return;

    for ( size_t i = 0; i < threads.size() - 1; ++ i )
        o << (d ? "=====" : "-----");
    for ( size_t i = 0; i < (10 - text.length()) / 2; ++i )
        o << (d ? "=" : "-");
    o << " " << text << " ";
    for ( size_t i = 0; i < (11 - text.length()) / 2; ++i )
        o << (d ? "=" : "-");
    for ( size_t i = 0; i < threads.size() - 1; ++ i )
        o << (d ? "=====" : "-----");
    o << (d ? " == SUM ==" : " ---------");
    o << std::endl;
}

void TrackStatistics::format( std::ostream &o ) {
    label( o, "QUEUES" );
    matrix( o, diff );

    if ( !gnuplot ) {
        int nthreads = threads.size();
        label( o, "local", false );
        int sum = 0;
        for ( int i = 0; i < nthreads; ++ i )
            printv( o, 9, thread( i ).enq - thread( i ).deq, &sum );
        printv( o, 10, sum, 0 );
        o << std::endl;

        label( o, "totals", false );
        sum = 0;
        for ( int j = 0; j < nthreads; ++ j ) {
            int t = thread( j ).enq - thread( j ).deq;
            for ( int i = 0; i < nthreads; ++ i ) {
                t += thread( i ).sent[ j ] - thread( j ).received[ i ];
            }
            printv( o, 9, t, &sum );
        }
        printv( o, 10, sum, 0 );
        o << std::endl;

        label( o, "CPU USAGE" );
        sum = 0;
        for ( int i = 0; i < nthreads; ++ i )
            printv( o, 9, thread( i ).idle, &sum );
        printv( o, 10, sum, 0 );

        o << std::endl;
        sum = 0;
        for ( int i = 0; i < nthreads; ++ i )
            printv( o, 9, thread( i ).cputime, &sum );
        printv( o, 10, sum, 0 );

        o << std::endl;
        label( o, "HASHTABLES" );
        sum = 0;
        for ( int i = 0; i < nthreads; ++ i )
            printv( o, 9, thread( i ).hashused, &sum );
        printv( o, 10, sum, 0 );

        o << std::endl;
        sum = 0;
        for ( int i = 0; i < nthreads; ++ i )
            printv( o, 9, thread( i ).hashsize, &sum );
        printv( o, 10, sum, 0 );
        o << std::endl;

        label( o, "MEMORY EST" );
        long memSum = 0;
        for ( int j = 0; j < nthreads; ++ j ) {
            PerThread &th = thread( j );
            long threadMem = th.memQueue + th.memHashes + th.hashsize * sizeof(HashSet< Blob >::Cell);
            for ( int i = 0; i < nthreads; ++ i)
                threadMem += thread( i ).memSent[ j ] - thread( j ).memReceived[ i ];
            memSum += threadMem;
            printv(o, 9, threadMem / 1024, 0 );
        }
        printv( o, 10, memSum / 1024, 0 );
        o << std::endl << std::setw(10 * nthreads)
          << "> Used: " << std::setw(11) << memUsed() << std::endl;
    }
}

void TrackStatistics::send() {
    bitblock data;
    data << localmin;

    for ( int i = 0; i < pernode; ++i ) {
        PerThread &t = thread( i + localmin );
        data << t.sent << t.received << t.memSent << t.memReceived;
        data << t.enq << t.deq << t.hashsize << t.hashused << t.memQueue << t.memHashes;
    }

    wibble::sys::MutexLock _lock( mpi.global().mutex );
    mpi.sendStream( _lock, data, 0, TAG_STATISTICS );
}

Loop TrackStatistics::process( wibble::sys::MutexLock &_lock, MpiStatus &status )
{
    bitblock data;
    mpi.recvStream( _lock, status, data );

    int min;
    data >> min;

    for ( int i = 0; i < pernode; ++i ) {
        PerThread &t = thread( i + min );
        data >> t.sent >> t.received >> t.memSent >> t.memReceived;
        data >> t.enq >> t.deq >> t.hashsize >> t.hashused >> t.memQueue >> t.memHashes;
    }

    return Continue;
}

void TrackStatistics::snapshot() {
    std::stringstream str;
    format( str );
    if ( gnuplot )
        str << std::endl;
    if ( output )
        *output << str.str() << std::flush;
    else
        Output::output().statistics() << str.str() << std::flush;
}

void *TrackStatistics::main() {
    while ( true ) {
        if ( mpi.master() ) {
            wibble::sys::sleep( 1 );
            snapshot();
        } else {
            wibble::sys::usleep( 200 * 1000 );
            send();
        }
    }
    return 0;
}


void TrackStatistics::resize( int s ) {
    s = std::max( size_t( s ), threads.size() );
    threads.resize( s, 0 );
    for ( int i = 0; i < s; ++i ) {
        PerThread &th = thread( i );
        th.sent.resize( s, 0 );
        th.received.resize( s, 0 );
        th.enq = th.deq = 0;
        th.hashsize = th.hashused = 0;
        th.memHashes = th.memQueue = 0;
        th.idle = 0;
        th.cputime = 0;
        th.memSent.resize( s, 0 );
        th.memReceived.resize( s, 0 );
    }
}

void TrackStatistics::setup( const Meta &m ) {
    int total = m.execution.nodes * m.execution.threads;
    resize( total );
    mpi.registerMonitor( TAG_STATISTICS, *this );
    pernode = m.execution.threads;
    localmin = m.execution.threads * m.execution.thisNode;
    Output::output().setStatsSize( total * 10 + 11, total + 11 );
}

TrackStatistics::~TrackStatistics() {
    for ( auto p : threads )
        delete p;
}

}

