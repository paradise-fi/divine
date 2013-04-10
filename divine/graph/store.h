// -*- C++ -*-
#include <utility>
#include <memory>
#include <tuple>

#include <divine/utility/statistics.h>
#include <divine/toolkit/sharedhashset.h>
#include <divine/toolkit/treecompressedhashset.h>
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

template< typename T > inline bool permanent( Pool& pool, T ) { return false; }
template< typename T > inline void setPermanent( Pool& pool, T, bool = true ) {}

template<> inline bool permanent( Pool& pool, Blob b ) {
    return b.valid() ? pool.header( b ).permanent : false;
}
template<> inline void setPermanent( Pool& pool, Blob b, bool x ) {
    if ( b.valid() )
        pool.header( b ).permanent = x;
}

template < typename Self, typename Table, typename _Hasher, typename _Statistics >
struct TableUtils
{
    typedef _Hasher Hasher;
    typedef _Statistics Statistics;
    typedef typename Table::Item T;

    Self& self() {
        return *static_cast< Self* >( this );
    }

    Table& table() {
        return self().table();
    }

    Hasher &hasher() {
        return table().hasher;
    }

    WithID *id;

    // Retrieve node from hash table
    std::tuple< T, bool > fetch( T s, hash_t h ) {
        T found;
        bool had;
        std::tie( found, had ) = table().getHinted( s, h );

        if ( !had ) {
            assert( !hasher().valid( found ) );
            assert( !alias( s, found ) );
            return std::make_tuple( s, false );
        }
        assert( hasher().valid( found ) );
        return std::make_tuple( found, true );
    }

    // Store node in hash table
    bool store( T s, hash_t h ) {
        Statistics::global().hashadded( id->id(), memSize( s, hasher().pool ) );
        Statistics::global().hashsize( id->id(), table().size() );
        table().insertHinted( s, h );
        setPermanent( s );
    }

    bool has( T s ) { return table().has( s ); }

    struct iterator {
        Table *t;
        size_t n;
        void bump() {
            while ( n < t->size() && !t->hasher.valid( (*t)[ n ] ) )
                ++ n;
        }

        iterator &operator++() { ++n; bump(); return *this; }
        bool operator!=( const iterator &o ) const { return o.t != t || o.n != n; }
        T operator*() { return (*t)[ n ]; }
        iterator( Table &t, int k ) : t( &t ), n( k ) { bump(); }
    };

    iterator begin() { return iterator( table(), 0 ); }
    iterator end() { return iterator( table(), table().size() ); }

    bool valid( T a ) { return hasher().valid( a ); }
    hash_t hash( T s ) { return hasher().hash( s ); }
    bool equal( T a, T b ) { return hasher().equal( a, b ); }
    bool alias( T a, T b ) { return visitor::alias( a, b ); }
    void setSize( size_t sz ) { table().setSize( sz ); }
    void maxSize( size_t sz ) { table().m_maxsize = sz; }
    size_t size() { return table().size(); }

    T operator[]( unsigned index ) { return table()[index]; }

    TableUtils() : id( nullptr ) {}
};

template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
           typename Statistics = NoStatistics >
struct PartitionedStore : TableUtils< PartitionedStore< Graph, Hasher, Statistics >, HashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    typedef typename Graph::Node T;
    typedef HashSet< T, Hasher > Table;
    typedef PartitionedStore< Graph, Hasher, Statistics > This;

    Table _table;

    PartitionedStore( Graph &g ) :
        _table( Hasher( g.base().alloc.pool() ) )
    {}
    PartitionedStore( Graph &g, This * = nullptr ) :
        _table( Hasher( g.base().alloc.pool() ) )
    {}

    Table& table() {
        return _table;
    }

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
struct SharedStore : TableUtils< SharedStore< Graph, Hasher, Statistics >, SharedHashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    typedef typename Graph::Node T;
    typedef SharedHashSet< T, Hasher > Table;
    typedef std::shared_ptr< Table > TablePtr;
    typedef SharedStore< Graph, Hasher, Statistics > This;
    typedef TableUtils< This, Table, Hasher, Statistics > Super;

    TablePtr _table;

    SharedStore( Graph &g, This *master = nullptr ) {
        if ( master )
            _table = master->_table;
        else
            _table = std::make_shared< Table >( Hasher( g.base().alloc.pool() ) );
    }
    SharedStore() :
        _table( std::make_shared< Table >() )
    {}

    Table& table() {
        return *_table;
    }

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
    HcHasher( Pool& pool ) : Hasher( pool )
    { }

    bool equal(Blob a, Blob b) {
        return true;
    }
};

