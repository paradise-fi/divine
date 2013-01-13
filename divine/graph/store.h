// -*- C++ -*-
#include <utility>
#include <memory>

#include <divine/utility/statistics.h>
#include <divine/toolkit/sharedhashset.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/blob.h>
#include <divine/toolkit/parallel.h> // for WithID

#ifndef DIVINE_STORE_H
#define DIVINE_STORE_H

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
template< typename T > inline void setPermanent( T, bool = true ) {}

template<> inline bool permanent( Blob b ) { return b.valid() ? b.header().permanent : false; }
template<> inline void setPermanent( Blob b, bool x ) {
    if ( b.valid() )
        b.header().permanent = x;
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
    void store( T s, hash_t h, bool* ) {
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
    void setSize( int sz ) { table.setSize( sz ); }

    TableUtils() : id( nullptr ) {}
    /* TODO: need to revision of this change */
    template< typename... Args >
    TableUtils( Args&&... args ) :
        table( std::forward< Args >( args )... ),
        id( nullptr )
    {}
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


template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
           typename Statistics = NoStatistics >
struct SharedStore : TableUtils< SharedHashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    typedef typename Graph::Node T;
    typedef TableUtils< SharedHashSet< typename Graph::Node, Hasher >, Hasher, Statistics > Super;
    typedef SharedStore< Graph, Hasher, Statistics > This;

    SharedStore( Graph & ) : Super( 65536 ) {}
    SharedStore( unsigned size ) : Super( size ) {}

    void update( T s, hash_t h ) {}

    void store( T s, hash_t h, bool* had ) {
        Statistics::global().hashadded( Super::id->id(), memSize( s ) );
        Statistics::global().hashsize( Super::id->id(), Super::table.size() );
        *had = !Super::table.insertHinted( s, h );
        setPermanent( s );
    }
};

template< typename Hasher >
struct HcHasher : Hasher
{
    bool equal(Blob a, Blob b) {
        return true;
    }
};

template< typename Graph, typename Hasher, typename Statistics >
struct HcStore : public TableUtils< HashSet< typename Graph::Node, HcHasher<Hasher> >, HcHasher<Hasher>, Statistics >
{
    static_assert( wibble::TSame< typename Graph::Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );
    typedef TableUtils< HashSet< typename Graph::Node, HcHasher<Hasher> >, HcHasher<Hasher>, Statistics > Utils;
    typedef typename Graph::Node Node;

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
//        Statistics::global().hashadded( this->id , memSize( stub ) );
//        Statistics::global().hashsize( this->id, this->table.size() );
        std::copy( s.data(), s.data() + stub.size(), stub.data() );
        this->table.insertHinted( stub, h );
        assert( this->equal( s, stub ) );
    }

    void update( Blob s, hash_t h ) {
        // update state information in hashtable
        Blob stub = this->table.getHinted( s, h, NULL );
        assert( this->valid( stub ) );
        std::copy( s.data(), s.data() + stub.size(), stub.data() );
    }

    template< typename W >
    int owner( W &w, Node s, hash_t hint = 0 ) {
        if ( hint )
            return hint % w.peers();
        else
            return this->hash( s ) % w.peers();
    }
};

}
}
#endif
