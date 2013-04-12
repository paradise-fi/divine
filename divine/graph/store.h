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



template < typename Self, typename _Table, typename Statistics >
struct StoreCommon {
    using Table = _Table;
    using Hasher = typename Table::Hasher;
    using Node = typename Table::Item;

  protected: // we don't want algoritms to mess with table directly
    WithID* _id;
    Self& self() {
        return *static_cast< Self* >( this );
    }

    Table& table() {
        return self().table();
    }

    const Self& self() const {
        return *static_cast< const Self* >( this );
    }

    const Table& table() const {
        return self().table();
    }

  public:
    StoreCommon() : _id( nullptr )
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
    Node _store( Node s, hash_t h, bool * = nullptr ) {
        Statistics::global().hashadded( _id->id(), memSize( s, hasher().pool ) );
        Statistics::global().hashsize( _id->id(), table().size() );
        Node s2 = table().insertHinted( s, h );
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

#define STORE_CLASS using Node = typename Base::Node; \
    using Hasher = typename Base::Hasher; \
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

  public:
    StoreIterator( This& store, size_t i ) : i( i ), store( store ) { }

    VertexId operator*() {
        return VertexId( store.table()[ i ] );
    }

    StoreIterator& operator++() {
        ++i;
        return *this;
    }

    bool operator==( StoreIterator b ) {
        return i == b.i;
    }

    bool operator!=( StoreIterator b ) {
        return i != b.i;
    }
};

template < typename _Node, typename _Hasher, typename Statistics >
struct PartitionedStore
    : public StoreCommon< PartitionedStore< _Node, _Hasher, Statistics >,
                          HashSet< _Node, _Hasher >, Statistics >
{
    using Table = HashSet< _Node, _Hasher >;
    using This = PartitionedStore< _Node, _Hasher, Statistics >;
    using Base = StoreCommon< This, Table, Statistics >;
    STORE_CLASS;

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

        bool valid() {
            return node.valid();
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

    struct Vertex {
        Node node;

        explicit Vertex( Node n ) : node( n ) { }
        Vertex() : node() { }

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

    void update( Node s, hash_t h ) {}

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
        return owner( w, s.node, hint );
    }

    template < typename W >
    int owner( W &w, VertexId h ) const {
        int own = hash( h.node ) % w.peers();
        return own;
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


template < typename _Node, typename _Hasher, typename Statistics >
struct SharedStore
    : public StoreCommon< SharedStore< _Node, _Hasher, Statistics >,
                          SharedHashSet< _Node, _Hasher >, Statistics >
{
    typedef SharedStore< _Node, _Hasher, Statistics > This;
    typedef SharedHashSet< _Node, _Hasher > Table;
    typedef StoreCommon< This, Table, Statistics > Base;
    typedef std::shared_ptr< Table > TablePtr;
    STORE_CLASS;

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

        bool valid() {
            return node.valid();
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

    struct Vertex {
        Node node;

        explicit Vertex( Node n ) : node( n ) { }
        Vertex() : node() { }

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
            p.free( node );
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

    typedef Vertex QueueVertex;

    TablePtr _table;

    template< typename Graph >
    SharedStore( Graph& g, int slack, This *master = nullptr ) :
        This( g, Hasher( g.base().alloc.pool(), slack ), master )
    {}

    template< typename Graph >
    SharedStore( Graph &, Hasher h, This *master = nullptr ) :
        _table( master ? master->_table : std::make_shared< Table >( h ) )
    { }

    Table& table() {
        return *_table;
    }
    const Table& table() const {
        return *_table;
    }

    Vertex fromQueue( QueueVertex v ) {
        return v;
    }

    QueueVertex toQueue( Vertex v ) {
        return v;
    }

    /*
    Vertex fetch( Node node, hash_t h, bool* had = nullptr ) {
        return Vertex( Base::_fetch( node, h, had ) );
    }
    */

    Vertex fetchByVertexId( VertexId vi ) {
        return fetch( vi.node, hash( vi.node ) );
    }

    VertexId fetchVertexId( VertexId vi ) {
        return fetchByVertexId( vi ).getVertexId();
    }

    /*
    Vertex store( Node node, hash_t h, bool* had = nullptr ) {
        Node n = Base::_store( node, h, had );
        return Vertex( n );
    }
    */

    void update( Node s, hash_t h ) {}

    void maxSize( size_t ) {}
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

    template< typename W, typename V >
    int owner( W &w, V, hash_t hint = 0 ) const {
        return w.id();
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

template< typename _Node, typename _Hasher, typename Statistics >
struct HcStore
        : public StoreCommon< HcStore< _Node, _Hasher, Statistics >,
                              HashSet< _Node, HcHasher< _Hasher > >, Statistics >
{
    static_assert( wibble::TSame< _Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );
    using This = HcStore< _Node, _Hasher, Statistics >;
    using Table = HashSet< _Node, HcHasher< _Hasher > >;
    using Base = StoreCommon< This, Table, Statistics >;
    STORE_CLASS;

    struct VertexId {
        Node hash;
        explicit VertexId( Node hash ) : hash( hash ) { }
        VertexId() : hash() { }

        uintptr_t weakId() {
            return hash.valid()
                ? reinterpret_cast< uintptr_t >( hash.ptr )
                : 0;
        }

        template < typename Extension >
        Extension& extension( Pool& p, int n = 0 ) {
            return p.template get< Extension >( hash, n );
        }

        bool valid() {
            return hash.valid();
        }

        template < typename BS >
        friend bitstream_impl::base< BS >& operator<<(
                bitstream_impl::base< BS >& bs, VertexId vi )
        {
            return bs << vi.hash;
        }
        template < typename BS >
        friend bitstream_impl::base< BS >& operator>>(
                bitstream_impl::base< BS >& bs, VertexId& vi )
        {
            return bs >> vi.hash;
        }
    };

    struct Vertex {
        Node node;
        Node hash;

        Vertex( Node node, Node hash ) : node( node ), hash( hash ) { }
        Vertex() : node(), hash( 0 ) { }

        Node getNode() {
            return node;
        }

        VertexId getVertexId() {
            return VertexId( hash );
        }

        template < typename Graph >
        Vertex clone( Graph& g ) {
            return Vertex( g.clone( node ), hash );
        }

        void free( Pool& p ) {
            poolFree( p, node );
            p.free( hash );
            node = hash = Node();
        }

        template < typename BS >
        friend bitstream_impl::base< BS >& operator<<(
                bitstream_impl::base< BS >& bs, Vertex v )
        {
            return bs << v.node << v.hash;
        }
        template < typename BS >
        friend bitstream_impl::base< BS >& operator>>(
                bitstream_impl::base< BS >& bs, Vertex& v )
        {
            return bs >> v.node >> v.hash;
        }
    };

    typedef Vertex QueueVertex;

    template < typename Graph >
    HcStore( Graph& g, Hasher h, This * = nullptr ) : _table( h ), _alloc( g.base().alloc )
    {
        static_assert( wibble::TSame< typename Graph::Node, Node >::value,
                "using incompatible graph" );
    }

    template < typename Graph >
    HcStore( Graph& g, int slack, This *m = nullptr ) :
             This( g, Hasher( g.base().alloc.pool(), slack ), m )
    { }

    Table _table;
    Allocator& _alloc;

    Table& table() {
        return _table;
    }
    const Table& table() const {
        return _table;
    }

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
        Node n = _table.insertHinted( stub, h );
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
        return stubHash( s.hash ) % w.peers();
    }

    template< typename W >
    int owner( W &w, VertexId h ) const {
        return stubHash( h.hash ) % w.peers();
    }

    bool valid( Vertex v ) {
        return valid( v.node );
    }

    bool valid( VertexId vi ) {
        return valid( vi.hash );
    }

    int compareId( VertexId a, VertexId b ) {
        return Base::_compareId( []( VertexId x ) { return x.hash; }, a, b );
    }

    STORE_ITERATOR;
};


template < template < template < typename, typename > class, typename, typename > class _Table,
    template < typename, typename > class _BaseTable, typename _Node, typename _Hasher,
    typename Statistics >
struct CompressedStore
    : public StoreCommon< CompressedStore< _Table, _BaseTable, _Node, _Hasher, Statistics >,
                          _Table< _BaseTable, _Node, _Hasher >, Statistics >
{
    using Table = _Table< _BaseTable, _Node, _Hasher >;
    using This = CompressedStore< _Table, _BaseTable, _Node, _Hasher, Statistics >;
    using Base = StoreCommon< This, Table, Statistics >;
    STORE_CLASS;

    static_assert( wibble::TSame< Node, Blob >::value,
            "CompressedStore can only work with Blob nodes" );

    struct VertexId {
        Node compressed;

        explicit VertexId( Node compressed ) : compressed( compressed ) { }
        VertexId() : compressed() { }

        uintptr_t weakId() {
            return compressed.valid()
                ? reinterpret_cast< uintptr_t >( compressed.ptr )
                : 0;
        }

        bool valid() {
            return compressed.valid();
        }

        void free( Pool& p ) {
            p.free( compressed );
            compressed = Node();
        }

        template < typename BS >
        friend bitstream_impl::base< BS >& operator<<(
                bitstream_impl::base< BS >& bs, VertexId vi )
        {
            return bs << vi.compressed;
        }
        template < typename BS >
        friend bitstream_impl::base< BS >& operator>>(
                bitstream_impl::base< BS >& bs, VertexId& vi )
        {
            return bs >> vi.compressed;
        }

        template < typename Extension >
        Extension& extension( Pool& p, int n = 0 ) {
            return p.template get< Extension >( compressed, n );
        }
    };

    struct Vertex {
        Node node;
        Node compressed;

        Vertex( Node node, Node compressed ) : node( node ),
            compressed( compressed )
        { }
        Vertex() : node(), compressed() { }

        Node getNode() {
            return node;
        }

        VertexId getVertexId() {
            return VertexId( compressed );
        }

        template < typename Graph >
        Vertex clone( Graph& g ) {
            return Vertex( g.clone( node ), compressed );
        }

        void free( Pool& p ) {
            poolFree( p, node );
            p.free( compressed );
            node = compressed = Node();
        }

        template < typename BS >
        friend bitstream_impl::base< BS >& operator<<(
                bitstream_impl::base< BS >& bs, Vertex v )
        {
            return bs << v.node << v.compressed;
        }
        template < typename BS >
        friend bitstream_impl::base< BS >& operator>>(
                bitstream_impl::base< BS >& bs, Vertex& v )
        {
            return bs >> v.node >> v.compressed;
        }
    };

    typedef VertexId QueueVertex;

    Table _table;
    Table& table() {
        return _table;
    }
    const Table& table() const {
        return _table;
    }

    template< typename Graph, typename... Args >
    CompressedStore( Graph&, Hasher h, This *, Args&&... args ) :
        _table( h, std::forward< Args >( args )... )
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
        Node found = _table.getHinted( node, h, NULL );
        assert( valid( found ) );
        assert( !alias( node, found ) );
        pool().copyTo( node, found, slack() );
    }

    Vertex store( Node node, hash_t h, bool* = nullptr ) {
        Statistics::global().hashadded( _id->id(),
                memSize( node, pool() ) );
        Statistics::global().hashsize( _id->id(), _table.size() );
        Node n = _table.insertHinted( node, h );
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
        return owner( w, s.node, hint );
    }

    bool valid( Vertex v ) {
        return valid( v.node );
    }

    bool valid( VertexId vi ) {
        return valid( vi.compressed );
    }

    int compareId( VertexId a, VertexId b ) {
        return Base::_compareId( []( VertexId x ) { return x.compressed; }, a, b );
    }
};


template < typename _Node, typename _Hasher, typename Statistics >
struct TreeCompressedStore : public CompressedStore< TreeCompressedHashSet,
        HashSet, _Node, _Hasher, Statistics >
{
    using Base = CompressedStore< TreeCompressedHashSet, HashSet, _Node,
          _Hasher, Statistics >;
    using This = TreeCompressedStore< _Node, _Hasher, Statistics >;
    using Table = typename This::Table;
    using VertexId = typename Base::VertexId;
    using Vertex = typename Base::Vertex;
    using QueueVertex = typename Base::QueueVertex;
    STORE_CLASS;
    using Base::_table;
    using Base::table;

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
        return VertexId( Base::_fetch( vi.compressed, _hash( vi ) ) );
    }

    Vertex fetchByVertexId( VertexId vi ) {
        if ( !vi.valid() )
            return Vertex();
        vi = fetchVertexId( vi );
        return Vertex( _table.getReassembled( vi.compressed ), vi.compressed );
    }

    using Base::owner;

    template< typename W >
    int owner( W &w, VertexId h ) {
        return _hash( h ) % w.peers();
    }

    STORE_ITERATOR;

  private:
    hash_t _hash( VertexId vi ) {
        return _table.header( vi.compressed ).fork
                ? _table.rootFork( vi.compressed )->hash
                : _table.m_roots.hasher.hash( vi.compressed );
    }

};

}
}
#undef STORE_CLASS
#undef STORE_ITERATOR
#endif
