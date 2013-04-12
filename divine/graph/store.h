// -*- C++ -*-
#include <utility>
#include <memory>

#include <divine/utility/statistics.h>
#include <divine/toolkit/sharedhashset.h>
#include <divine/toolkit/treecompressedhashset.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/blob.h>
#include <divine/toolkit/parallel.h> // for WithID
#include <divine/graph/tableutils.h>

#ifndef DIVINE_STORE_H
#define DIVINE_STORE_H

namespace divine {
namespace visitor {

/* Global utility functions independent of store type
 */

template < typename Store >
inline typename Store::VertexId max( Store& st, typename Store::VertexId a,
        typename Store::VertexId b )
{
    if ( st.compareId( a, b ) > 0 )
        return a;
    return b;
}

template < typename Store >
inline bool equalId( Store& st, typename Store::VertexId a, typename Store::VertexId b ) {
    return st.compareId( a, b ) == 0;
}



template < typename Utils >
struct StoreCommon {
    typedef typename Utils::Hasher Hasher;

  protected: // we don't want algoritms to mess with table directly
    Utils m_base;

  public:
    template < typename... Args >
    StoreCommon( Args&&... args ) : m_base( std::forward< Args >( args )... )
    { }

    const typename Utils::Hasher& hasher() const {
        return m_base.hasher();
    }

    typename Utils::Hasher& hasher() {
        return m_base.hasher();
    }

    hash_t hash( typename Utils::T node ) const {
        return hasher().hash( node );
    }

    void setId( WithID& id ) {
        m_base.id = &id;
    }

    Pool& pool() {
        return m_base.pool();
    }

    const Pool& pool() const {
        return m_base.pool();
    }

    int slack() const {
        return m_base.slack();
    }

    bool alias( typename Utils::T n1, typename Utils::T n2 ) {
        return m_base.alias( n1, n2 );
    }

    bool valid( typename Utils::T n ) {
        return m_base.valid( n );
    }

    bool equal( typename Utils::T m, typename Utils::T n ) {
        return m_base.equal( m, n );
    }

  protected:
    template < typename Select, typename VertexId >
    int _compareId( Select select, VertexId a, VertexId b ) {
        if ( !permanent( this->pool(), select( a ) ) )
            return 0 - permanent( this->pool(), select( b ) );

        if ( !permanent( this->pool(), select( b ) ) )
            return 1;

        return this->pool().compare( select( a ), select( b ) );
    }
};

template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
           typename Statistics = NoStatistics >
struct PartitionedStore
    : public StoreCommon< TableUtils< HashSet< typename Graph::Node, Hasher >,
        Hasher, Statistics > >
{
    typedef typename Graph::Node Node;
    typedef TableUtils< HashSet< Node, Hasher >, Hasher, Statistics > Base;
    typedef PartitionedStore< Graph, Hasher, Statistics > This;

    static const bool CanFetchByHandle = true;

    struct VertexId {
        Node node;
        explicit VertexId( Node n ) : node( n ) { }
        VertexId() : node() { }

        uintptr_t weakId() {
            return node.valid() ?
                reinterpret_cast< uintptr_t >( node.ptr )
                : 0;
        }

        template < typename Extension >
        Extension& extension( Pool& p, int n = 0 ) {
            return p.template get< Extension >( node, n );
        }

        template < typename BS >
        friend bitstream_impl::base< BS >& operator<<(
                bitstream_impl::base< BS >& bs, VertexId vi )
        {
            return bs << vi.node;
        }
        template < typename BS >
        friend bitstream_impl::base< BS >& operator>>(
                bitstream_impl::base< BS >& bs, VertexId& vi )
        {
            return bs >> vi.node;
        }
    };

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
        T found = table().getHinted( s, h, had );

        if ( !had ) {
            assert( !hasher().valid( found ) );
            assert( !alias( s, found ) );
            return std::make_tuple( s, false );
        }

        return found;
    }

    // Store node in hash table
    bool store( T s, hash_t h ) {
        Statistics::global().hashadded( id->id(), memSize( s, hasher().pool ) );
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

    TableUtils( Pool& pool, int slack ) : table( Hasher( pool, slack ) ), id( nullptr ) {}

    /* TODO: need to revision of this change */
    template< typename... Args >
    TableUtils( Pool& pool, int slack, Args&&... args ) :
        table( Hasher( pool, slack), std::forward< Args >( args )... ),
        id( nullptr )
    {}
};

template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
           typename Statistics = NoStatistics >
struct PartitionedStore : TableUtils< HashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    typedef typename Graph::Node T;
    typedef TableUtils< HashSet< typename Graph::Node, Hasher >, Hasher, Statistics > Base;

    PartitionedStore( Graph& g, int slack ) :
            Base( g.base().alloc.pool(), slack )
    { }
    void update( T s, hash_t h ) {}

