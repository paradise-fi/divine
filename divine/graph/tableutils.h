// -*- C++ -*-
#include <utility>

#include <divine/utility/statistics.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/blob.h>

#ifndef DIVINE_TABLEUTILS_H
#define DIVINE_TABLEUTILS_H

namespace divine {
namespace visitor {


template< typename T >
inline bool alias( Pool &, T a, T b ) {
    return false;
}

template<> inline bool alias< Blob >( Pool &p, Blob a, Blob b ) {
    return p.alias( a, b );
}

template < typename Table, typename _Hasher, typename Statistics >
struct TableUtils
{
    typedef _Hasher Hasher;
    typedef typename Table::Item T;

    Table table;
    const Hasher& hasher() const {
        return table.hasher;
    }

    Hasher& hasher() {
        return table.hasher;
    }

    Pool& pool() {
        return hasher().pool;
    }

    const Pool& pool() const {
        return hasher().pool;
    }

    int slack() const {
        return hasher().slack;
    }

    WithID *id;

    // Retrieve node from hash table
    T fetch( T s, hash_t h, bool *had = 0 ) {
        T found = table.getHinted( s, h, had );

        if ( alias( pool(), s, found ) )
            assert( hasher().valid( found ) );

        if ( !hasher().valid( found ) ) {
            assert( !alias( pool(), s, found ) );
            if ( had )
                assert( !*had );
            return s;
        }

        return found;
    }

    // Store node in hash table
    T store( T s, hash_t h, bool * = nullptr ) {
        Statistics::global().hashadded( id->id(), memSize( s, hasher().pool ) );
        Statistics::global().hashsize( id->id(), table.size() );
        T s2 = table.insertHinted( s, h );
        setPermanent( hasher().pool, s2 );
        return s2;
    }

    bool has( T s ) { return table.has( s ); }
    bool valid( T a ) { return hasher().valid( a ); }
    hash_t hash( T s ) { return hasher().hash( s ); }
    bool equal( T a, T b ) { return hasher().equal( a, b ); }
    bool alias( T a, T b ) { return visitor::alias( pool(), a, b ); }
    void setSize( int sz ) { table.setSize( sz ); }

    TableUtils( Pool& pool, int slack ) : table( Hasher( pool, slack ) ), id( nullptr ) {}

    /* TODO: need to revision of this change */
    template< typename... Args >
    TableUtils( Pool& pool, int slack, Args&&... args ) :
        table( Hasher( pool, slack), std::forward< Args >( args )... ),
        id( nullptr )
    {}
};

}
}

#endif // DIVINE_TABLEUTILS_H
