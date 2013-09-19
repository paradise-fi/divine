// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <set>
#include <wibble/sys/thread.h>
#include <divine/toolkit/concurrentset.h>
#include <divine/toolkit/hashset.test.h>

using namespace divine;

struct TestSharedHashSet
{

    Test basic() {
        ConcurrentSet< int > set;

        set.setSize( 8 );

        assert( !set.count( 1 ) );
        assert( set.insert( 1 ).isnew() );
        assert( set.count( 1 ) );
        unsigned count = 0;
        for ( unsigned i = 0; i != set.size(); ++i )
            if ( set[ i ] )
                ++count;
        assert_eq( count, unsigned(1) );
    }

    struct Insert : wibble::sys::Thread {
        ConcurrentSet< int > *_set;
        typename ConcurrentSet< int >::ThreadData td;
        int from, to;
        bool overlap;

        void *main() {
            auto set = _set->withTD( td );
            for ( int i = from; i < to; ++i ) {
                set.insert( i );
                assert( !set.insert( i ).isnew() );
                if ( !overlap && i < to - 1 )
                    assert( !set.count( i + 1 ) );
            }
            return 0;
        }
    };

    Test stress() {
        ConcurrentSet< int > set;
        set.setSize( 4 * 1024 );
        Insert a;
        a._set = &set;
        a.from = 1;
        a.to = 32 * 1024;
        a.overlap = false;
        a.main();
        for ( int i = 1; i < 32*1024; ++i )
            assert( set.count( i ) );
    }

    void par( ConcurrentSet< int > *set, int f1, int t1, int f2, int t2 )
    {
        Insert a, b;

        a.from = f1;
        a.to = t1;
        b.from = f2;
        b.to = t2;
        a._set = set;
        b._set = set;
        a.overlap = b.overlap = (t1 > f2);

        a.start();
        b.start();
        a.join();
        b.join();
    }

    void multi( ConcurrentSet< int > *set, std::size_t count, int from, int to ) {
        Insert *field = new Insert[ count ];

        for ( std::size_t i = 0; i < count; ++i ) {
            field[ i ].from = from;
            field[ i ].to = to;
            field[ i ]._set = set;
            field[ i ].overlap = true;
        }

        for ( std::size_t i = 0; i < count; ++i )
            field[ i ].start();

        for ( std::size_t i = 0; i < count; ++i )
            field[ i ].join();

        delete[] field;
    }

    Test multistress() {
        ConcurrentSet< int > set;
        set.setSize( 4 * 1024 );
        multi( &set, 10, 1, 32 * 1024 );

        for  ( int i = 1; i < 32 * 1024; ++i )
            assert( set.count( i ) );

        int count = 0;
        std::set< int > s;
        for ( size_t i = 0; i != set.size(); ++i ) {
            if ( set[ i ] ) {
                if ( s.find( set[ i ] ) != s.end() )
                    std::cout << set[ i ] << std::endl;
                else
                    s.insert( set[ i ] );
                ++count;
            }
        }
        assert_eq( count, 32 * 1024 - 1 );
    }

    Test parstress() {
        ConcurrentSet< int > set;

        set.setSize( 4 * 1024 );
        par( &set, 1, 16*1024, 8*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i )
            assert( set.count( i ) );
    }

    Test set() {
        ConcurrentSet< int > set;
        set.setSize( 4 * 1024 );
        for ( int i = 1; i < 32*1024; ++i )
            assert( !set.count( i ) );

        par( &set, 1, 16*1024, 16*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i )
            assert_eq( i, i * set.count( i ) );

        for ( int i = 32*1024; i < 64 * 1024; ++i )
            assert( !set.count( i ) );
    }
};
