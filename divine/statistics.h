// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_STATISTICS_H
#define DIVINE_STATISTICS_H

#include <wibble/sys/thread.h>
#include <wibble/sys/mutex.h>
#include <wibble/string.h>
#include <wibble/regexp.h>
#include <divine/mpi.h>
#include <divine/report.h>
#include <iomanip>

#include <divine/output.h>

#define TAG_STATISTICS 128

namespace divine {

struct NoStatistics {
    void enqueue( int , int ) {}
    void dequeue( int , int ) {}
    void hashsize( int , int ) {}
    void hashadded( int , int ) {}
    void sent( int, int, int ) {}
    void received( int, int, int ) {}

    std::ostream *output;
    bool gnuplot;

    static NoStatistics &global() {
        static NoStatistics *g = new NoStatistics;
        return *g;
    }

    template< typename D >
    void useDomain( D &d ) {}
    void start() {}
};

struct Statistics : wibble::sys::Thread, MpiMonitor {
    struct PerThread {
        std::vector< int > sent;
        std::vector< int > received;
        int enq, deq;
        int hashsize;
        int hashused;
        int memQueue;
        int memHashes;
        std::vector< int > memSent;
        std::vector< int > memReceived;
    };

    std::vector< PerThread * > threads;
    divine::MpiBase *mpi;
    int pernode, localmin;

    bool gnuplot;
    std::ostream *output;

    void enqueue( int id , int size ) {
        thread( id ).enq ++;
        thread( id ).memQueue += size;
    }

    void dequeue( int id , int size ) {
        thread( id ).deq ++;
        thread( id ).memQueue -= size;
    }

    void hashsize( int id , int s ) {
        thread( id ).hashsize = s;
    }

    void hashadded( int id , int nodeSize ) {
        thread( id ).hashused ++;
        thread( id ).memHashes += nodeSize;
    }

    PerThread &thread( int id ) {
        assert_leq( size_t( id ), threads.size() );
        if ( !threads[ id ] )
            threads[ id ] = new PerThread;
        return *threads[ id ];
    }

    void sent( int from, int to, int nodeSize ) {
        assert_leq( 0, to );

        PerThread &f = thread( from );
        if ( f.sent.size() <= size_t( to ) )
            f.sent.resize( to + 1, 0 );
        ++ f.sent[ to ];
        f.memSent[ to ] += nodeSize;
    }

    void received( int from, int to, int nodeSize ) {
        assert_leq( 0, from );

        PerThread &t = thread( to );
        if ( t.received.size() <= size_t( from ) )
            t.received.resize( from + 1, 0 );
        ++ t.received[ from ];
        t.memReceived[ from ] += nodeSize;
    }

    static int first( int a, int ) { return a; }
    static int second( int, int b ) { return b; }
    static int diff( int a, int b ) { return a - b; }

    void matrix( std::ostream &o, int (*what)(int, int) ) {
        for ( int i = 0; size_t( i ) < threads.size(); ++i ) {
            int sum = 0;
            for ( int j = 0; size_t( j ) < threads.size(); ++j )
                printv( o, 9, what( thread( i ).sent[ j ], thread( j ).received[ i ] ), &sum );
            if ( !gnuplot )
                printv( o, 10, sum, 0 );
            o << std::endl;
        }
    }

    void printv( std::ostream &o, int width, int v, int *sum ) {
        o << " " << std::setw( width ) << v;
        if ( sum )
            *sum += v;
    }

