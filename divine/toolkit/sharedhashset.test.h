// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <set>
#include <wibble/sys/thread.h>
#include <divine/toolkit/sharedhashset.h>

using namespace divine;

template< typename _Item, typename _Hasher = divine::default_hasher< _Item > >
struct SharedHashSetWrapper {

    using Set = SharedHashSet< _Item, _Hasher >;
    using Hasher = _Hasher;
    using Item = _Item;

    Set &table;
    Hasher &hasher;
    typename Set::ThreadData tld;

    SharedHashSetWrapper( Set &t ) : table( t ), hasher( table.hasher ), tld()
    {}

    void setSize( size_t s ) {
        table.setSize( s );
    }

    size_t size() {
        return table.size();
    }

    Item operator[]( size_t index ) {
        return table[ index ];
    }

    std::tuple< Item, bool > insertHinted( Item x, hash64_t h ) {
        return table.insertHinted( x, h, tld );
    }

    std::tuple< Item, bool > getHinted( Item x, hash64_t h ) {
        return table.getHinted( x, h, tld );
    }

    std::tuple< Item, bool > insert( Item x ) {
        return insertHinted( x, hasher.hash( x ).first );
    }

    bool has( Item x ) {
        return std::get< 1 >( getHinted( x, hasher.hash( x ).first ) );
    }
};

struct TestSharedHashSet
{

    Test basic() {
        SharedHashSet< int > _set;
        SharedHashSetWrapper< int > set( _set );

        set.setSize( 8 );

        assert( !set.has( 1 ) );
        assert( std::get< 1 >( set.insert( 1 ) ) );
        assert( set.has( 1 ) );
        unsigned count = 0;
        for ( unsigned i = 0; i != set.size(); ++i )
            if ( set[ i ] )
                ++count;
        assert_eq( count, unsigned(1) );
    }

    struct Insert : wibble::sys::Thread {
        SharedHashSetWrapper< int > *set;
        int from, to;
        bool overlap;

        void *main() {
            for ( int i = from; i < to; ++i ) {
                set->insert( i );
                assert( !std::get< 1 >( set->insert( i ) ) );
                if ( !overlap && i < to - 1 )
                    assert( !set->has( i + 1 ) );
            }
            return 0;
        }
    };

    Test stress() {
        SharedHashSet< int > _set;
        SharedHashSetWrapper< int > set( _set );
        set.setSize( 4 * 1024 );
        Insert a;
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
        Insert a, b;

        a.from = f1;
        a.to = t1;
        b.from = f2;
        b.to = t2;
        a.set = new SharedHashSetWrapper< int >( *set );
        b.set = new SharedHashSetWrapper< int >( *set );
        a.overlap = b.overlap = (t1 > f2);

        a.start();
        b.start();
        a.join();
        b.join();

        delete a.set;
        delete b.set;
    }

    void multi( SharedHashSet< int > *set, std::size_t count, int from, int to ) {
        Insert *field = new Insert[ count ];

        for ( std::size_t i = 0; i < count; ++i ) {
            field[ i ].from = from;
            field[ i ].to = to;
            field[ i ].set = new SharedHashSetWrapper< int >( *set );
            field[ i ].overlap = true;
        }

        for ( std::size_t i = 0; i < count; ++i )
            field[ i ].start();

        for ( std::size_t i = 0; i < count; ++i ) {
            field[ i ].join();
            delete field[ i ].set;
        }
        delete[] field;
    }

    Test multistress() {
        SharedHashSet< int > _set;
        SharedHashSetWrapper< int > set( _set );
        set.setSize( 4 * 1024 );
        multi( &_set, 10, 1, 32 * 1024 );

        for  ( int i = 1; i < 32 * 1024; ++i ) {
            assert( set.has( i ) );
        }
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
        SharedHashSet< int > _set;
        SharedHashSetWrapper< int > set( _set );

        set.setSize( 4 * 1024 );
        par( &_set, 1, 16*1024, 8*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i ) {
            assert( set.has( i ) );
        }
    }

    Test set() {
        SharedHashSet< int > _set;
        SharedHashSetWrapper< int > set( _set );
        set.setSize( 4 * 1024 );
        for ( int i = 1; i < 32*1024; ++i ) {
            assert( !set.has( i ) );
        }

        par( &_set, 1, 16*1024, 16*1024, 32*1024 );

        for ( int i = 1; i < 32*1024; ++i ) {
            assert_eq( i, i * set.has( i ) );
        }

        for ( int i = 32*1024; i < 64 * 1024; ++i ) {
            assert( !set.has( i ) );
        }
    }
};
