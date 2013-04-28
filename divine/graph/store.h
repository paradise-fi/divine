// -*- C++ -*-
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
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

#ifndef DIVINE_STORE_H
#define DIVINE_STORE_H

namespace divine {
namespace visitor {

/* Global utility functions independent of store type
 */

template< typename T >
struct IsNew : T {
    T &base() { return *this; }
    bool _isnew;
    IsNew( const T &t, bool isnew ) : T( t ), _isnew( isnew ) {}
    bool isnew() { return _isnew; }
    operator T() { return base(); }

    template< typename F >
    auto map( F f ) -> IsNew< typename std::result_of< F( T ) >::type >
    {
        return isNew( f( base() ), _isnew );
    }
};

template< typename T >
IsNew< T > isNew( const T &x, bool y ) {
    return IsNew< T >( x, y );
}

template < typename TableProvider, typename Statistics >
struct StoreCommon : TableProvider
{
    using Table = typename TableProvider::Table;
    using Hasher = typename Table::Hasher;
    using Node = typename Table::Item;
    using TableItem = typename Table::TableItem;

  protected: // we don't want algoritms to mess with table directly

  public:
    template< typename... Args >
    StoreCommon( Args&&... args )
        : TableProvider( std::forward< Args >( args )... ) {}

    Hasher& hasher() { return this->table().hasher; }
    const Hasher& hasher() const { return this->table().hasher; }

    const Pool& pool() const { return hasher().pool; }
    hash_t hash( Node node ) const { return hasher().hash( node ); }
    Pool& pool() { return hasher().pool; }
    int slack() const { return hasher().slack; }
    bool valid( Node n ) { return hasher().valid( n ); }
    bool equal( Node m, Node n ) { return hasher().equal( m, n ); }
    bool has( Node n ) { return this->table().has( n ); }
    void setSize( int size ) { this->table().setSize( size ); }

    int owner( Node n, hash_t hint = 0 ) {
        return TableProvider::owner( hint ? hint : hash( n ) );
    }

    bool knows( Node n, hash_t hint = 0 ) {
        return TableProvider::knows( hint ? hint : hash( n ) );
    }

    IsNew< Node > _fetch( Node s, hash_t h ) {
        TableItem found;
        bool had;
        std::tie( found, had ) = this->table().getHinted( s, h ? h : hash( s ) );

        if ( !had ) {
            assert( !hasher().valid( found ) );
            return isNew( s, false );
        }

        assert( hasher().valid( found ) );
        return isNew( found, true );
    }

    // Store node in hash table
    IsNew< Node > _store( Node s, hash_t h ) {
        TableItem s2;
        bool inserted;
        std::tie( s2, inserted ) = this->table().insertHinted( s, h ? h : hash( s ) );
        if ( inserted ) {
            Statistics::global().hashadded( this->id(), memSize( s, hasher().pool ) );
            Statistics::global().hashsize( this->id(), this->table().size() );
        }
        return isNew( s2, inserted );
    }
};

template< typename Generator, typename Hasher >
struct IdentityWrap {
    template< template< typename, typename > class T >
    using Make = T< typename Generator::Node, Hasher >;
};

template< typename Generator, typename Hasher >
struct NTreeWrap {
    template< template < typename, typename > class T >
    using Make = NTreeHashSet< T, typename Generator::Node, Hasher >;
};

struct ProviderCommon {
    WithID* _id;
    void setId( WithID& id ) { _id = &id; }

    int id() { assert( _id ); return _id->id(); }
    int owner( hash_t hash ) const { return hash % _id->peers(); }
    bool knows( hash_t hash ) const { return owner( hash ) == _id->id(); }
    ProviderCommon() : _id( nullptr ) {}
};

template< typename Provider, typename Generator, typename Hasher >
using NTreeTable = typename Provider::template Make< NTreeWrap< Generator, Hasher > >;

template< typename Provider, typename Generator, typename Hasher >
using PlainTable = typename Provider::template Make< IdentityWrap< Generator, Hasher > >;

struct PartitionedProvider {
    template < typename WrapTable >
    struct Make : ProviderCommon {
        using Table = typename WrapTable::template Make< HashSet >;
        Table _table;

        Table &table() { return _table; }
        const Table &table() const { return _table; }

        Make( typename Table::Hasher h, Make * ) : _table( h ) {}
    };
};

struct SharedProvider {
    template < typename WrapTable >
    struct Make : ProviderCommon {
        using Table = typename WrapTable::template Make< SharedHashSet >;
        using TablePtr = std::shared_ptr< Table >;

