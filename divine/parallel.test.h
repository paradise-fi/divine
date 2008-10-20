// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/parallel.h>

using namespace divine;

struct TestParallel {
    struct Counter {
        int i;
        void inc() { i++; }
        Counter() : i( 0 ) {}
    };

    Test runThread() {
        Counter c;
        assert_eq( c.i, 0 );
        RunThread< Counter > t( c, &Counter::inc );
        assert_eq( c.i, 0 );
        t.start();
        t.join();
        assert_eq( c.i, 1 );
    }

    struct ParCounter {
        struct Shared {
            int i;
        } shared;

        void inc() {
            shared.i ++;
        }
        ParCounter() { shared.i = 0; }
    };

    Test parCounter() {
        ParCounter m;
        assert_eq( m.shared.i, 0 );

        Parallel< ParCounter > p( m, 10 );
        p.run( &ParCounter::inc );
        for ( int i = 0; i < 10; ++i )
            assert_eq_l( i, p.shared( i ).i, 1 );

        m.shared.i = 10;
        p.run( &ParCounter::inc );
        for ( int i = 0; i < 10; ++i )
            assert_eq( p.shared( i ).i, 11 );
    }
};
