// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <iomanip>
#include <wibble/string.h>

#include <divine/statistics.h>
#include <divine/output.h>

namespace divine {

void Statistics::matrix( std::ostream &o, int (*what)(int, int) ) {
    for ( int i = 0; size_t( i ) < threads.size(); ++i ) {
        int sum = 0;
        for ( int j = 0; size_t( j ) < threads.size(); ++j )
            printv( o, 9, what( thread( i ).sent[ j ], thread( j ).received[ i ] ), &sum );
        if ( !gnuplot )
            printv( o, 10, sum, 0 );
        o << std::endl;
    }
}

void Statistics::printv( std::ostream &o, int width, int v, int *sum ) {
    o << " " << std::setw( width ) << v;
    if ( sum )
        *sum += v;
}

void Statistics::label( std::ostream &o, std::string text, bool d ) {
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

void Statistics::format( std::ostream &o ) {
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

#ifdef O_MPI
void Statistics::send() {
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

Loop Statistics::process( wibble::sys::MutexLock &, MPI::Status &status )
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
}
#endif

void Statistics::snapshot() {
    std::stringstream str;
    format( str );
    if ( gnuplot )
        str << std::endl;
    if ( output )
        *output << str.str() << std::flush;
    else
        Output::output().statistics() << str.str() << std::flush;
}

void *Statistics::main() {
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


void Statistics::resize( int s ) {
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

void Statistics::setup( const Meta &m, MpiBase *_mpi ) {
    int total = m.execution.nodes * m.execution.threads;
    resize( total );
    mpi = _mpi;
    if ( mpi )
        mpi->registerMonitor( TAG_STATISTICS, *this );
    pernode = m.execution.threads;
    localmin = m.execution.threads * m.execution.thisNode;
    Output::output().setStatsSize( total * 10 + 11, total + 11 );
}

}

