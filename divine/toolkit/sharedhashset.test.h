// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <divine/toolkit/sharedhashset.h>

using namespace divine;

struct TestSharedHashset
{
    Test basic() {
        SharedHashSet< int > set( 8 );
        assert( !set.has( 1 ) );
        assert( set.insert( 1 ) );
        assert( set.has( 1 ) );
    }

    template< typename T >
    struct Insert : wibble::sys::Thread {
        SharedHashSet< T > *set;
        int from, to;
        bool overlap;

        void *main() {
            for ( int i = from; i < to; ++i ) {
                set->insert( i );
                assert( set->has( i ) );
                if (!overlap && i < to - 1)
                    assert( !set->has( i + 1 ) );
            }
            return 0;
        }
    };

    Test stress() {
        SharedHashSet< int > set( 65536 );
        Insert< int > a;
        a.set = &set;
        a.from = 1;
        a.to = 32 * 1024;
        a.overlap = false;
        a.main();
        for ( int i = 1; i < 32*1024; ++i ) {
            assert( set.has( i ) );
        }
    }

    void par( SharedHashSet< int > *set, int f1, int t1, int f2, int t2 )
    {
        Insert< int > a, b;

        a.from = f1;
        a.to = t1;
        b.from = f2;
        b.to = t2;
        a.set = b.set = set;
        a.overlap = b.overlap = (t1 > f2);

        a.start();
        b.start();
        a.join();
        b.join();
    }

    Test parstress() {
        SharedHashSet< int > set( 64*1024 );
        par( &set, 1, 16*1024, 8*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i ) {
            assert( set.has( i ) );
        }
    }

    Test set() {
        SharedHashSet< int > set( 64*1024 );

        for ( int i = 1; i < 32*1024; ++i ) {
            assert( !set.has( i ) );
        }

        par( &set, 1, 16*1024, 16*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i ) {
            assert_eq( i, i * set.has( i ) );
        }

        for ( int i = 32*1024; i < 64 * 1024; ++i ) {
            assert( !set.has( i ) );
        }
    }
};
