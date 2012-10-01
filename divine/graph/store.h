// -*- C++ -*-
#ifndef DIVINE_STORE_H
#define DIVINE_STORE_H

#include <divine/utility/statistics.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/blob.h>
#include <divine/toolkit/parallel.h> // for WithID

namespace divine {
namespace visitor {


template< typename T >
inline bool alias( T a, T b ) {
    return false;
}

template<> inline bool alias< Blob >( Blob a, Blob b ) {
    return a.ptr == b.ptr;
}

template< typename T > inline bool permanent( T ) { return false; }
template< typename T > inline void setPermanent( T ) {}

template<> inline bool permanent( Blob b ) { return b.header().permanent; }
template<> inline void setPermanent( Blob b ) {
    if ( b.valid() )
        b.header().permanent = 1;
}

template < typename Table, typename _Hasher, typename Statistics >
struct TableUtils
{
    typedef _Hasher Hasher;
    typedef typename Table::Item T;

    Table table;
    Hasher &hasher() {
        return table.hasher;
    }

    WithID *id;

    // Retrieve node from hash table
    T fetch( T s, hash_t h, bool *had = 0 ) {
        T found = table.getHinted( s, h, had );

        if ( alias( s, found ) )
            assert( hasher().valid( found ) );

        if ( !hasher().valid( found ) ) {
            assert( !alias( s, found ) );
            if ( had )
                assert( !*had );
            return s;
        }

        return found;
    }

    // Store node in hash table
    void store( T s, hash_t h ) {
        Statistics::global().hashadded( id->id(), memSize( s ) );
        Statistics::global().hashsize( id->id(), table.size() );
        table.insertHinted( s, h );
        setPermanent( s );
    }

    bool has( T s ) { return table.has( s ); }
    bool valid( T a ) { return hasher().valid( a ); }
    hash_t hash( T s ) { return hasher().hash( s ); }
    bool equal( T a, T b ) { return hasher().equal( a, b ); }
    bool alias( T a, T b ) { return visitor::alias( a, b ); }

    TableUtils() : id( 0 ) {}
};

template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
           typename Statistics = NoStatistics >
struct PartitionedStore : TableUtils< HashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    typedef typename Graph::Node T;

    PartitionedStore( Graph & ) {}
    void update( T s, hash_t h ) {}

    template< typename W >
    int owner( W &w, T s, hash_t hint = 0 ) {
        if ( hint )
            return hint % w.peers();
        else
            return this->hash( s ) % w.peers();
    }
};

#if 0
template< typename Graph, typename Hasher, typename Statistics >
struct HcStore : public TableUtils< HashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    static_assert( wibble::TSame< typename Graph::Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );
    typedef TableUtils< Set< Graph >, Hasher, Statistics > Utils;

    Graph &m_graph;

    HcStore( Graph &g ) :
        m_graph( g ) {}

    Blob fetch( Blob s, hash_t h, bool* had = 0 ) {
        Blob found = Utils::fetch( s, h, had );

        if ( !alias( s, found ) ) {
            // copy saved state information
            std::copy( found.data(), found.data() + found.size(), s.data() );
            return s;
        }
        return found;
    }

    void store( Blob s, hash_t h ) {
        // store just a stub containing state information
        Blob stub = m_graph.base().alloc.new_blob( 0 );
        Statistics::global().hashadded( this->id , memSize( stub ) );
        Statistics::global().hashsize( this->id, this->table.size() );
        std::copy( s.data(), s.data() + stub.size(), stub.data() );
        this->seen().insertHinted( stub, h );
        assert( this->seen().equal( s, stub ) );
    }

    void update( Blob s, hash_t h ) {
        // update state information in hashtable
        Blob stub = this->table.getHinted( s, h, NULL );
        assert( this->table.valid( stub ) );
        std::copy( s.data(), s.data() + stub.size(), stub.data() );
    }

};
#endif

}
}
#endif
