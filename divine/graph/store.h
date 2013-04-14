// -*- C++ -*-
#include <utility>
#include <memory>
#include <tuple>

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
        Statistics::global().hashadded( id->id(), memSize( s ) );
        Statistics::global().hashsize( id->id(), table().size() );
        table().insertHinted( s, h );
        setPermanent( s );
        return true;// thus we call this only if we could not find $s in table
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

    TableUtils() :
        id( nullptr )
    {}
};

template < typename Graph, typename Hasher = default_hasher< typename Graph::Node >,
           typename Statistics = NoStatistics >
struct PartitionedStore : TableUtils< PartitionedStore< Graph, Hasher, Statistics >, HashSet< typename Graph::Node, Hasher >, Hasher, Statistics >
{
    typedef typename Graph::Node T;
    typedef HashSet< T, Hasher > Table;
    typedef PartitionedStore< Graph, Hasher, Statistics > This;

    Table _table;

    PartitionedStore() :
        _table( Hasher() )
    {}
    PartitionedStore( Graph &, This * = nullptr ) :
        _table( Hasher() )
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

    SharedStore( Graph &, This *master = nullptr ) {
        if ( master )
            _table = master->_table;
        else
            _table = std::make_shared< Table >();
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
        m_graph( g ), _table( HcHasher< Hasher >() )
    {}

    Table& table() {
        return _table;
    }

    std::tuple< Blob, bool > fetch( Blob s, hash_t h ) {
        Blob found;
        bool had;
        std::tie( found, had ) = Utils::fetch( s, h );

        if ( !alias( s, found ) ) {
            // copy saved state information
            std::copy( found.data(), found.data() + found.size(), s.data() );
            return std::make_tuple( s, false );
        }
        return std::make_tuple( found, true );
    }

    bool store( Blob s, hash_t h ) {
        // store just a stub containing state information
        Blob stub = m_graph.base().alloc.new_blob( 0 );
//        Statistics::global().hashadded( this->id , memSize( stub ) );
//        Statistics::global().hashsize( this->id, this->table.size() );
        std::copy( s.data(), s.data() + stub.size(), stub.data() );
        table().insertHinted( stub, h );
        assert( this->equal( s, stub ) );
        return true;// same as in TableUtils::fetch
    }

    void update( Blob s, hash_t h ) {
        // update state information in hashtable
        Blob stub = table().getHinted( s, h, NULL );
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
