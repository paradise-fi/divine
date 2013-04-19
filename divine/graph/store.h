// -*- C++ -*-
#include <utility>
#include <memory>
#include <iterator>
#include <tuple>

#include <divine/utility/statistics.h>
#include <divine/toolkit/sharedhashset.h>
#include <divine/toolkit/treecompressedhashset.h>
#include <divine/toolkit/ntreehashset.h>
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

template< typename Pool, typename T >
void poolFree( Pool p, T* x ) {
    if ( x != nullptr )
        x->free( p );
}

template < typename Pool, typename T >
typename std::enable_if< !std::is_fundamental< T >::value >::type poolFree( Pool p, T x ) {
    p.free( x );
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
    using TableUtils::_owner;
    using Hasher = typename Table::Hasher;
    using Node = typename Table::Item;
    using TableItem = typename Table::TableItem;
    static_assert( wibble::TSame< Node, typename TableUtils::Node >::value,
            "Table's Node is incompatible with generator" );

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

    template< typename T >
    bool alias( T* a, T* b ) { return a == b; }

    bool valid( Node n ) {
        return hasher().valid( n );
    }

    bool equal( Node m, Node n ) {
        return hasher().equal( m, n );
    }

    bool has( Node n ) {
        return table().has( n );
    }

    void setSize( int size ) {
        table().setSize( size );
    }

  protected:
    // Retrieve node from hash table
    std::tuple< TableItem, bool > _fetch( TableItem s, hash_t h ) {
        TableItem found;
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
    std::tuple< TableItem, bool > _store( Node s, hash_t h ) {
        Statistics::global().hashadded( _id->id(), memSize( s, hasher().pool ) );
        Statistics::global().hashsize( _id->id(), table().size() );
        TableItem s2;
        bool inserted;
        std::tie( s2, inserted ) = table().insertHinted( s, h );
        setPermanent( hasher().pool, s2 );
        return std::make_tuple( s2, inserted );
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

template < typename Generator, typename Hasher,
         template < template < typename, typename > class,
              typename, typename > class TableWrapper
         >
struct PartitionedTable {
    using Node = typename Generator::Node;
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

  protected:
    template< typename W >
    int _owner( W &w, hash_t hash ) const {
        return hash % w.peers();
    }
};

template < typename Generator, typename Hasher,
         template < template < typename, typename > class,
              typename, typename > class TableWrapper
         >
struct SharedTable {
    using Node = typename Generator::Node;
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

  protected:
    template< typename W >
    int _owner( W &w, hash_t hash ) const {
        return w.id();
    }
};

#define STORE_CLASS using Node = typename Base::Node; \
    using Table = typename Base::Table; \
    using TableItem = typename Base::TableItem; \
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
    using Base::_id; \
    using Base::_owner

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

template < typename T >
bool _valid( T t ) {
    return t.valid();
}

template < typename T >
bool _valid( T* t ) {
    return t != nullptr;
}

template < typename T, typename Extension >
inline Extension& _extension( T t, Pool& p, int n, Extension* ) {
    return p.template get< Extension >( t, n );
}

template < typename Root, typename Extension >
inline Extension& _extension( Root* root, Pool&, int n, Extension* ) {
    return *reinterpret_cast< Extension* >( root->slack() + n );
}

template < typename T >
inline uintptr_t _weakId( T t ) {
    return t.valid() ? reinterpret_cast< uintptr_t >( t.ptr ) : 0;
}

template < typename T >
inline uintptr_t _weakId( T* t ) {
    return reinterpret_cast< uintptr_t >( t );
}

template < typename Node >
struct StdVertexId {
    using VertexId = StdVertexId< Node >;

    Node node;
    explicit StdVertexId( Node n ) : node( n ) { }
    StdVertexId() : node() { }

    uintptr_t weakId() {
        return _weakId( node );
    }

    template < typename Extension >
    Extension& extension( Pool& p, int n = 0 ) {
        return _extension( node, p, n,
                reinterpret_cast< Extension* >( 0 ) );
    }

    bool valid() {
        return _valid( node );
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

    void free( Pool& p ) {
        poolFree( p, node );
        node = Node();
    }
};

template < typename Node >
struct StdVertex {
    using Vertex = StdVertex< Node >;
    using VertexId = StdVertexId< Node >;

    Node node;

    explicit StdVertex( Node n ) : node( n ) { }
    StdVertex() : node() { }

    Node getNode() {
        return node;
    }

    VertexId getVertexId() {
        return VertexId( node );
    }

    template < typename Graph >
    Vertex clone( Graph& g ) {
        return Vertex( g.clone( node ) );
    }

    void free( Pool& p ) {
        poolFree( p, node );
        node = Node();
    }

    template < typename BS >
    friend bitstream_impl::base< BS >& operator<<(
            bitstream_impl::base< BS >& bs, Vertex v )
    {
        return bs << v.node;
    }
    template < typename BS >
    friend bitstream_impl::base< BS >& operator>>(
            bitstream_impl::base< BS >& bs, Vertex& v )
    {
        return bs >> v.node;
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
    CompressedVertex() : base(), compressed() { }

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
         typename _Generator, typename _Hasher, typename Statistics >
struct Store
    : public StoreCommon< Store< Utils, _Generator, _Hasher, Statistics >,
                          Utils< _Generator, _Hasher, TableIdentity >,
                          Statistics >
{
    using TableUtils = Utils< _Generator, _Hasher, TableIdentity >;
    using This = Store< Utils, _Generator, _Hasher, Statistics >;
    using Base = StoreCommon< This, TableUtils, Statistics >;
    STORE_CLASS;

    using VertexId = StdVertexId< Node >;
    using Vertex = StdVertex< Node >;

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

    std::tuple< Vertex, bool > wrapTuple( std::tuple< Node, bool > tuple ) {
        Node n;
        bool i;
        std::tie( n, i ) = tuple;
        return std::make_tuple( Vertex( n ), i );
    }

    std::tuple< Vertex, bool > fetch( Node node, hash_t h ) {
        return wrapTuple( Base::_fetch( node, h ) );
    }

    Vertex fetchByVertexId( VertexId vi ) {
        return std::get< 0 >( fetch( vi.node, hash( vi.node ) ) );
    }

    VertexId fetchVertexId( VertexId vi ) {
        return fetchByVertexId( vi ).getVertexId();
    }

    std::tuple< Vertex, bool > store( Node node, hash_t h ) {
        return wrapTuple( Base::_store( node, h ) );
    }

    void update( Node s, hash_t h ) {}

    template< typename W >
    int owner( W &w, Node n, hash_t hint = 0 ) const {
        return _owner( w, hint ? hint : hash( n ) );
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return owner( w, s.node, hint );
    }

    template < typename W >
    int owner( W &w, VertexId h ) const {
        return _owner( w, hash( h.node ) );
    }

    bool valid( Vertex v ) {
        return valid( v.node );
    }

    bool valid( VertexId vi ) {
        return valid( vi.node );
    }

    int compareId( VertexId a, VertexId b ) {
        return Base::_compareId( []( VertexId x ) { return x.node; }, a, b );
    }

    STORE_ITERATOR;
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
         typename _Generator, typename _Hasher, typename Statistics >
struct HcStore
    : public StoreCommon< HcStore< Utils, _Generator, _Hasher, Statistics >,
                          Utils< _Generator, HcHasher< _Hasher >, TableIdentity >,
                          Statistics >
{
    using TableUtils = Utils< _Generator, HcHasher< _Hasher >, TableIdentity >;
    using This = HcStore< Utils,_Generator, _Hasher, Statistics >;
    using Base = StoreCommon< This, TableUtils, Statistics >;
    STORE_CLASS;
    static_assert( wibble::TSame< Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );

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

    hash_t stubHash( const Blob stub ) const {
        return this->pool().template get< hash_t >( stub, this->slack() );
    }

    std::tuple< Vertex, bool > fetch( Blob s, hash_t h ) {
        Blob found;
        bool had;
        std::tie( found, had ) = Base::_fetch( s, h );

        if ( had ) {
            // copy saved state information
            pool().copyTo( found, s, slack() );
            return std::make_tuple( Vertex( s, found ), had );
        }
        return std::make_tuple( Vertex( found, Node() ), had );
    }

    Vertex fetchByVertexId( VertexId ) {
        assert_unimplemented();
    }

    VertexId fetchVertexId( VertexId vi ) {
        return VertexId( std::get< 0 >( Base::_fetch( vi.node, stubHash( vi.node ) ) ) );
    }

    std::tuple< Vertex, bool > store( Blob s, hash_t h ) {
        // store just a stub containing state information
        Blob stub = _alloc.new_blob( int( sizeof( hash_t ) ) );
        this->pool().copyTo( s, stub, slack() );
        stubHash( stub ) = h;
        Node n;
        bool inserted;
        std::tie( n, inserted ) = Base::_store( stub, h );
        if ( !inserted )
            pool().free( stub );
        assert( equal( s, stub ) );
        return std::make_tuple( Vertex( s, n ), inserted );
    }

    void update( Blob s, hash_t h ) {
        // update state information in hashtable
        Blob stub;
        bool had;
        std::tie( stub, had ) = table().getHinted( s, h );
        assert( valid( stub ) );
        assert( had );
        pool().copyTo( s, stub, slack() );
    }

    bool has( Node node ) {
        return table().has( node );
    }

    template< typename W >
    int owner( W &w, Node n, hash_t hint = 0 ) const {
        return _owner( w, hint ? hint : hash( n ) );
    }

    template< typename W >
    int owner( W &w, Vertex s, hash_t hint = 0 ) const {
        return _owner( w, stubHash( s.compressed ) );
    }

    template< typename W >
    int owner( W &w, VertexId h ) const {
        return _owner( w, stubHash( h.node ) );
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
          template < template < typename, typename > class,
              typename, typename > class _Table,
          typename _Generator, typename _Hasher, typename Statistics >
struct CompressedStore
    : public StoreCommon< CompressedStore< Utils, _Table, _Generator, _Hasher, Statistics >,
                          Utils< _Generator, _Hasher, _Table >,
                          Statistics >
{
    using This = CompressedStore< Utils, _Table, _Generator, _Hasher, Statistics >;
    using TableUtils = Utils< _Generator, _Hasher, _Table >;
    using Base = StoreCommon< This, TableUtils, Statistics >;
    STORE_CLASS;

    static_assert( wibble::TSame< Node, Blob >::value,
            "CompressedStore can only work with Blob nodes" );

    using VertexId = StdVertexId< TableItem >;
    using Vertex = CompressedVertex< Node, TableItem >;
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

    std::tuple< Vertex, bool > fetch( Node node, hash_t h ) {
        TableItem found;
        bool had;
        std::tie( found, had ) = Base::_fetch( node, h );
        if ( had ) {
            pool().copyTo( found, node, slack() );
            return std::make_tuple( Vertex( node, found ), had );
        }
        return std::make_tuple( Vertex( found, Node() ), had );
    }

    void update( Node node, hash_t h ) {
        // update state information in hashtable
        Node found;
        bool had;
        std::tie( found, had ) = table().getHinted( node, h );
        assert( valid( found ) );
        assert( had );
        assert( !alias( node, found ) );
        pool().copyTo( node, found, slack() );
    }

    std::tuple< Vertex, bool > store( Node node, hash_t h ) {
        TableItem n;
        bool inserted;
        std::tie( n, inserted ) = Base::_store( node, h );
        return std::make_tuple( Vertex( node, n ), inserted );
    }

    template< typename W >
    int owner( W &w, Node n, hash_t hint = 0 ) const {
        return _owner( w, hint ? hint : hash( n ) );
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
            typename _Generator, typename _Hasher, typename Statistics >
struct TreeCompressedStore : public CompressedStore< Utils,
        TreeCompressedHashSet, _Generator, _Hasher, Statistics >
{
    using Base = CompressedStore< Utils, TreeCompressedHashSet, _Generator,
          _Hasher, Statistics >;
    using TableUtils = typename Base::TableUtils;
    using This = TreeCompressedStore< Utils, _Generator, _Hasher, Statistics >;
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
        return VertexId( std::get< 0 >( Base::_fetch( vi.node, _hash( vi ) ) ) );
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
        return _owner( w, _hash( h ) );
    }

    STORE_ITERATOR;

  private:
    hash_t _hash( VertexId vi ) {
        return table().header( vi.node ).fork
                ? table().rootFork( vi.node )->hash
                : table().m_roots.hasher.hash( vi.node );
    }

};

template < typename Hasher >
struct NTHasher : public Hasher {
    template < typename... Args >
    NTHasher( Args&&... args ) : Hasher( std::forward< Args >( args )... ) { }

    using Hasher::valid;
    template < typename T >
    bool valid( T* t ) {
        return t != nullptr;
    }
};

template < template < typename, typename, template <
              template < typename, typename > class,
              typename, typename > class
            > class Utils,
            typename _Generator, typename _Hasher, typename Statistics >
struct NTreeStore : public CompressedStore< Utils,
        NTreeHashSet, _Generator, NTHasher< _Hasher >, Statistics >
{
    using Base = CompressedStore< Utils, NTreeHashSet, _Generator,
          NTHasher< _Hasher >, Statistics >;
    using TableUtils = typename Base::TableUtils;
    using This = NTreeStore< Utils, _Generator, _Hasher, Statistics >;
    using VertexId = typename Base::VertexId;
    using Vertex = typename Base::Vertex;
    using QueueVertex = typename Base::QueueVertex;
    STORE_CLASS;
    using Root = typename Table::Root;

    template< typename Graph >
    NTreeStore( Graph& g, Hasher h ) : This( g, h, nullptr )
    { }

    template< typename Graph, typename... Args >
    NTreeStore( Graph& g, int slack, This *m, Args&&... args ) :
        This( g, _Hasher( g.base().alloc.pool(), slack ), m, std::forward< Args >( args )... )
    { }

    template< typename Graph, typename... Args >
    NTreeStore( Graph& g, Hasher h, This *m, Args&&... args ) :
        Base( g, h, m, g.base(), std::forward< Args >( args )... )
    { }

    Vertex fromQueue( QueueVertex v ) {
        return fetchByVertexId( v );
    }

    void update( Node node, hash_t h ) {
        // update state information in hashtable
        Root* found;
        bool had;
        std::tie( found, had ) = table().getHinted( node, h );
        assert( found != nullptr );
        assert( had );
        std::memcpy( found->slack(), pool().data( node ), slack() );
    }

    std::tuple< Vertex, bool > fetch( Node node, hash_t h ) {
        Root* found;
        bool had;
        std::tie( found, had ) = table().getHinted( node, h );

        if ( !had ) {
            assert( !hasher().valid( found ) );
            return std::make_tuple( Vertex( node, nullptr ), false );
        }

        std::memcpy( pool().data( node ), found->slack(), slack() );
        assert( hasher().valid( found ) );
        return std::make_tuple( Vertex( node, found ), true );
    }

    VertexId fetchVertexId( VertexId vi ) {
        if ( !vi.valid() )
            return vi;
        return VertexId( std::get< 0 >( Base::_fetch( vi.node, vi.node->hash ) ) );
    }

    Vertex fetchByVertexId( VertexId vi ) {
        if ( !vi.valid() )
            return Vertex();
        vi = fetchVertexId( vi );
        return Vertex( vi.node->reassemble( pool() ), vi.node );
    }

    using Base::owner;

    template< typename W >
    int owner( W &w, VertexId h ) {
        return _owner( w, h.node->hash );
    }

    bool valid( VertexId vi ) {
        return hasher().valid( vi.node );
    }

    int compareId( VertexId a, VertexId b ) {
        return table()._roots.hasher.equal( a.node, b.node );
    }

    STORE_ITERATOR;
};

}
}
#undef STORE_CLASS
#undef STORE_ITERATOR
#endif
