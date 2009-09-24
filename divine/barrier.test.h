// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <divine/barrier.h>
#include <stdlib.h> // for rand

using namespace divine;

struct TestBarrier {
    struct Thread : wibble::sys::Thread {
        volatile int i;
        int id;
        bool busy;
        TestBarrier *owner;
        bool sleeping;
        bool workWaiting() {
            if ( busy )
                return true;
            busy = (i % 3) != 1;
            return busy;
        }

        void *main() {
            busy = true;
            owner->barrier.started( this );
            while ( true ) {
                // std::cerr << "thread " << this << " iteration " << i << std::endl;
                owner->threads[ rand() % owner->count ].i ++;
                if ( i % 3 == 1 ) {
                    busy = false;
                    if ( owner->barrier.idle( this ) )
                        return 0;
                }
            }
        }
    };

    Barrier< Thread > barrier;
    std::vector< Thread > threads;
    int count;

    Test basic() {
        Thread bp;
        count = 5;
        bp.owner = this;
        bp.i = 0;
        threads.resize( count, bp );
        barrier.setExpect( count );
        for ( int i = 0; i < count; ++i ) {
            threads[ i ].id = i;
            threads[ i ].start();
        }
        for ( int i = 0; i < count; ++i ) {
            threads[ i ].join();
            assert_eq( threads[ i ].i % 3, 1 );
        }
        barrier.clear();
    }

};
