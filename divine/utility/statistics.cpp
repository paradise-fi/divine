// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <iomanip>
#include <mutex>
#include <chrono>

#include <bricks/brick-hashset.h>
#include <divine/utility/statistics.h>
#include <divine/utility/output.h>

#ifdef __unix
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace divine {

NoStatistics NoStatistics::_global;

#ifdef __unix
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

void TrackStatistics::matrix( std::ostream &o, int64_t (*what)(int64_t, int64_t) ) {
    for ( int i = 0; size_t( i ) < threads.size(); ++i ) {
        int64_t sum = 0;
        o << std::endl;
        for ( int j = 0; size_t( j ) < threads.size(); ++j )
            printv( o, 9, what( thread( i ).sent[ j ], thread( j ).received[ i ] ), &sum );
        if ( !gnuplot )
            printv( o, 10, sum, 0 );
        o << " items";
    }
}

void TrackStatistics::printv( std::ostream &o, int width, int64_t v, int64_t *sum ) {
    o << " " << std::setw( width ) << v;
    if ( sum )
        *sum += v;
}

void TrackStatistics::label( std::ostream &o, std::string text, bool d ) {
    if ( gnuplot )
        return;

    o << std::endl;
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
}

template< typename F >
void TrackStatistics::line( std::ostream &o, std::string lbl, F f ) {
    o << std::endl;
    int64_t sum = 0;
    for ( int i = 0; i < int( threads.size() ); ++ i )
        printv( o, 9, f( i ), &sum );
    printv( o, 10, sum, 0 );
    o << " " << lbl;
}

void TrackStatistics::format( std::ostream &o ) {
    label( o, "QUEUES" );
    matrix( o, diff );

    if ( !gnuplot ) {
        int nthreads = threads.size();
        label( o, "local", false );
        line( o, "items", [&]( int i ) { return thread( i ).enq - thread( i ).deq; } );

        label( o, "totals", false );
        line( o, "items", [&]( int i ) {
                int64_t x = thread( i ).enq - thread( i ).deq;
                for ( int j = 0; j < nthreads; ++j )
                    x += thread( j ).sent[ i ] - thread( i ).received[ j ];
                return x;
            } );

        label( o, "CPU USAGE" );
        line( o, "idle cnt", [&]( int i ) { return thread( i ).idle; } );
        line( o, "cpu sec", [&]( int i ) { return thread( i ).cputime; } );

        label( o, "HASHTABLES" );
        line( o, "used", [&]( int i ) { return thread( i ).hashused; } );
        line( o, "alloc'd", [&]( int i ) { return thread( i ).hashsize; } );

        label( o, "MEMORY EST" );
        long memSum = 0;
        o << std::endl;
        for ( int j = 0; j < nthreads; ++ j ) {
            PerThread &th = thread( j );
            long threadMem = th.memQueue + th.memHashes + th.hashsize * sizeof( Blob ) /* FIXME */;
            for ( int i = 0; i < nthreads; ++ i)
                threadMem += thread( i ).memSent[ j ] - thread( j ).memReceived[ i ];
            memSum += threadMem;
            printv(o, 9, threadMem / 1024, 0 );
        }
        printv( o, 10, memSum / 1024, 0 );
        o << " kB" << std::endl;
        auto meminfo = [&]( std::string i, int64_t v ) {
            o << std::setw( 10 * nthreads ) << i << std::setw( 11 )
              << v << " kB" << std::endl;
        };
        meminfo( "> Virtual mem peak:  ", vmPeak() );
        meminfo( "> Virtual mem:       ", vmNow() );
        meminfo( "> Physical mem peak: ", residentMemPeak() );
        meminfo( "> Physical mem:      ", residentMemNow() );
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

    std::unique_lock< std::mutex > _lock( mpi.global().mutex );
    mpi.sendStream( _lock, data, 0, TAG_STATISTICS );
}

Loop TrackStatistics::process( std::unique_lock< std::mutex > &_lock, MpiStatus &status )
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
        Output::output( out_token ).statistics() << str.str() << std::flush;
}

void TrackStatistics::main() {
    while ( !interrupted() ) {
        if ( mpi.master() ) {
            std::this_thread::sleep_for( 
                std::chrono::seconds( 1 )
            );
            snapshot();
        } else {
            std::this_thread::sleep_for(
                std::chrono::milliseconds( 200 )
            );
            send();
        }
    }
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
    // the destructor is idempotent and must be run before data are deallocated
    static_cast< Thread * >( this )->~Thread();
    for ( auto p : threads )
        delete p;
}

}