    void label( std::ostream &o, std::string text, bool d = true ) {
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

    void format( std::ostream &o ) {
        label( o, "QUEUES" );
        matrix( o, diff );

        if ( !gnuplot ) {
            label( o, "local", false );
            int sum = 0;
            for ( int i = 0; i < threads.size(); ++ i )
                printv( o, 9, thread( i ).enq - thread( i ).deq, &sum );
            printv( o, 10, sum, 0 );
            o << std::endl;

            label( o, "totals", false );
            sum = 0;
            for ( int j = 0; j < threads.size(); ++ j ) {
                int t = thread( j ).enq - thread( j ).deq;
                for ( int i = 0; i < threads.size(); ++ i ) {
                    t += thread( i ).sent[ j ] - thread( j ).received[ i ];
                }
                printv( o, 9, t, &sum );
            }
            printv( o, 10, sum, 0 );
            o << std::endl;

            label( o, "HASHTABLES" );
            sum = 0;
            for ( int i = 0; i < threads.size(); ++ i )
                printv( o, 9, thread( i ).hashused, &sum );
            printv( o, 10, sum, 0 );

            o << std::endl;
            sum = 0;
            for ( int i = 0; i < threads.size(); ++ i )
                printv( o, 9, thread( i ).hashsize, &sum );
            printv( o, 10, sum, 0 );
            o << std::endl;
            
            label( o, "MEMORY EST" );
            long memSum = 0;
            for ( int j = 0; j < threads.size(); ++ j ) {
                PerThread &th = thread( j );
            	long threadMem = th.memQueue + th.memHashes + th.hashsize * sizeof(hash_t);
                for ( int i = 0; i < threads.size(); ++ i)
                    threadMem += thread( i ).memSent[ j ] - thread( j ).memReceived[ i ];
                memSum += threadMem;
                printv(o, 9, threadMem / 1024, 0 );
            }
            printv( o, 10, memSum / 1024, 0 );
            o << std::endl << std::setw(10 * threads.size())
            	<< "> Used: " << std::setw(11) << memUsed() << std::endl;
        }
    }

    static int memUsed() {
        sysinfo::Info i;
        return i.peakVmSize();
    }
    

#ifdef O_MPI
    void send() {
        std::vector< int > data;
        data.push_back( localmin );

        for ( int i = 0; i < pernode; ++i ) {
            PerThread &t = thread( i + localmin );
            std::copy( t.sent.begin(), t.sent.end(),
                       std::back_inserter( data ) );
            std::copy( t.received.begin(), t.received.end(),
                       std::back_inserter( data ) );
            std::copy( t.memSent.begin(), t.memSent.end(),
                       std::back_inserter( data ) );
            std::copy( t.memReceived.begin(), t.memReceived.end(),
                       std::back_inserter( data ) );
            data.push_back( t.enq );
            data.push_back( t.deq );
            data.push_back( t.hashsize );
            data.push_back( t.hashused );
            data.push_back( t.memQueue );
            data.push_back( t.memHashes );
        }

        mpi->send( &data.front(), data.size(), MPI::INT, 0, TAG_STATISTICS );
        mpi->mpidebug() << "stats sent: " << wibble::str::fmt( data ) << std::endl;
    }

    Loop process( wibble::sys::MutexLock &, MPI::Status &status )
    {
        std::vector< int > data;
        int n = threads.size();
        int sendrecv = n * 4,
               local = 6;
        data.resize( 1 /* localmin */ + pernode * (sendrecv + local) );

        MPI::COMM_WORLD.Recv( &data.front(), data.size(),
                              MPI::INT, status.Get_source(), status.Get_tag() );
        mpi->mpidebug() << "stats received: " << wibble::str::fmt( data ) << std::endl;

        int min = data.front();
        std::vector< int >::iterator iter = data.begin() + 1;

        for ( int i = 0; i < pernode; ++i ) {
            PerThread &t = thread( i + min );
            std::copy( iter, iter + n, t.sent.begin() );
            std::copy( iter + n, iter + 2*n, t.received.begin() );
            std::copy( iter + 2*n, iter + 3*n, t.memSent.begin() );
            std::copy( iter + 3*n, iter + 4*n, t.memReceived.begin() );

            iter += sendrecv;

            t.enq = *iter++;
            t.deq = *iter++;
            t.hashsize = *iter++;
            t.hashused = *iter++;
            t.memQueue = *iter++;
            t.memHashes = *iter++;
        }

        return Continue;
        // TODO
    }
#else
    void send() {}
#endif

    void snapshot() {
        std::stringstream str;
        format( str );
        if ( gnuplot )
            str << std::endl;
        if ( output )
            *output << str.str() << std::flush;
        else
            Output::output().statistics() << str.str() << std::flush;
    }

    void *main() {
        while ( true ) {
            if ( !mpi || mpi->master() ) {
                wibble::sys::sleep( 1 );
                snapshot();
            }

            if ( mpi && !mpi->master() ) {
                wibble::sys::usleep( 200 * 1000 );
                send();
            }
        }
        return 0;
    }

    Statistics() : mpi( 0 ), pernode( 1 ), localmin( 0 )
    {
        output = 0;
        gnuplot = false;
        resize( 1 );
    }

    void resize( int s ) {
        s = std::max( size_t( s ), threads.size() );
        threads.resize( s, 0 );
        for ( int i = 0; i < s; ++i ) {
        	PerThread &th = thread( i );
            th.sent.resize( s, 0 );
            th.received.resize( s, 0 );
            th.enq = th.deq = 0;
            th.hashsize = th.hashused = 0;
            th.memHashes = th.memQueue = 0;
            th.memSent.resize( s, 0 );
            th.memReceived.resize( s, 0 );
        }
    }

    template< typename D >
    void useDomain( D &d ) {
        int threadcount = d.n * d.mpi.size();
        resize( threadcount );
        mpi = &d.mpi;
        mpi->registerMonitor( TAG_STATISTICS, *this );
        pernode = d.n;
        localmin = d.minId;
        Output::output().setStatsSize( threadcount * 10 + 11, threadcount + 11 );
    }

    static Statistics &global() {
        static Statistics *g = new Statistics;
        return *g;
    }
};

template <typename Ty>
int memSize(Ty x) {
    return sizeof(x);
}

template <>
inline int memSize<Blob>(Blob x) {
    return sizeof(Blob) + (x.valid() ? Blob::allocationSize(x.size()) : 0);
}

}

#endif
