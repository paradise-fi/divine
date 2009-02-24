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
        typedef Counter Shared;
        Counter shared;

        void inc() {
            shared.inc();
        }
    };

    Test parCounter() {
        ParCounter m;
        assert_eq( m.shared.i, 0 );

        Parallel< ParCounter > p( 10 );
        p.run( m.shared, &ParCounter::inc );
        for ( int i = 0; i < 10; ++i )
            assert_eq_l( i, p.shared( i ).i, 1 );

        m.shared.i = 10;
        p.run( m.shared, &ParCounter::inc );
        for ( int i = 0; i < 10; ++i )
            assert_eq( p.shared( i ).i, 11 );
    }

    struct DomCounter : DomainWorker< DomCounter >
    {
        typedef Counter Shared;
        Domain< DomCounter > domain;
        Counter shared;
        void inc() { shared.inc(); }

        void run() {
            domain.parallel().run( shared, &DomCounter::inc );
        }

        DomCounter() { shared.i = 0; }
    };

    Test domCounter() {
        DomCounter d;
        assert_eq( d.shared.i, 0 );
        for ( int i = 0; i < d.domain.n; ++i )
            assert_eq_l( i, d.domain.parallel().shared( i ).i, 0 );
        d.run();
        assert_eq( d.shared.i, 0 );
        for ( int i = 0; i < d.domain.n; ++i )
            assert_eq_l( i, d.domain.parallel().shared( i ).i, 1 );
    }

    struct Dom2Counter : DomainWorker< Dom2Counter >
    {
        typedef Counter Shared;
        Domain< Dom2Counter > domain;
        Counter shared;

        void tellInc() {
            Blob b( sizeof( int ) );
            b.get< int >() = 1;
            queue( globalId(), (globalId() + 1) % peers() ).push( b );
            do {
                if ( !fifo.empty() ) {
                    shared.i += fifo.next().get< int >();
                    fifo.next().free();
                    return;
                }
            } while ( true );
        }

        void run() {
            domain.parallel().run( shared, &Dom2Counter::tellInc );
        }

        Dom2Counter() { shared.i = 0; }
    };

    Test dom2Counter() {
        Dom2Counter d;
        assert_eq( d.shared.i, 0 );
        for ( int i = 0; i < d.domain.n; ++i )
            assert_eq_l( i, d.domain.parallel().shared( i ).i, 0 );
        d.run();
        assert_eq( d.shared.i, 0 );
        for ( int i = 0; i < d.domain.n; ++i )
            assert_eq_l( i, d.domain.parallel().shared( i ).i, 1 );
    }
};