    template< typename W >
    int owner( W &w, Node n, hash_t hint = 0 ) const {
        if ( hint )
            return hint % w.peers();
        else
            return this->hash( n ) % w.peers();
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return owner( w, s.node, hint );
    }

    template < typename W >
    int owner( W &w, VertexId h ) const {
        int own = this->hash( h.node ) % w.peers();
        return own;
    }

    using StoreCommon< Base >::valid;

    bool valid( Vertex v ) {
        return this->valid( v.node );
    }

    bool valid( VertexId vi ) {
        return this->valid( vi.node );
    }

    int compareId( VertexId a, VertexId b ) {
        return this->_compareId( []( VertexId x ) { return x.node; }, a, b );
    }

    class Iterator : std::iterator< std::input_iterator_tag, VertexId > {
        size_t i;
        This& store;

      public:
        Iterator( This& store, size_t i ) : i( i ), store( store ) { }

        VertexId operator*() {
            return VertexId( store.m_base.table[ i ] );
        }

        Iterator& operator++() {
            ++i;
            return *this;
        }

        bool operator==( Iterator b ) {
            return i == b.i;
        }

        bool operator!=( Iterator b ) {
            return i != b.i;
        }
    };

    Iterator begin() {
        return Iterator( *this, 0 );
    }

    Iterator end() {
        return Iterator( *this, this->m_base.table.size() );
    }
};


template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
           typename Statistics = NoStatistics >
struct SharedStore : TableUtils< SharedHashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    typedef typename Graph::Node T;
    typedef SharedHashSet< T, Hasher > Table;
    typedef std::shared_ptr< Table > TablePtr;
    typedef SharedStore< Graph, Hasher, Statistics > This;
    typedef TableUtils< This, Table, Hasher, Statistics > Super;

    TablePtr _table;

    SharedStore( Graph & ) : Super( 65536 ) {}
    SharedStore( unsigned size ) : Super( size ) {}

    void update( T s, hash_t h ) {}

    void maxSize( size_t ) {}

    bool store( T s, hash_t h ) {
        Statistics::global().hashadded( Super::id->id(), memSize( s ) );
        Statistics::global().hashsize( Super::id->id(), table().size() );
        bool inserted;
        std::tie( std::ignore, inserted ) = table().insertHinted( s, h );
        setPermanent( s );
        return inserted;
    }

    template< typename W >
    int owner( W &w, T s, hash_t hint = 0 ) {
        return w.id();
    }
};

template< typename Hasher >
struct HcHasher : Hasher
{
    HcHasher( Pool& pool, int slack ) : Hasher( pool, slack )
    { }

    bool equal(Blob a, Blob b) {
        return true;
    }
};

template< typename Graph, typename Hasher, typename Statistics >
struct HcStore : public TableUtils< HashSet< typename Graph::Node, HcHasher<Hasher> >, HcHasher<Hasher>, Statistics >
{
    static_assert( wibble::TSame< typename Graph::Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );
    typedef typename Graph::Node Node;

    Graph &m_graph;
    Table _table;

    Pool& pool() {
        return m_graph.base().alloc.pool();
    }

    Blob fetch( Blob s, hash_t h, bool* had = 0 ) {
        Blob found = Utils::fetch( s, h, had );

        if ( !alias( s, found ) ) {
            // copy saved state information
            std::copy( pool().data( found ),
                       pool().data( found ) + pool().size( found ),
                       pool().data( s ) );
            return s;
        }
        return found;
    }

    void store( Blob s, hash_t h, bool * = nullptr ) {
        // store just a stub containing state information
        Blob stub = m_graph.base().alloc.new_blob( int( sizeof( hash_t ) ) );
//        Statistics::global().hashadded( this->id , memSize( stub ) );
//        Statistics::global().hashsize( this->id, this->table.size() );
        std::copy( pool().data( s ),
                   pool().data( s ) + pool().size( stub ),
                   pool().data( stub ) );
        this->table.insertHinted( stub, h );
        assert( this->equal( s, stub ) );
    }

    void update( Blob s, hash_t h ) {
        // update state information in hashtable
        Blob stub = table().getHinted( s, h, NULL );
        assert( this->valid( stub ) );
        this->pool().copyTo( s, stub, this->slack() );
    }

    bool has( Node node ) {
        return this->m_base.has( node );
    }

    void setSize( int ) { } // TODO

    template< typename W >
    int owner( W &w, Node n, hash_t hint = 0 ) const {
        if ( hint )
            return hint % w.peers();
        else
            return this->hash( n ) % w.peers();
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return stubHash( s.hash ) % w.peers();
    }

    template< typename W >
    int owner( W &w, VertexId h ) const {
        return stubHash( h.hash ) % w.peers();
    }

    using StoreCommon< Utils >::valid;

    bool valid( Vertex v ) {
        return this->valid( v.node );
    }

    bool valid( VertexId vi ) {
        return this->valid( vi.hash );
    }

