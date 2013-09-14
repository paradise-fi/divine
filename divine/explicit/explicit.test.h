// -*- C++ -*- (c) 2013 Vladimír Štill

#include <divine/explicit/explicit.h>

using namespace divine::dess;

struct TestDataBlock {
    Test empty() {
        DataBlock empty;
    }

    void inserterT() { /* this was dropped from code as it was not used
        char *data = new char[ 100 * sizeof( int64_t ) + 100 * 100 ];
        DataBlock block( 100, data );

        auto inserter = block.inserter< int64_t >();
        for ( int i = 0; i < 100; ++i )
            assert_eq( inserter.emplace( i ), i );
        for ( int i = 0; i < 100; ++i )
            assert_eq( block.get< int64_t >( i ), i );
        for ( int64_t i = 0; i < 100; ++i )
            block.map( i )( [ i ]( char *data, int64_t size ) -> void {
                assert_eq( int64_t( sizeof( int64_t ) ), size );
                assert_eq( *reinterpret_cast< int64_t *>( data ), i );
            } );*/
    }

    Test inserter() {
        char *data = new char[ 100 * sizeof( int64_t ) + 100 * 100 ];
        DataBlock block( 100, data );

        auto inserter = block.inserter();
        for ( int i = 0; i < 100; ++i )
            assert_eq( inserter.emplace( std::max( size_t( i ), sizeof( int64_t ) ),
                [ i ]( char *data, int64_t size ) -> int64_t {
                    *reinterpret_cast< int64_t * >( data ) = i;
                    return *reinterpret_cast< int64_t * >( data );
                } ), i );
        for ( int i = 0; i < 100; ++i )
            assert_eq( block.get< int64_t >( i ), i );
        for ( int64_t i = 0; i < 100; ++i )
            block.map( i )( [ i ]( char *data, int64_t size ) -> void {
                assert_eq( std::max( int64_t( i ), int64_t( sizeof( int64_t ) ) ), size );
                assert_eq( *reinterpret_cast< int64_t *>( data ), i );
            } );
    }
};
