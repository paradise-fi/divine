// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <set>
#include <wibble/sys/thread.h>
#include <divine/toolkit/concurrentset.h>
#include <divine/toolkit/hashset.test.h>

using namespace divine;

template< template< typename > class HS >
struct Parallel
{
    struct Insert : wibble::sys::Thread {
        HS< int > *_set;
        typename HS< int >::ThreadData td;
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

    static void insert() {
        HS< int > set;
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

    static void _par( HS< int > *set, int f1, int t1, int f2, int t2 )
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

    static void _multi( HS< int > *set, std::size_t count, int from, int to )
    {
        Insert *arr = new Insert[ count ];

        for ( std::size_t i = 0; i < count; ++i ) {
            arr[ i ].from = from;
            arr[ i ].to = to;
            arr[ i ]._set = set;
            arr[ i ].overlap = true;
        }

        for ( std::size_t i = 0; i < count; ++i )
            arr[ i ].start();

        for ( std::size_t i = 0; i < count; ++i )
            arr[ i ].join();

        delete[] arr;
    }

    static void multi()
    {
        HS< int > set;
        set.setSize( 4 * 1024 );
        _multi( &set, 10, 1, 32 * 1024 );

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

    static void stress()
    {
        HS< int > set;

        set.setSize( 4 * 1024 );
        _par( &set, 1, 16*1024, 8*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i )
            assert( set.count( i ) );
    }

    static void set() {
        HS< int > set;
        set.setSize( 4 * 1024 );
        for ( int i = 1; i < 32*1024; ++i )
            assert( !set.count( i ) );

        _par( &set, 1, 16*1024, 16*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i )
            assert_eq( i, i * set.count( i ) );

        for ( int i = 32*1024; i < 64 * 1024; ++i )
            assert( !set.count( i ) );
    }
};

struct TestConcurrentSet
{
    template< typename T > using CS = ConcurrentSet< T >;
    template< typename T > using FCS = FastConcurrentSet< T >;

    Test basicCS() { Cases< CS >::basic(); }
    Test basicFCS() { Cases< FCS >::basic(); }

    Test stressCS() { Cases< CS >::stress(); }
    Test stressFCS() { Cases< FCS >::stress(); }

    Test setCS() { Cases< CS >::set(); }
    Test setFCS() { Cases< FCS >::set(); }

    Test blobishCS() { Cases< CS >::blobish(); }
    Test blobishFCS() { Cases< FCS >::blobish(); }

    Test insertCS() { Parallel< CS >::insert(); }
    Test insertFCS() { Parallel< FCS >::insert(); }

    Test pSetCS() { Parallel< CS >::set(); }
    Test pSetFCS() { Parallel< FCS >::set(); }

    Test pStressCS() { Parallel< CS >::stress(); }
    Test pStressFCS() { Parallel< FCS >::stress(); }

    Test pMultiCS() { Parallel< CS >::multi(); }
    Test pMultiFCS() { Parallel< FCS >::multi(); }
};
