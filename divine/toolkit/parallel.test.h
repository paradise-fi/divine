// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/parallel.h>

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

    Test threadVector() {
        std::vector< Counter > vec;
        vec.resize( 10 );
        ThreadVector< Counter > p( vec, &Counter::inc );
        p.run();

        for ( int i = 0; i < 10; ++i )
            assert_eq_l( i, vec[ i ].i, 1 );
    }

    struct ParallelCounter : Parallel< Topology< wibble::Unit >::Local, ParallelCounter >
    {
        Counter counter;

        int get() { return counter.i; }
        void set( Counter c ) { counter = c; }
        void inc() { counter.inc(); }

        void run() {
            this->topology().distribute( counter, &ParallelCounter::set );
            this->topology().parallel( &ParallelCounter::inc );
        }

        ParallelCounter( bool master = true ) {
            if (master) this->becomeMaster( 10, false );
        }
    };

    template< typename X >
    void checkValues( X &x, int n, int k ) {
        std::vector< int > values;
        x.topology().collect( values, &X::get );
        assert_eq( values.size(), n );
        for ( int i = 0; i < values.size(); ++i )
            assert_eq_l( i, values[ i ], k );
    }

    Test parallel() {
        ParallelCounter c;
        assert_eq( c.get(), 0 );
        checkValues( c, 10, 0 );

        c.run();
        assert_eq( c.get(), 0 );
        checkValues( c, 10, 1 );
    }

    struct CommCounter : Parallel< Topology< int >::Local, CommCounter >
    {
        Counter counter;

        void tellInc() {
            submit( this->id(), (this->id() + 1) % this->peers(), 1 );
            do {
                if ( comms().pending( id() ) ) {
                    counter.i += comms().take( this->id() );
                    return;
                }
            } while ( true );
        }

        int get() { return counter.i; }
        void run() {
            this->topology().parallel( &CommCounter::tellInc );
        }

        CommCounter( bool master = true ) {
            if (master) this->becomeMaster( 10, false );
            counter.i = 0;
        }
    };

    Test comm() {
        CommCounter c;
        assert_eq( c.counter.i, 0 );
        checkValues( c, 10, 0 );

        c.run();
        assert_eq( c.counter.i, 0 );
        checkValues( c, 10, 1 );
    }
};
