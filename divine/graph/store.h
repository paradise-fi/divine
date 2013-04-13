// -*- C++ -*-
#include <utility>
#include <memory>
#include <iterator>
#include <tuple>

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

template < typename Pool, typename T >
typename std::enable_if< !std::is_fundamental< T >::value >::type poolFree( Pool p, T x ) {
    return p.free( x );
}

template < typename Pool, typename T >
typename std::enable_if< std::is_fundamental< T >::value >::type poolFree( Pool, T ) {}

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

template < typename Store >
inline bool identity( Store& st, typename Store::Vertex a, typename Store::Vertex b ) {
    return st.owner( a.getVertexId() ) == st.owner( b.getVertexId() )
        && a.getVertexId().weakId() == b.getVertexId().weakId();
}

template < typename Store >
inline bool equalVertex( Store& st, typename Store::Vertex a, typename Store::Vertex b ) {
    return st.pool().equal( a.getNode(), b.getNode() );
}



template < typename Self, typename TableUtils, typename Statistics >
struct StoreCommon : public TableUtils {
    using Table = typename TableUtils::Table;
    using TableUtils::table;
    using Hasher = typename Table::Hasher;
    using Node = typename Table::Item;

  protected: // we don't want algoritms to mess with table directly
    WithID* _id;

  public:
    template < typename... Args >
    StoreCommon( Args&&... args ) :
        TableUtils( std::forward< Args >( args )... ), _id( nullptr )
    { }

    const Hasher& hasher() const {
        return table().hasher;
    }

    Hasher& hasher() {
        return table().hasher;
    }

    hash_t hash( Node node ) const {
        return hasher().hash( node );
    }