        TablePtr _table;

        Table &table() { return *_table; }
        const Table &table() const { return *_table; }

        bool knows( hash_t ) const { return true; } // we know all
        int owner( hash_t ) const {
            assert_unreachable( "no owners in shared store" );
        }

        Make ( typename Table::Hasher h, Make *master )
            : _table( master ? master->_table : std::make_shared< Table >( h ) )
        {}
    };
};

#define STORE_ITERATOR using Iterator = StoreIterator< This >; \
    Iterator begin() { return Iterator( *this, 0 ); } \
    Iterator end() { return Iterator( *this, this->table().size() ); } \
    friend class StoreIterator< This >

template < typename This >
class StoreIterator
    : std::iterator< std::input_iterator_tag, typename This::Vertex > {
    using Vertex = typename This::Vertex;
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

    Vertex operator*() {
        return store.vertex( store.table()[ i ] );
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

template< typename Store >
struct _Vertex
{
    using Handle = typename Store::Handle;
    using Node = typename Store::Node;

    template< typename T >
    T &extension( int offset = 0 ) const {
        assert( _s );
        return *reinterpret_cast< T * >( _s->extension( _h ) + offset );
    }

    Node node() const {
        if ( _s && !_s->valid( _n ) )
            _n = _s->unpack( _h );
        return _n;
    }

    void disown() { _n = typename Store::Node(); }
    Handle handle() { return _h; }

    _Vertex() : _s( nullptr ) {}
    _Vertex( Store &s, Handle h )
        : _s( &s ), _h( h )
    {}
    ~_Vertex() {
        if ( _s )
            _s->free_unpacked( _n );
    }

private:
    Store *_s;
    typename Store::Handle _h;
    mutable typename Store::Node _n;
};

struct TrivialHandle {
    Blob b;
    TrivialHandle() = default;
    explicit TrivialHandle( Blob b ) : b( b ) {}
    uint64_t asNumber() { return b.raw(); }
};

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, TrivialHandle h )
{
    return bs << h.b.raw();
}

template< typename BS >
typename BS::bitstream &operator>>( BS &bs, TrivialHandle &h )
{
    uint64_t n;
    bs >> n;
    h.b = Blob::fromRaw( n );
    return bs;
}

template < typename TableProvider,
           typename _Generator, typename _Hasher, typename Statistics >
struct DefaultStore
    : StoreCommon< PlainTable< TableProvider, _Generator, _Hasher >,
                   Statistics >
{
    using This = DefaultStore< TableProvider, _Generator, _Hasher, Statistics >;
    using Base = StoreCommon< PlainTable< TableProvider, _Generator, _Hasher >, Statistics >;
    using Hasher = _Hasher;
    using Table = typename Base::Table;

    using Node = typename Base::Node;
    using Vertex = _Vertex< This >;
    using Handle = TrivialHandle;

    void free_unpacked( Node n ) {}
    void free( Node n ) { this->pool().free( n ); }

    bool valid( Node n ) { return Base::valid( n ); }
    bool valid( Handle h ) { return this->pool().valid( h.b ); }
    bool valid( Vertex v ) { return valid( v.handle() ); }
    bool equal( Handle a, Handle b ) { return a.b.raw() == b.b.raw(); }
    bool equal( Node a, Node b ) { return Base::equal( a, b ); }

    int owner( Vertex v, hash_t hint = 0 ) { return owner( v.handle(), hint ); }
    int owner( Handle h, hash_t hint = 0 ) { return owner( h.b, hint ); }
    int owner( Node n, hash_t hint = 0 ) { return Base::owner( n, hint ); }

    int knows( Handle h, hash_t hint = 0 ) { return knows( h.b, hint ); }
    int knows( Node n, hash_t hint = 0 ) { return Base::knows( n, hint ); }

    Vertex vertex( Handle h ) { return Vertex( *this, h ); }
    Vertex vertex( Node n ) { return Vertex( *this, Handle( n ) ); } // XXX dangerous?

    /* Remember the extension for this Handle, but we won't need to get
     * successors from it. A Vertex obtained from a discarded Handle may fail
     * to produce a valid Node. */
    void discard( Handle ) {}

    Blob unpack( Handle h ) { return h.b; }

    // Vertex vertex( Node n ) { return fetch( n, hash( n ) ); }

    IsNew< Vertex > store( Node n, hash_t h = 0 ) {
        return this->_store( n, h ).map(
            [this, &n]( Node x ) {
                if ( x.raw() != n.raw() )
                    this->free( n );
                return Vertex( *this, Handle( x ) );
            } );
    }

    IsNew< Vertex > fetch( Node n, hash_t h = 0 )
    {
        return this->_fetch( n, h ).map(
            [this]( Node x ) { return Vertex( *this, Handle( x ) ); } );
    }

    template< typename T = char >
    T *extension( Handle h ) {
        return reinterpret_cast< T* >( this->pool().dereference( h.b ) );
    }

    template < typename Graph >
    DefaultStore( Graph &g, int slack, This *m = nullptr )
        : Base( Hasher( g.pool(), slack ), m )
    { }

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

#if 0
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
        Base( master, h )
    {
        static_assert( wibble::TSame< typename Graph::Node, Node >::value,
                "using incompatible graph" );
    }

    template < typename Graph >
    HcStore( Graph& g, int slack, This *m = nullptr ) :
             This( g, Hasher( g.pool(), slack ), m )
    { }

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
            pool().copy( found, s, slack() );
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
        Blob stub = this->pool().allocate( slack() + int( sizeof( hash_t ) ) );
        this->pool().copy( s, stub, slack() );
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
        pool().copy( s, stub, slack() );
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

    ptrdiff_t compareId( VertexId a, VertexId b ) {
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

    template< typename Graph, typename... Args >
    CompressedStore( Graph&, Hasher h, This *master, Args&&... args ) :
        Base( master, h, std::forward< Args >( args )... )
    {
        static_assert( wibble::TSame< typename Graph::Node, Node >::value,
                "using incompatible graph" );
    }

    std::tuple< Vertex, bool > fetch( Node node, hash_t h ) {
        TableItem found;
        bool had;
        std::tie( found, had ) = Base::_fetch( node, h );
        if ( had ) {
            pool().copy( found, node, slack() );
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
        pool().copy( node, found, slack() );
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

    ptrdiff_t compareId( VertexId a, VertexId b ) {
        return Base::_compareId( []( VertexId x ) { return x.node; }, a, b );
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
    struct Vertex : public Base::Vertex {
        Vertex( const Vertex& ) = default;
        template< typename... Args >
        explicit Vertex( Args&&... args ) :
            Base::Vertex( std::forward< Args >( args )... )
        { }

        typename Base::VertexId toQueue( Pool& ) {
            return this->getVertexId();
        }
    };
    struct VertexId : public Base::VertexId {
        VertexId( const VertexId& ) = default;
        template< typename... Args >
        explicit VertexId( Args&&... args ) :
            Base::VertexId( std::forward< Args >( args )... )
        { }
        VertexId( const typename Base::VertexId& base ) :
            Base::VertexId( base )
        { }

        typename Base::Node getNode( Pool& pool ) {
            return this->node->reassemble( pool );
        }

        Vertex fromQueue( Pool& p ) {
            if ( this->node == nullptr )
                return Vertex();
            return Vertex( this->node->reassemble( p ), this->node );
        }
    };
    using QueueVertex = VertexId;
    STORE_CLASS;
    using Root = typename Table::Root;

    _Generator& generator;

    template< typename Graph >
    NTreeStore( Graph& g, Hasher h ) : This( g, h, nullptr )
    { }

    template< typename Graph, typename... Args >
    NTreeStore( Graph& g, int slack, This *m, Args&&... args ) :
        This( g, _Hasher( g.pool(), slack ), m, std::forward< Args >( args )... )
    { }

    template< typename Graph, typename... Args >
    NTreeStore( Graph& g, Hasher h, This *m, Args&&... args ) :
        Base( g, h, m, std::forward< Args >( args )... ), generator( g.base() )
    { }

    std::tuple< Vertex, bool > store( Node node, hash_t h ) {
        Root* root;
        bool inserted;
        std::tie( root, inserted ) =
            table().insertHinted( node, h, generator );
        if ( inserted ) {
            Statistics::global().hashadded( _id->id(), memSize( node, pool() ) );
            Statistics::global().hashsize( _id->id(), table().size() );
        } else
            std::memcpy( pool().data( node ), root->slack(), slack() );
        return std::make_tuple( Vertex( node, root ), inserted );
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

    ptrdiff_t compareId( VertexId a, VertexId b ) {
        // either way ids in algoritms are already permanent, so we don't
        // need to look inside
        return a.node - b.node;
    }

    STORE_ITERATOR;
};
#endif

}
}
#undef STORE_CLASS
#undef STORE_ITERATOR
#endif