    int compareId( VertexId a, VertexId b ) {
        return this->_compareId( []( VertexId x ) { return x.hash; }, a, b );
    }

    class Iterator : std::iterator< std::input_iterator_tag, VertexId > {
        size_t i;
        This& store;

      public:
        Iterator( This& store, size_t i ) : i( i ), store( store ) { }

        VertexId operator*() {
            return VertexId( store.m_base.table[ i ] );
        }

        Iterator& operator++() {
            ++i;
            return *this;
        }

        bool operator==( Iterator b ) {
            return i == b.i;
        }

        bool operator!=( Iterator b ) {
            return i != b.i;
        }
    };

    Iterator begin() {
        return Iterator( *this, 0 );
    }

    Iterator end() {
        return Iterator( *this, this->m_base.table.size() );
    }
};


template < template < template < typename, typename > class, typename, typename > class Table,
    template < typename, typename > class BaseTable, typename Graph, typename Hasher,
    typename Statistics >
struct CompressedStore : public TableUtils< Table< BaseTable, typename Graph::Node,
    Hasher >, Hasher, Statistics >
{
    static_assert( wibble::TSame< typename Graph::Node, Blob >::value,
            "CompressedStore can only work with Blob nodes" );
    typedef TableUtils< Table< BaseTable, typename Graph::Node, Hasher >,
            Hasher, Statistics > Utils;
    typedef typename Graph::Node Node;

    template< typename... Args >
    CompressedStore( Graph& g, int slack, This *, Args&&... args ) :
        StoreCommon< Utils >( g.base().alloc.pool(), slack, std::forward< Args >( args )... )
    { }

    Vertex fetch( Node node, hash_t h, bool* had = nullptr ) {
        Node found = this->m_base.fetch( node, h, had );
        if ( !this->m_base.valid( found ) )
            return Vertex();

        if ( !this->m_base.alias( node, found ) ) {
            this->pool().copyTo( found, node, this->slack() );
            return Vertex( node, found );
        }
        return Vertex( found, Node() );
    }

    void update( Node node, hash_t h ) {
        // update state information in hashtable
        Node found = this->m_base.table.getHinted( node, h, NULL );
        assert( this->valid( found ) );
        assert( !alias( node, found ) );
        this->pool().copyTo( node, found, this->slack() );
    }

    Vertex store( Node node, hash_t h, bool* = nullptr ) {
        Statistics::global().hashadded( this->m_base.id->id(),
                memSize( node, this->pool() ) );
        Statistics::global().hashsize( this->m_base.id->id(), this->m_base.table.size() );
        Node n = this->m_base.table.insertHinted( node, h );
        setPermanent( this->pool(), n );
        return Vertex( node, n );
    }

    void setSize( int ) { } // TODO

    template< typename W >
    int owner( W &w, Node n, hash_t hint = 0 ) const {
        if ( hint )
            return hint % w.peers();
        else
            return this->hash( n ) % w.peers();
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return owner( w, s.node, hint );
    }

    using StoreCommon< Utils >::valid;

    bool valid( Vertex v ) {
        return this->valid( v.node );
    }

    bool valid( VertexId vi ) {
        return this->valid( vi.compressed );
    }

    int compareId( VertexId a, VertexId b ) {
        return this->_compareId( []( VertexId x ) { return x.compressed; }, a, b );
    }

    class Iterator : std::iterator< std::input_iterator_tag, VertexId > {
        size_t i;
        This& store;

      public:
        Iterator( This& store, size_t i ) : i( i ), store( store ) { }

        VertexId operator*() {
            return VertexId( store.m_base.table[ i ] );
        }

        Iterator& operator++() {
            ++i;
            return *this;
        }

        bool operator==( Iterator b ) {
            return i == b.i;
        }

        bool operator!=( Iterator b ) {
            return i != b.i;
        }
    };

    Iterator begin() {
        return Iterator( *this, 0 );
    }

    Iterator end() {
        return Iterator( *this, this->m_base.table.size() );
    }

};


template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
          typename Statistics = NoStatistics >
struct TreeCompressedStore : public CompressedStore< TreeCompressedHashSet,
        HashSet, Graph, Hasher, Statistics >
{
    typedef CompressedStore< TreeCompressedHashSet, HashSet, Graph, Hasher,
              Statistics > Base;

    template< typename... Args >
    TreeCompressedStore( Graph& g, int slack, This *, Args&&... args ) :
        Base( g, slack, 16, std::forward< Args >( args )... )
    { }

    using Base::owner;

    template< typename W >
    int owner( W &w, VertexId h ) {
        return ( this->m_base.table.header( h.compressed ).fork
                ? this->m_base.table.rootFork( h.compressed )->hash
                : this->m_base.table.m_roots.hasher.hash( h.compressed ) ) % w.peers();
    }
};

}
}
#endif
