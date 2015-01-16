// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#if GEN_EXPLICIT || ALG_EXPLICIT

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <divine/explicit/explicit.h>

#ifndef DIVINE_COMPACT_TRANSPOSE_H
#define DIVINE_COMPACT_TRANSPOSE_H

namespace divine {
namespace dess {

template< typename EdgeSpec >
void transpose( DataBlock &from, DataBlock &to ) {
#if 0
    static_assert( std::is_same< typename std::result_of<
                dectype( &EdgeSpec::index() )( EdgeSpec ) >::type,
                int64_t & >::value,
            "Invalid EdgeSpec passed (expected method `int64_t &index()'" );
#endif
    ASSERT_EQ( from.count(), to.count() );

    int64_t *counts = to.lowLevelIndices();
    std::memset( counts, 0, from.count() * sizeof( int64_t ) );

    // count outdegree
    ASSERT_EQ( from.size( 0 ) % sizeof( EdgeSpec ), 0 );
    counts[ 0 ] = from.size( 0 ) / sizeof( EdgeSpec ); // intials
    EdgeSpec *revEdges = reinterpret_cast< EdgeSpec * >( from[ 1 ] );
    EdgeSpec *revEdgesEnd = reinterpret_cast< EdgeSpec * >( from[ from.count() ] );
    for ( EdgeSpec *p = revEdges; p < revEdgesEnd; ++p )
        ++counts[ p->index() ];

    auto inserter = to.inserter();

    // copy initials
    inserter.emplace( counts[ 0 ] * sizeof( EdgeSpec ), [&]( char *ptr, int64_t size ) {
            std::copy( from[ 0 ], from[ 1 ], ptr );
        } );

    // distribute space as needed
    for ( int64_t i = 1; i < to.count(); ++i )
        inserter.emplace( counts[ i ] * sizeof( EdgeSpec ), []( char *ptr, int64_t size ) {
            std::memset( ptr, 0, size );
        } );

    // transpose
    for ( int i = 1; i < from.count(); ++i )
        from.map< EdgeSpec >( i )( [ &to, i ]( EdgeSpec *edges, int64_t size ) {
            for ( int64_t j = 0; j < size; ++j )
                to.map< EdgeSpec >( edges[ j ].index() )
                    ( [ edges, i, j ]( EdgeSpec *de, int64_t size ) {
                        auto last = de + size - 1;
                        for ( ; de->index(); ++de )
                            ASSERT_LEQ( de, last );
                        static_cast< void >( last );
                        de->index() = i;
                        de->label() = edges[ j ].label();
                    } );
        } );
}

}
}

#endif // DIVINE_COMPACT_TRANSPOSE_H
#endif
