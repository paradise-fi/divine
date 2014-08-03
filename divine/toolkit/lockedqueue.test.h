#pragma once

#include <atomic>
#include <algorithm>
#include <numeric>

#include <wibble/test.h>
#include <divine/toolkit/lockedqueue.h>

using namespace divine;

/*template< typename T >
std::ostream& operator<<( std::ostream& out, const std::list< T >& l ) {
    bool begin = true;
    for ( auto i : l ) {
        if ( begin )
            begin = false;
        else
            out << " ";
        out << i;
    }
    return out;
}
*/
struct TestLockedQueue {

    typedef void Test;

    struct Producer : brick::shmem::Thread {
        LockedQueue< int > *q;
        int produce;
        std::atomic< int > *work;
        void main() {
            for ( int i = 1; i <= produce; ++i)
                q->push( i );
            --*work;
        }
    };
    struct Consumer : brick::shmem::Thread {

        LockedQueue< int > *q;
        int consumed;
        std::atomic< int > *work;
        void main() {
            consumed = 0;
            while( *work ) {
                while( !q->empty() ) {
                    if( q->pop() )
                        ++consumed;
                }
            }
            while( !q->empty() ) {
                if( q->pop() )
                    ++consumed;
            }
        }
    };

    Test stress() {
        const int threads = 3;
        std::atomic< int > work( threads );

        Producer *producents = new Producer[ threads ];
        Consumer *consumers = new Consumer[ threads ];

        LockedQueue< int > queue;

        for ( int i = 0; i < threads; ++i ) {
            producents[ i ].produce = 10000;
            consumers[ i ].work = producents[ i ].work = &work;
            consumers[ i ].q = producents[ i ].q = &queue;
            producents[ i ].start();
            consumers[ i ].start();
        }
        for ( int i = 0; i < threads; ++i ) {
            producents[ i ].join();
            consumers[ i ].join();
        }
        int inserts = std::accumulate( producents, producents + threads, 0,
                                       []( int v, const Producer& p ) -> int {
                                           return v + p.produce;
                                    } );
        int reads = std::accumulate( consumers, consumers + threads, 0,
                                     []( int v, const Consumer& c) -> int {
                                         return v + c.consumed;
                                    } );
        assert_eq( queue.q.size(), 0u );
        assert_eq( reads, inserts );
        assert( queue.empty() );
    }
};
