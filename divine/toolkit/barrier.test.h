// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <cstdlib> // for rand
#include <brick-shmem.h>
#include <divine/toolkit/barrier.h>

using namespace divine;

struct TestBarrier {
    struct Thread : brick::shmem::Thread {
        volatile int i;
        int id;
        bool busy;
        TestBarrier *owner;
        bool sleeping;

        Thread() : i( 0 ), id( 0 ), busy( false ), sleeping( false ) {}
        Thread( const Thread &o ) : i( o.i ), id( o.id ), busy( o.busy ), owner( o.owner ), sleeping( o.sleeping ) {}

        bool workWaiting() {
            if ( busy )
                return true;
            busy = (i % 3) != 1;
            return busy;
        }
        bool isBusy() { return false; }

        void main() {
            busy = true;
            owner->barrier.started( this );
            while ( true ) {
                // std::cerr << "thread " << this << " iteration " << i << std::endl;
                owner->threads[ rand() % owner->count ].i ++;
                if ( i % 3 == 1 ) {
                    busy = false;
                    if ( owner->barrier.idle( this ) )
                        return;
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
