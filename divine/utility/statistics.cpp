// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <iomanip>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cctype>

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

void dump( TrackStatistics &st, std::stringstream &str ) {
    auto s = str.str();
    if ( s.empty() )
        return;

    if ( st.output )
        *st.output << s << std::flush;
    else
        Output::output( st.out_token ).statistics() << s << std::flush;
}

void TrackStatistics::snapshot() {
    std::stringstream str;
    format( str );
    dump( *this, str );
}

void TrackStatistics::main() {
    auto ts = std::chrono::steady_clock::now();
    auto second = std::chrono::duration_cast< decltype( ts )::duration >(
            std::chrono::seconds( 1 ) );

    std::stringstream str;
    startHeader( str );
    dump( *this, str );

    while ( !interrupted() ) {
        if ( mpi.master() ) {
            ts += second;
            std::this_thread::sleep_until( ts );
            snapshot();
        } else {
            std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
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
    meta = &m;
    int total = m.execution.nodes * m.execution.threads;
    resize( total );
    mpi.registerMonitor( TAG_STATISTICS, *this );
    pernode = m.execution.threads;
    localmin = m.execution.threads * m.execution.thisNode;
    shared = m.algorithm.sharedVisitor;
    Output::output().setStatsSize( total * 10 + 11, total + 11 );
}

TrackStatistics::~TrackStatistics() {
    for ( auto p : threads )
        delete p;
}

namespace statistics {

struct Matrix : TrackStatistics {

    Matrix( Baseline b, bool summarize ) : TrackStatistics( b ), summarize( summarize ) { }

    void matrix( std::ostream &o, int64_t (*what)(int64_t, int64_t) ) {
        for ( int i = 0; size_t( i ) < threads.size(); ++i ) {
            int64_t sum = 0;
            o << std::endl;
            for ( int j = 0; size_t( j ) < threads.size(); ++j )
                printv( o, 9, what( thread( i ).sent[ j ], thread( j ).received[ i ] ), &sum );
            if ( !summarize )
                printv( o, 10, sum, 0 );
            o << " items";
        }
    }

    void printv( std::ostream &o, int width, int64_t v, int64_t *sum = nullptr, bool max = false ) {
        o << " " << std::setw( width ) << v;
        if ( sum ) {
            if ( max )
                *sum = std::max( *sum, v );
            else
                *sum += v;
        }
    }

  private:
    const bool summarize;
};

struct Gnuplot : Matrix {
    Gnuplot( Baseline b ) : Matrix( b, false ) { }

    virtual void format( std::ostream &o ) override {
        matrix( o, diff );
        o << std::endl;
    }
};

struct Detailed : Matrix {
    Detailed( Baseline b ) : Matrix( b, true ) { }

    template< typename F >
    void line( std::ostream &o, std::string lbl, F f, bool max = false ) {
        o << std::endl;
        int64_t sum = 0;
        for ( int i = 0; i < int( threads.size() ); ++ i )
            printv( o, 9, f( i ), &sum, max );
        printv( o, 10, sum );
        o << " " << lbl;
    }

    void label( std::ostream &o, std::string text, bool d = true ) {
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

    virtual void format( std::ostream &o ) override {
        label( o, "QUEUES" );
        matrix( o, diff );

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
        line( o, "alloc'd", [&]( int i ) { return thread( i ).hashsize; }, shared );

        label( o, "MEMORY EST" );

        int64_t memSum = 0;
        int64_t tableMem = 0;
        o << std::endl;
        for ( int j = 0; j < nthreads; ++ j ) {
            PerThread &th = thread( j );
            int64_t threadMem = th.memQueue + th.memHashes;
            int64_t tt = th.hashsize * sizeof( Blob ); // FIXME
            if ( shared )
                tableMem += tt;
            else
                tableMem = std::max( tt, tableMem );
            for ( int i = 0; i < nthreads; ++ i)
                threadMem += thread( i ).memSent[ j ] - thread( j ).memReceived[ i ];
            memSum += threadMem;
            printv(o, 9, threadMem / 1024 );
        }
        memSum += tableMem;

        printv( o, 10, memSum / 1024 );
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
};

struct Simple : TrackStatistics {

    virtual void format( std::ostream &o ) override {
        for ( auto s : select )
            o << s.first << "=" << (this->*s.second)() << "; ";
        o << std::endl;
    }

    virtual void startHeader( std::ostream &o ) override {
        o << "meta: model=" << meta->input.model
          << "; algorithm=" << tolower( meta->algorithm.name )
          << "; visitor=" << (meta->algorithm.sharedVisitor ? "shared" : "partitioned")
          << "; compression=" << tolower( std::to_string( meta->algorithm.compression ) )
          << "; threads=" << meta->execution.threads
          << "; mpinodes=" << meta->execution.nodes << ";" << std::endl;
    }

    Simple( Baseline b, std::vector< std::string > selectstr ) :
        TrackStatistics( b ),
        start( std::chrono::steady_clock::now() )
    {
        if ( selectstr.empty() )
            select = map;
        else for ( auto s : selectstr ) {
            std::pair< std::string, int64_t (Simple::*)() > val;
            for ( auto p : map )
                if ( p.first == s )
                    val = p;
            if ( val.first.empty() )
                throw std::invalid_argument( "selector " + s + " not available for statistics" );
            select.emplace_back( val );
        }
    }

  private:
    std::vector< std::pair< std::string, int64_t (Simple::*)() > > select;
    std::chrono::time_point< std::chrono::steady_clock > start;
    static std::vector< std::pair< std::string, int64_t (Simple::*)() > > map;

    int64_t timestamp() {
        auto dur = std::chrono::steady_clock::now() - start;
        return std::chrono::duration_cast< std::chrono::milliseconds >( dur ).count();
    }

    static std::string tolower( std::string str ) {
        for ( auto &c : str )
            c = std::tolower( c );
        return str;
    }

    int64_t hashsizes() {
        return shared
                ? maxof( &PerThread::hashsize )
                : sumof( &PerThread::hashsize );
    }

    int64_t states() {
        return sumof( &PerThread::hashused );
    }

    int64_t statesmem() {
        return sumof( &PerThread::memHashes ) / 1024;
    }

    int64_t avgstate() {
        return sumof( &PerThread::memHashes ) / states();
    }

    int64_t queues() {
        const int nthreads = threads.size();
        int64_t sum = 0;
        for ( int i = 0; i < nthreads; ++i ) {
            sum += thread( i ).enq - thread( i ).deq;
            for ( int j = 0; j < nthreads; ++j )
                sum += thread( j ).sent[ i ] - thread( i ).received[ j ];
        }
        return sum;
    }

    int64_t rssperst() {
        auto st = states();
        return st ? (residentMemNow() * 1024) / st : -1;
    }

    int64_t vmperst() {
        auto st = states();
        return st ? (vmNow() * 1024) / st : -1;
    }

    template< typename T >
    T maxof( T (PerThread::*sel) ) {
        return accumulate( T( 0 ), [sel]( T acc, PerThread &t ) {
                return std::max( acc, t.*sel );
            } );
    }

    template< typename T >
    T sumof( T (PerThread::*sel) ) {
        return accumulate( T( 0 ), [sel]( T acc, PerThread &t ) {
                return acc + t.*sel;
            } );
    }

    template< typename T, typename BinFn >
    T accumulate( T accum, BinFn fn ) {
        const int nthreads = threads.size();
        for ( int i = 0; i < nthreads; ++i )
            accum = fn( accum, thread( i ) );
        return accum;
    }
};

std::vector< std::pair< std::string, int64_t (Simple::*)() > > Simple::map {
    { "hashsize", &Simple::hashsizes },
    { "states",   &Simple::states },
    { "queues",   &Simple::queues },
    { "vmpeak",   &Simple::vmPeak },
    { "vm",       &Simple::vmNow },
    { "vmperst",  &Simple::vmperst },
    { "rsspeak",  &Simple::residentMemPeak },
    { "rss",      &Simple::residentMemNow },
    { "rssperst", &Simple::rssperst },
    { "statesmem", &Simple::statesmem },
    { "avgstate", &Simple::avgstate },
    { "time",     &Simple::timestamp }
};

} // namespace statistics

void TrackStatistics::makeGlobalDetailed( Baseline b ) {
    _global().reset( new statistics::Detailed( b ) );
}

void TrackStatistics::makeGlobalGnuplot( Baseline b, std::string file ) {
    _global().reset( new statistics::Gnuplot( b ) );
    if ( file != "-" )
        _global()->output.reset( new std::ofstream( file ) );
}

void TrackStatistics::makeGlobalSimple( Baseline b, std::vector< std::string > selectors ) {
    _global().reset( new statistics::Simple( b, selectors ) );
}

} // namespace divine

