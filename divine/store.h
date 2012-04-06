#ifndef DIVINE_STORE_H
#define DIVINE_STORE_H

#include <divine/statistics.h>
#include <divine/hashset.h>
#include <divine/pool.h>
#include <divine/blob.h>

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

template < typename Node, typename Seen, typename Statistics >
struct StoreBase {
    Seen *m_seen;
    int id;

    StoreBase( Seen *s ) : m_seen( s ), id( 0 ) {
        if ( !m_seen )
            m_seen = new Seen();
    }

    Seen &seen() {
        return *m_seen;
    }

    bool has( Node s ) {
        return seen().has( s );
    }

    hash_t hash( Node s ) {
        return seen().hash( s );
    }

    static bool isSame( Node a, Node b ) {
        return alias( a, b );
    }

    // Retrieve node from hash table
    Node fetchNode( Node s, hash_t h, bool* had ) {
        Node found = seen().getHinted( s, h, had );

        if ( alias( s, found ) )
            assert( seen().valid( found ) );

        if ( !seen().valid( found ) ) {
            assert( !alias( s, found ) );
            assert( !*had );
            return s;
        }

        return found;
    }

    // Store node in hash table
    void storeNode( Node s, hash_t h ) {
        Statistics::global().hashadded( id , memSize( s ) );
        Statistics::global().hashsize( id, seen().size() );
        seen().insertHinted( s, h );
        setPermanent( s );
    }

    // Notify hash table that algorithm-specific information about a node have changed
    void notifyUpdate( Node s, hash_t h ) {}
};


template < typename Node, typename Graph, typename Seen, typename Statistics, typename Dummy = wibble::Unit >
struct Store : public StoreBase< Node, Seen, Statistics> {
    typedef StoreBase< Node, Seen, Statistics > base;

    Store( Graph &g, Seen *s, bool hc = false ) : base( s ) {}
};


template < typename Graph, typename Seen, typename Statistics >
struct Store < Blob, Graph, Seen, Statistics, typename Graph::HasAllocator > : public StoreBase< Blob, Seen, Statistics > {
    typedef Blob Node;
    typedef StoreBase< Node, Seen, Statistics > Base;

    Graph &m_graph;
    bool hashComp;

    Store( Graph &g, Seen *s, bool _hashComp = false ) :
        Base( s ), m_graph( g ), hashComp( _hashComp ) {}

    Node fetchNode( Node s, hash_t h, bool* had ) {
        Node found = Base::fetchNode( s, h, had );

        if ( hashComp && !alias( s, found ) ) {
            // copy saved state information
            std::copy( found.data(), found.data() + found.size(), s.data() );
            return s;
        }
        return found;
    }

    void storeNode( Node s, hash_t h ) {
        if ( hashComp ) {
            // store just a stub containing state information
            Node stub = unblob< Node >( m_graph.base().alloc.new_blob( 0 ) );
            Statistics::global().hashadded( this->id , memSize( stub ) );
            Statistics::global().hashsize( this->id, this->seen().size() );
            std::copy( s.data(), s.data() + stub.size(), stub.data() );
            this->seen().insertHinted( stub, h );
            assert( this->seen().equal( s, stub ) );
        } else {
            Base::storeNode( s, h );
        }
    }

    void notifyUpdate( Node s, hash_t h ) {
        if ( hashComp ) {
            // update state information in hashtable
            Node stub = this->seen().getHinted( s, h, NULL );
            assert( this->seen().valid( stub ) );
            std::copy( s.data(), s.data() + stub.size(), stub.data() );
        }
        Base::notifyUpdate( s, h );
    }

};

}
}
#endif