template< typename Graph, typename Hasher, typename Statistics >
struct HcStore : public TableUtils< HcStore< Graph, Hasher, Statistics >,
                                    HashSet< typename Graph::Node, HcHasher< Hasher > >,
                                    HcHasher< Hasher >, Statistics >
{
    static_assert( wibble::TSame< typename Graph::Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );
    typedef typename Graph::Node Node;
    typedef HashSet< typename Graph::Node, HcHasher< Hasher > > Table;
    typedef HcStore< Graph, Hasher, Statistics > This;
    typedef TableUtils< This, Table, HcHasher< Hasher >, Statistics > Utils;

    Graph &m_graph;
    Table _table;

    HcStore( Graph &g, This * ) :
        m_graph( g ),
        _table( HcHasher< Hasher >( g.base().alloc.pool() ) )
    {}

    Table& table() {
        return _table;
    }
    Pool& pool() {
        return m_graph.base().alloc.pool();
    }

    std::tuple< Blob, bool > fetch( Blob s, hash_t h ) {
        Blob found;
        bool had;
        std::tie( found, had ) = Utils::fetch( s, h );

        if ( !alias( s, found ) ) {
            // copy saved state information
            std::copy( pool().data( found ),
                       pool().data( found ) + pool().size( found ),
                       pool().data( s ) );
            return std::make_tuple( s, false );
        }
        return std::make_tuple( found, true );
    }

    bool store( Blob s, hash_t h ) {
        // store just a stub containing state information
        Blob stub = m_graph.base().alloc.new_blob( 0 );
//        Statistics::global().hashadded( this->id , memSize( stub ) );
//        Statistics::global().hashsize( this->id, this->table.size() );
        std::copy( pool().data( s ),
                   pool().data( s ) + pool().size( stub ),
                   pool().data( stub ) );
        table().insertHinted( stub, h );
        assert( this->equal( s, stub ) );
        return true;// same as in TableUtils::fetch
    }

    void update( Blob s, hash_t h ) {
        // update state information in hashtable
        Blob stub;
        bool had;
        std::tie( stub, had ) = table().getHinted( s, h );
        assert( this->valid( stub ) && had );
        std::copy( pool().data( s ),
                   pool().data( s ) + pool().size( stub ),
                   pool().data( stub ) );
    }

    template< typename W >
    int owner( W &w, Node s, hash_t hint = 0 ) {
        if ( hint )
            return hint % w.peers();
        else
            return this->hash( s ) % w.peers();
    }
};

template < template < template < typename, typename > class, typename, typename > class Table,
    template < typename, typename > class BaseTable, typename Graph, typename Hasher,
    typename Statistics >
struct CompressedStore : public TableUtils< CompressedStore< Table, BaseTable, Graph,
    Hasher, Statistics >, Table< BaseTable, typename Graph::Node, Hasher >,
    Hasher, Statistics >
{
    static_assert( wibble::TSame< typename Graph::Node, Blob >::value,
            "CompressedStore can only work with Blob nodes" );
    typedef TableUtils< CompressedStore< Table, BaseTable, Graph, Hasher, Statistics >,
            Table< BaseTable, typename Graph::Node, Hasher >, Hasher, Statistics > Utils;
    typedef typename Graph::Node Node;

    template< typename... Args >
    CompressedStore( Graph& g, int slack, Args&&... args ) :
        Utils( g.base().alloc.pool(), slack, std::forward< Args >( args )... )
    { }

    int slack() {
        return this->table.slack();
    }

    Pool& pool() {
        return this->hasher().pool;
    }

    Node fetch( Node node, hash_t h, bool* had = nullptr ) {
        Node found = Utils::fetch( node, h, had );

        if ( !alias( node, found ) ) {
            pool().copyTo( found, node, slack() );
            return node;
        }
        return found;
    }

    void update( Node node, hash_t h ) {
        // update state information in hashtable
        Node found = this->table.getHinted( node, h, NULL );
        assert( this->valid( found ) );
        if ( !alias( node, found ) )
            pool().copyTo( node, found, slack() );
    }

    void store( Node node, hash_t h, bool* = nullptr ) {
        Statistics::global().hashadded( this->id->id(),
                memSize( node, pool() ) );
        Statistics::global().hashsize( this->id->id(), this->table.size() );
        Node n = this->table.insertHinted( node, h );
        if ( alias( n, node ) )
            setPermanent ( pool(), node );
    }

    void setSize( int sz ) { } // TODO

    template< typename W >
    int owner( W &w, Node s, hash_t hint = 0 ) {
        if ( hint )
            return hint % w.peers();
        else
            return this->hash( s ) % w.peers();
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
    TreeCompressedStore( Graph& g, int slack, Args&&... args ) :
        Base( g, slack, 16, std::forward< Args >( args )... )
    { }
};

}

}
#endif