    void setId( WithID& id ) {
        _id = &id;
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

    bool alias( Node n1, Node n2 ) {
        return visitor::alias( n1, n2 );
    }

    bool valid( Node n ) {
        return hasher().valid( n );
    }

    bool equal( Node m, Node n ) {
        return hasher().equal( m, n );
    }

    bool has( Node n ) {
        return table().has( n );
    }

  protected:
    // Retrieve node from hash table
    Node _fetch( Node s, hash_t h, bool *had = 0 ) {
        Node found = table().getHinted( s, h, had );

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
    Node _store( Node s, hash_t h, bool *had = nullptr ) {
        Statistics::global().hashadded( _id->id(), memSize( s, hasher().pool ) );
        Statistics::global().hashsize( _id->id(), table().size() );
        Node s2;
        bool inserted;
        std::tie( s2, inserted ) = table().insertHinted( s, h );
        if ( had )
            *had = !inserted;
        setPermanent( hasher().pool, s2 );
        return s2;
    }

    template < typename Select, typename VertexId >
    int _compareId( Select select, VertexId a, VertexId b ) {
        if ( !permanent( pool(), select( a ) ) )
            return 0 - permanent( pool(), select( b ) );

        if ( !permanent( pool(), select( b ) ) )
            return 1;

        return pool().compare( select( a ), select( b ) );
    }
};

template < template< typename, typename > class Table,
         typename Node, typename Hasher >
using TableIdentity = Table< Node, Hasher >;

template < typename Node, typename Hasher,
         template < template < typename, typename > class, typename, typename >
            class TableWrapper
         >
struct PartitionedTable {
    using Table = TableWrapper< HashSet, Node, Hasher >;
    Table _table;

    Table &table() {
        return _table;
    }

    const Table &table() const {
        return _table;
    }

    template < typename... Args >
    PartitionedTable( PartitionedTable *, Args&&... args )
        : _table( std::forward< Args >( args )... )
    { }
};

template < typename Node, typename Hasher,
         template < template < typename, typename > class, typename, typename >
            class TableWrapper
         >
struct SharedTable {
    using Table = TableWrapper< SharedHashSet, Node, Hasher >;
    using TablePtr = std::shared_ptr< Table >;

    TablePtr _table;

    Table &table() {
        return *_table;
    }

    const Table &table() const {
        return *_table;
    }

    template < typename... Args >
    SharedTable( SharedTable *master, Args&&... args )
        : _table( master
                ? master->_table
                : std::make_shared< Table >( std::forward< Args >( args )... ) )
    { }
};

#define STORE_CLASS using Node = typename Base::Node; \
    using Table = typename Base::Table; \
    using Hasher = typename Base::Hasher; \
    using Base::table; \
    using Base::hasher; \
    using Base::hash; \
    using Base::setId; \
    using Base::pool; \
    using Base::slack; \
    using Base::alias; \
    using Base::valid; \
    using Base::equal; \
    using Base::has; \
    using Base::_id

#define STORE_ITERATOR using Iterator = StoreIterator< This >; \
    Iterator begin() { return Iterator( *this, 0 ); } \
    Iterator end() { return Iterator( *this, table().size() ); } \
    friend class StoreIterator< This >

template < typename This >
class StoreIterator
    : std::iterator< std::input_iterator_tag, typename This::VertexId > {
    using VertexId = typename This::VertexId;
    size_t i;
    This& store;

    void bump() {
        while ( i < store.table().size()
                && !store.hasher().valid( store.table()[ i ] ) )
            ++i;
    }

  public:
    StoreIterator( This& store, size_t i ) : i( i ), store( store )
    {
        bump();
    }

    VertexId operator*() {
        return VertexId( store.table()[ i ] );
    }

    StoreIterator& operator++() {
        ++i;
        bump();
        return *this;
    }

    bool operator==( StoreIterator b ) {
        return i == b.i;
    }

    bool operator!=( StoreIterator b ) {
        return i != b.i;
    }
};

template < typename Node >
struct StdVertexId {
    using VertexId = StdVertexId< Node >;

    Node node;
    explicit StdVertexId( Node n ) : node( n ) { }
    StdVertexId() : node() { }

    uintptr_t weakId() {
        return node.valid() ?
            reinterpret_cast< uintptr_t >( node.ptr )
            : 0;
    }

    template < typename Extension >
    Extension& extension( Pool& p, int n = 0 ) {
        return p.template get< Extension >( node, n );
    }

    bool valid() {
        return node.valid();
    }

    WithID *id;

    // Retrieve node from hash table
    T fetch( T s, hash_t h, bool *had = 0 ) {
        T found = table().getHinted( s, h, had );

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
    void store( T s, hash_t h, bool * = nullptr ) {
        Statistics::global().hashadded( id->id(), memSize( s, hasher().pool ) );
        Statistics::global().hashsize( id->id(), table.size() );
        table.insertHinted( s, h );
        setPermanent( s );
    }
};

template < typename Node, typename Compressed >
struct CompressedVertex {
    using Base = StdVertex< Node >;
    using Vertex = CompressedVertex< Node, Compressed >;
    using VertexId = StdVertexId< Compressed >;

    Base base;
    Compressed compressed;

    CompressedVertex( Node n, Compressed c ) : base( n ), compressed( c ) { }
    CompressedVertex() { }

    Node getNode() {
        return base.getNode();
    }

    VertexId getVertexId() {
        return VertexId( compressed );
    }

    template < typename Graph >
    Vertex clone( Graph &g ) {
        return Vertex( g.clone( base.node ), compressed );
    }

    void free( Pool &p ) {
        base.free( p );
        poolFree( p, compressed );
        compressed = Compressed();
    }

    template < typename BS >
    friend bitstream_impl::base< BS >& operator<<(
            bitstream_impl::base< BS >& bs, Vertex v )
    {
        return bs << v.base << v.compressed;
    }
    template < typename BS >
    friend bitstream_impl::base< BS >& operator>>(
            bitstream_impl::base< BS >& bs, Vertex& v )
    {
        return bs >> v.base >> v.compressed;
    }
};

// ( * -> * -> ( ( * -> * ) -> * -> * ) ) -> * -> * -> * -> *
template < template < typename, typename, template <
              template < typename, typename > class,
              typename, typename > class
            > class Utils,
         typename _Node, typename _Hasher, typename Statistics >
struct Store
    : public StoreCommon< Store< Utils, _Node, _Hasher, Statistics >,
                          Utils< _Node, _Hasher, TableIdentity >,
                          Statistics >
{
    using TableUtils = Utils< _Node, _Hasher, TableIdentity >;
    using This = Store< Utils, _Node, _Hasher, Statistics >;
    using Base = StoreCommon< This, TableUtils, Statistics >;
    STORE_CLASS;

    using VertexId = StdVertexId< _Node >;
    using Vertex = StdVertex< _Node >;

    typedef Vertex QueueVertex;

    template < typename Graph >
    Store( Graph&, Hasher h, This *master = nullptr ) : Base( master, h )
    {
        static_assert( wibble::TSame< typename Graph::Node, Node >::value,
                "using incompatible graph" );
    }

    template < typename Graph >
    Store( Graph& g, int slack, This *m = nullptr ) :
             This( g, Hasher( g.base().alloc.pool(), slack ), m )
    { }

    Vertex fromQueue( QueueVertex v ) {
        return v;
    }

    QueueVertex toQueue( Vertex v ) {
        return v;
    }

    Vertex fetch( Node node, hash_t h, bool* had = nullptr ) {
        return Vertex( Base::_fetch( node, h, had ) );
    }

    Vertex fetchByVertexId( VertexId vi ) {
        return fetch( vi.node, hash( vi.node ) );
    }

    VertexId fetchVertexId( VertexId vi ) {
        return fetch( vi.node, hash( vi.node ) ).getVertexId();
    }

    Vertex store( Node node, hash_t h, bool* had = nullptr ) {
        Node n = Base::_store( node, h, had );
        return Vertex( n );
    }

    void update( Node s, hash_t h ) {}

    void setSize( int ) { } // TODO

    void store( T s, hash_t h, bool* had = nullptr ) {
        Statistics::global().hashadded( Super::id->id(), memSize( s ) );
        Statistics::global().hashsize( Super::id->id(), table().size() );
        bool inserted;
        std::tie( std::ignore, inserted ) = table().insertHinted( s, h );
        if ( had )
            *had = !inserted;
        setPermanent( s );
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return owner( w, s.node, hint );
    }

    T fetch( T s, hash_t h, bool* had = nullptr ) {
        bool _had = table().has( s, h );
        if ( had )
            *had = _had;
        return s;
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

template < template < typename, typename, template <
              template < typename, typename > class,
              typename, typename > class
            > class Utils,
         typename _Node, typename _Hasher, typename Statistics >
struct HcStore
    : public StoreCommon< HcStore< Utils, _Node, _Hasher, Statistics >,
                          Utils< _Node, HcHasher< _Hasher >, TableIdentity >,
                          Statistics >
{
    static_assert( wibble::TSame< _Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );
    using TableUtils = Utils< _Node, HcHasher< _Hasher >, TableIdentity >;
    using This = HcStore< Utils,_Node, _Hasher, Statistics >;
    using Base = StoreCommon< This, TableUtils, Statistics >;
    STORE_CLASS;

    using VertexId = StdVertexId< Node >;
    using Vertex = CompressedVertex< Node, Node >;
    using QueueVertex = Vertex;

    template < typename Graph >
    HcStore( Graph& g, Hasher h, This *master = nullptr ) :
        Base( master, h ), _alloc( g.base().alloc )
    {
        static_assert( wibble::TSame< typename Graph::Node, Node >::value,
                "using incompatible graph" );
    }

    template < typename Graph >
    HcStore( Graph& g, int slack, This *m = nullptr ) :
             This( g, Hasher( g.base().alloc.pool(), slack ), m )
    { }

    Allocator& _alloc;

    Vertex fromQueue( QueueVertex v ) {
        return v;
    }

    QueueVertex toQueue( Vertex v ) {
        return v;
    }

    hash_t& stubHash( Blob stub ) {
        return pool().template get< hash_t >( stub, slack() );
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
        Blob stub = _alloc.new_blob( int( sizeof( hash_t ) ) );
//        Statistics::global().hashadded( this->_id , memSize( stub ) );
//        Statistics::global().hashsize( this->_id, this->_table.size() );
        this->pool().copyTo( s, stub, slack() );
        stubHash( stub ) = h;
        Node n;
        std::tie( n, std::ignore )= table().insertHinted( stub, h );
        setPermanent( pool(), n );
        if ( !alias( n, stub ) )
            this->pool().free( stub );
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
            return hash( n ) % w.peers();
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return stubHash( s.compressed ) % w.peers();
    }

    template< typename W >
    int owner( W &w, VertexId h ) const {
        return stubHash( h.node ) % w.peers();
    }

    bool valid( Vertex v ) {
        return valid( v.base.node );
    }

    bool valid( VertexId vi ) {
        return valid( vi.node );
    }

    int compareId( VertexId a, VertexId b ) {
        return Base::_compareId( []( VertexId x ) { return x.node; }, a, b );
    }

    STORE_ITERATOR;
};


template < template < typename, typename, template <
              template < typename, typename > class,
              typename, typename > class
            > class Utils,
          template < template < typename, typename > class, typename, typename
            > class _Table,
          typename _Node, typename _Hasher, typename Statistics >
struct CompressedStore
    : public StoreCommon< CompressedStore< Utils, _Table, _Node, _Hasher, Statistics >,
                          Utils< _Node, _Hasher, _Table >,
                          Statistics >
{
    using This = CompressedStore< Utils, _Table, _Node, _Hasher, Statistics >;
    using TableUtils = Utils< _Node, _Hasher, _Table >;
    using Base = StoreCommon< This, TableUtils, Statistics >;
    STORE_CLASS;

    static_assert( wibble::TSame< Node, Blob >::value,
            "CompressedStore can only work with Blob nodes" );

    using VertexId = StdVertexId< Node >;
    using Vertex = CompressedVertex< Node, Node >;
    using QueueVertex = VertexId;

    template< typename Graph, typename... Args >
    CompressedStore( Graph&, Hasher h, This *master, Args&&... args ) :
        Base( master, h, std::forward< Args >( args )... )
    {
        static_assert( wibble::TSame< typename Graph::Node, Node >::value,
                "using incompatible graph" );
    }

    QueueVertex toQueue( Vertex v ) {
        return v.getVertexId();
    }

    Vertex fetch( Node node, hash_t h, bool* had = nullptr ) {
        Node found = Base::_fetch( node, h, had );
        if ( !valid( found ) )
            return Vertex();

        if ( !alias( node, found ) ) {
            pool().copyTo( found, node, slack() );
            return Vertex( node, found );
        }
        return Vertex( found, Node() );
    }

    void update( Node node, hash_t h ) {
        // update state information in hashtable
        Node found = table().getHinted( node, h, NULL );
        assert( valid( found ) );
        assert( !alias( node, found ) );
        pool().copyTo( node, found, slack() );
    }

    Vertex store( Node node, hash_t h, bool* = nullptr ) {
        Statistics::global().hashadded( _id->id(),
                memSize( node, pool() ) );
        Statistics::global().hashsize( _id->id(), table().size() );
        Node n;
        std::tie( n, std::ignore ) = table().insertHinted( node, h );
        setPermanent( pool(), n );
        return Vertex( node, n );
    }

    void setSize( int ) { } // TODO

    template< typename W >
    int owner( W &w, Node n, hash_t hint = 0 ) const {
        if ( hint )
            return hint % w.peers();
        else
            return hash( n ) % w.peers();
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return owner( w, s.base.node, hint );
    }

    bool valid( Vertex v ) {
        return valid( v.base.node );
    }

    bool valid( VertexId vi ) {
        return valid( vi.node );
    }

    int compareId( VertexId a, VertexId b ) {
        return Base::_compareId( []( VertexId x ) { return x.node; }, a, b );
    }
};


template < template < typename, typename, template <
              template < typename, typename > class,
              typename, typename > class
            > class Utils,
            typename _Node, typename _Hasher, typename Statistics >
struct TreeCompressedStore : public CompressedStore< Utils,
        TreeCompressedHashSet, _Node, _Hasher, Statistics >
{
    using Base = CompressedStore< Utils, TreeCompressedHashSet, _Node,
          _Hasher, Statistics >;
    using TableUtils = typename Base::TableUtils;
    using This = TreeCompressedStore< Utils, _Node, _Hasher, Statistics >;
    using VertexId = typename Base::VertexId;
    using Vertex = typename Base::Vertex;
    using QueueVertex = typename Base::QueueVertex;
    STORE_CLASS;

    template< typename Graph >
    TreeCompressedStore( Graph& g, Hasher h ) : This( g, h, nullptr )
    { }

    template< typename Graph, typename... Args >
    TreeCompressedStore( Graph& g, int slack, This *m, Args&&... args ) :
        This( g, _Hasher( g.base().alloc.pool(), slack ), m, std::forward< Args >( args )... )
    { }

    template< typename Graph, typename... Args >
    TreeCompressedStore( Graph& g, Hasher h, This *m, Args&&... args ) :
        Base( g, h, m, 16, std::forward< Args >( args )... )
    { }

    Vertex fromQueue( QueueVertex v ) {
        return fetchByVertexId( v );
    }

    VertexId fetchVertexId( VertexId vi ) {
        if ( !vi.valid() )
            return vi;
        return VertexId( Base::_fetch( vi.node, _hash( vi ) ) );
    }

    Vertex fetchByVertexId( VertexId vi ) {
        if ( !vi.valid() )
            return Vertex();
        vi = fetchVertexId( vi );
        return Vertex( table().getReassembled( vi.node ), vi.node );
    }

    using Base::owner;

    template< typename W >
    int owner( W &w, VertexId h ) {
        return _hash( h ) % w.peers();
    }

    STORE_ITERATOR;

  private:
    hash_t _hash( VertexId vi ) {
        return table().header( vi.node ).fork
                ? table().rootFork( vi.node )->hash
                : table().m_roots.hasher.hash( vi.node );
    }

};

}
}
#undef STORE_CLASS
#undef STORE_ITERATOR
#endif
