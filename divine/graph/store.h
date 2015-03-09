// -*- C++ -*-
//             (c) 2013, 2014 Vladimír Štill <xstill@fi.muni.cz>
#include <utility>
#include <memory>
#include <iterator>
#include <tuple>

#include <brick-hashset.h>

#include <divine/toolkit/ntreehashset.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/parallel.h> // for WithID

#ifndef DIVINE_STORE_H
#define DIVINE_STORE_H

namespace divine {
namespace visitor {

namespace hashset = brick::hashset;
using hashset::Found;
using hashset::isNew;
using brick::types::Preferred;
using brick::types::NotPreferred;

/* Global utility functions independent of store type
 */

template < typename TableProvider, typename Statistics >
struct StoreCommon : TableProvider
{
    using Table = typename TableProvider::Table;
    using Hasher = typename TableProvider::Hasher;
    using InsertItem = typename TableProvider::Node;
    using StoredItem = typename Table::value_type;

    typename TableProvider::ThreadData td;

    template< typename... Args >
    StoreCommon( Pool &p, Hasher h, Args&&... args )
        : TableProvider( h, std::forward< Args >( args )... ), _pool( p ), _hasher( h ) {}

    StoreCommon( const StoreCommon & ) = default;
    StoreCommon( StoreCommon && ) = default;
    StoreCommon &operator=( const StoreCommon & ) { ASSERT_UNIMPLEMENTED(); }

    Pool &_pool;
    Hasher _hasher;

    Hasher& hasher() { return _hasher; } // this->table().hasher; }
    Pool& pool() { return _pool; }

    hash64_t hash( InsertItem node ) { return hash128( node ).first; }
    hash128_t hash128( InsertItem node ) { return hasher().hash( node ); }
    int slack() { return hasher().slack; }
    bool valid( InsertItem n ) { return hasher().valid( n ); }
    bool equal( InsertItem m, InsertItem n ) { return hasher().equal( m, n ); }
    bool has( InsertItem n ) { return this->table().has( n ); }
    void setSize( intptr_t size ) { this->table().setSize( size ); }

    int owner( hash64_t h ) { return TableProvider::owner( h ); }
    int owner( InsertItem n, hash64_t hint = 0 ) {
        return owner( hint ? hint : hash128( n ).second );
    }

    bool knows( hash64_t h ) { return TableProvider::knows( h ); }
    bool knows( InsertItem n, hash64_t hint = 0 ) {
        return TableProvider::knows( hint ? hint : hash128( n ).second );
    }

    Found< StoredItem > _fetch( InsertItem s, hash64_t h ) {
        auto found = this->table().withTD( td ).findHinted( s, h ? h : hash( s ) );
        return isNew( found.valid() ? *found : StoredItem(), found.isnew() );
    }

    Found< StoredItem > _store( InsertItem s, hash64_t h ) {
        auto found = this->table().withTD( td ).insertHinted( s, h ? h : hash( s ) );
        if ( found.isnew() ) {
            Statistics::global().hashadded( this->id(), memSize( s, hasher().pool() ) );
            Statistics::global().hashsize( this->id(), this->table().size() );
        }
        return isNew( *found, found.isnew() );
    }
};

template< typename Generator, typename H >
struct IdentityWrap {
    using Node = typename Generator::Node;
    using Hasher = H;

    template< template< typename, typename > class T >
    using Make = T< Node, H >;

    template< template < typename, typename > class T >
    using ThreadData = typename T< Node, H >::ThreadData;
};

template< typename Generator, typename H >
struct NTreeWrap {
    using Node = typename Generator::Node;
    using Hasher = H;

    template< template < typename, typename > class T >
    using Make = NTreeHashSet< T, Node, H >;

    template< template < typename, typename > class T >
    using ThreadData = typename NTreeHashSet< T, Node, H >::template ThreadData< Generator >;
};

struct ProviderCommon {
    WithID _id;
    void setId( WithID& id ) { _id = id; }

    int localId() const { return _id.localId(); }
    int id() const { return _id.id(); }
    int rank() const { return _id.rank(); }
    int locals() const { return _id.locals(); }
    int owner( hash64_t hash ) const { return hash % _id.peers(); }
    bool knows( hash64_t hash ) const { return owner( hash ) == _id.id(); }
    ProviderCommon() : _id( { 0, 0 }, 1, 1, 0 ) { }
};

template< typename Provider, typename Generator, typename Hasher >
using NTreeTable = typename Provider::template Make< NTreeWrap< Generator, Hasher > >;

template< typename Provider, typename Generator, typename Hasher >
using PlainTable = typename Provider::template Make< IdentityWrap< Generator, Hasher > >;

struct PartitionedProvider {
    template < typename WrapTable >
    struct Make : ProviderCommon, WrapTable  {
        using Table = typename WrapTable::template Make< hashset::Fast >;
        using ThreadData = typename WrapTable::template ThreadData< hashset::Fast >;
        using Hasher = typename WrapTable::Hasher;

        template< typename Mutex >
        struct Guard {
            template< typename V > Guard( V ) {}
            template< typename V > Guard( V, V ) {}
        };

        template< typename T >
        using DataWrapper = T;

        Table _table;

        Table &table() { return _table; }
        const Table &table() const { return _table; }

        size_t firstIndex() { return 0; }
        size_t lastIndex() { return table().size(); }

        Make( Hasher h, Make * ) : _table( h ) {}
    };
};

struct SharedProvider {
    template < typename WrapTable >
    struct Make : ProviderCommon, WrapTable {
        using Table = typename WrapTable::template Make< hashset::Concurrent >;
        using TablePtr = std::shared_ptr< Table >;
        using ThreadData = typename WrapTable::template ThreadData< hashset::Concurrent >;
        using Hasher = typename WrapTable::Hasher;

        template< typename Mutex >
        struct Guard {
            Mutex *_1;
            Mutex *_2;

            template< typename V >
            Guard( V v ) : Guard( v.valid() ? &v.template extension< Mutex >() : nullptr, nullptr ) {}

            template< typename V >
            Guard( V a, V b ) : Guard( a.valid() ? &a.template extension< Mutex >() : nullptr,
                                       b.valid() ? &b.template extension< Mutex >() : nullptr ) {}

            Guard( Mutex *m1, Mutex *m2 ) : _1( first( m1, m2 ) ), _2( second( m1, m2 ) ) {
                if ( _1 ) _1->lock();
                if ( _2 ) _2->lock();
            }

            ~Guard() {
                if ( _2 ) _2->unlock();
                if ( _1 ) _1->unlock();
            }

            static Mutex *first( Mutex *m1, Mutex *m2 ) {
                return m1 < m2 ? m1 : m2;
            }
            static Mutex *second( Mutex *m1, Mutex *m2 ) {
                if ( m1 == m2 )
                    return nullptr;
                return m1 < m2 ? m2 : m1;
            }

            Guard( const Guard & ) = delete;
            Guard( Guard && ) = delete;
        };

        template< typename T >
        using DataWrapper = std::atomic< T >;

        TablePtr _table;

        Table &table() { return *_table; }
        const Table &table() const { return *_table; }

        bool knows( hash64_t ) const { return true; } // we know all
        int owner( hash64_t ) const {
            ASSERT_UNREACHABLE( "no owners in shared store" );
        }

        size_t firstIndex() {
            return table().size() / locals() * localId();
        }

        size_t lastIndex() {
            return std::min(
                table().size() / locals() * ( localId() + 1 ),
                table().size() );
        }

        Make ( Hasher h, Make *master )
            : _table( master ? master->_table : std::make_shared< Table >( h ) )
        {}
    };
};

#define STORE_ITERATOR using Iterator = StoreIterator< This >; \
    Iterator begin() { return Iterator( *this, this->firstIndex() ); } \
    Iterator end() { return Iterator( *this, this->lastIndex() ); } \
    friend class StoreIterator< This >

template < typename This >
class StoreIterator
    : std::iterator< std::input_iterator_tag, typename This::Vertex > {
    using Vertex = typename This::Vertex;
    size_t i;
    This& store;

    void bump() {
        while ( i < store.table().size() && !store.table().valid( i ) )
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
        ASSERT( _s );
        return *reinterpret_cast< T * >(
                ( foreign() ? _s->pool().dereference( _n ) : _s->extension( _h ) )
                + offset );
    }

    Node node() const {
        if ( _s && !foreign() && !_s->valid( _n ) && _s->valid( _h ) )
            _n = _s->unpack( _h, _p );
        _clearTag( _n, Preferred() ); // Nodes must not be tagged so they can be easily compared
        return _n;
    }

    void disown() { if ( !foreign() ) _n = Node(); }
    bool valid() const { return _s && _s->valid( _h ); }
    Handle handle() const { return _h; }
    // belongs to another machine -> cannot be dereferenced...
    bool foreign() const { return _foreign( _h, Preferred() ); }
    void initForeign( Store& s ) {
        if ( _s == nullptr )
            _s = &s;
    }
    void deleteForeign() {
        if ( foreign() ) {
            _s->free_unpacked( _n, _p, true );
            _h = Handle();
            _n = Node();
            _s = nullptr;
        }
    }
    void setPool( Pool& p ) {
        _p = &p;
    }

    _Vertex() : _s( nullptr ), _p( nullptr ) {}
    _Vertex( const _Vertex &v ) : _s( v._s ), _h( v._h ), _p( v._p ) {
        // if Vertex is not local that value should not be thrown away as we
        // cannot recreate it
        if ( !v._s || v.foreign() )
            _n = v._n;
    }
    _Vertex( Store &s, Handle h )
        : _s( &s ), _h( h ), _p( &s.pool() )
    {}
    ~_Vertex() {
        if ( _s && !foreign() )
            _s->free_unpacked( _n, _p, false );
    }
    _Vertex &operator=( const _Vertex &x ) {
        if ( _s && !foreign() )
            _s->free_unpacked( _n, _p, false );
        _p = x._p;
        _s = x._s;
        _h = x._h;
        if ( x._s && !x.foreign() )
            _n = Node();
        else
            _n = x._n;
        return *this;
    }

    template< typename BS >
    friend typename BS::bitstream &operator<<( BS &bs, _Vertex v ) {
        return bs << v._h << v.node(); // decompress node if necessary
    }

    template< typename BS >
    friend typename BS::bitstream &operator>>( BS &bs, _Vertex &v ) {
        v._s = nullptr;
        return bs >> v._h >> v._n;
    }

private:
    template< typename H >
    inline auto _foreign( const H &handle, Preferred ) const ->
        typename std::enable_if< (H::Base::tagBits > 0), bool >::type
    {
        ASSERT( _s ); return handle.rank() != _s->rank();
    }

    template< typename H >
    inline bool _foreign( const H &, NotPreferred ) const {
        return false;
    }

    template< typename N >
    inline auto _clearTag( N &node, Preferred ) const ->
        typename std::enable_if< (N::tagBits > 0) >::type
    {
        node.setTag( 0 );
    }

    template< typename N >
    inline void _clearTag( N &, NotPreferred ) const { }

    Store *_s; // origin store
    Handle _h;
    Pool *_p; // local pool
    mutable Node _n;
};

struct TrivialHandle {
    using Base = Blob;

    Blob b;
    TrivialHandle() = default;
    explicit TrivialHandle( Blob blob, int rank ) : b( blob ) {
        _setRank( b, rank, Preferred() );
    }
    uint64_t asNumber() { return b.raw(); }
    int rank() const { return _getRank( b, Preferred() ); }

  private:
    template< typename T >
    inline auto _setRank( T &b, int rank, Preferred ) ->
        typename std::enable_if< (T::tagBits > 0) >::type
    { b.setTag( rank ); }

    template< typename T >
    inline void _setRank( T &, int, NotPreferred ) { }

    template< typename T >
    inline auto _getRank( const T &b, Preferred ) const ->
        typename std::enable_if< (T::tagBits > 0), int >::type
    { return b.tag(); }

    template< typename T >
    inline int _getRank( const T &, NotPreferred ) const { return 0; }

} __attribute__((packed));

template< typename BS >
typename BS::bitstream &operator<<( BS &bs, TrivialHandle h )
{
    return bs << h.b.raw();
}

template< typename BS >
typename BS::bitstream &operator>>( BS &bs, TrivialHandle &h )
{
    Blob::Raw n;
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

    using Node = typename Base::InsertItem;
    using Vertex = _Vertex< This >;
    using Handle = TrivialHandle;

    void free_unpacked( Node n, Pool *p, bool foreign ) {
        if ( foreign ) {
            ASSERT( p );
            p->free( n );
        }
    }
    void free( Node n ) { this->pool().free( n ); }

    bool valid( Node n ) { return Base::valid( n ); }
    bool valid( Handle h ) { return this->pool().valid( h.b ); }
    bool valid( Vertex v ) { return valid( v.handle() ); }
    bool equal( Handle a, Handle b ) { return a.asNumber() == b.asNumber(); }
    bool equal( Node a, Node b ) { return Base::equal( a, b ); }

    int owner( Vertex v, hash64_t hint = 0 ) { return owner( v.node(), hint ); }
    int owner( Node n, hash64_t hint = 0 ) { return Base::owner( n, hint ); }

    int knows( Handle h, hash64_t hint = 0 ) {
        return h.rank() == this->rank() && knows( h.b, hint );
    }
    int knows( Node n, hash64_t hint = 0 ) { return Base::knows( n, hint ); }

    Vertex vertex( Handle h ) { return Vertex( *this, h ); }

    /* Remember the extension for this Handle, but we won't need to get
     * successors from it. A Vertex obtained from a discarded Handle may fail
     * to produce a valid Node. */
    void discard( Handle ) {}

    Blob unpack( Handle h, Pool * ) { return h.b; }

    Found< Vertex > store( Node n, hash64_t h = 0 ) {
        return fmap( [this, &n]( Node x ) {
                if ( x.raw_address() != n.raw_address() )
                    this->free( n );
                return this->vertex( x );
            }, this->_store( n, h ) );
    }

    Found< Vertex > fetch( Node n, hash64_t h = 0 )
    {
        return fmap( [this]( Node x ) { return this->vertex( x ); },
                     this->_fetch( n, h ) );
    }

    template< typename T = char >
    T *extension( Handle h ) {
        return reinterpret_cast< T* >( this->pool().dereference( h.b ) );
    }

    template < typename Graph >
    DefaultStore( Graph &g, int slack, This *m = nullptr )
        : Base( g.pool(), Hasher( g.pool(), slack ), m )
    { }

    STORE_ITERATOR;
private:
    Vertex vertex( Node n ) { return Vertex( *this, Handle( n, this->rank() ) ); }
};

template< typename Hasher >
struct HcHasher : Hasher
{
    HcHasher( Pool& pool, int slack ) : Hasher( pool, slack ) {}
    bool equal( Blob, Blob ) { return true; }
};

template < typename TableProvider,
           typename _Generator, typename _Hasher, typename Statistics >
struct HcStore
    : StoreCommon< PlainTable< TableProvider, _Generator, HcHasher< _Hasher > >,
                   Statistics >
{
    using Hasher = HcHasher< _Hasher >;
    using This = HcStore< TableProvider, _Generator, _Hasher, Statistics >;
    using Base = StoreCommon< PlainTable< TableProvider, _Generator, Hasher >, Statistics >;
    using Table = typename Base::Table;

    using Node = typename Base::InsertItem;
    using Vertex = _Vertex< This >;
    using Handle = TrivialHandle;

    static_assert( std::is_same< Node, Blob >::value,
                   "HcStore can only work with Blob nodes" );

    hash64_t& stubHash( Blob stub ) {
        return this->pool().template get< hash64_t >( stub, this->slack() );
    }

    void free_unpacked( Node n, Pool *p, bool foreign ) {
        if ( foreign ) {
            ASSERT( p );
            p->free( n );
        }
    }
    void free( Node n ) { this->pool().free( n ); }

    bool valid( Node n ) { return Base::valid( n ); }
    bool valid( Handle h ) { return this->pool().valid( h.b ); }
    bool valid( Vertex v ) { return valid( v.handle() ); }
    bool equal( Handle a, Handle b ) { return a.asNumber() == b.asNumber(); }
    bool equal( Node a, Node b ) { return Base::equal( a, b ); }

    int owner( Vertex v, hash64_t hint = 0 ) { return owner( v.node(), hint ); }
    int owner( Node n, hash64_t hint = 0 ) { return Base::owner( n, hint ); }

    int knows( Handle h, hash64_t hint = 0 ) {
        return h.rank() == this->rank() && knows( h.b, hint );
    }
    int knows( Node n, hash64_t hint = 0 ) { return Base::knows( n, hint ); }

    Vertex vertex( Handle h ) { return Vertex( *this, h ); }

    /* Remember the extension for this Handle, but we won't need to get
     * successors from it. A Vertex obtained from a discarded Handle may fail
     * to produce a valid Node. */
    void discard( Handle ) {}

    Blob unpack( Handle h, Pool * ) { return h.b; }

    Found< Vertex > store( Node n, hash64_t h = 0 ) {
        return fmap( [this, &n]( Node x ) {
                if ( x.raw_address() != n.raw_address() )
                    this->free( n );
                return this->vertex( x );
            }, this->_store( n, h ) );
    }

    Found< Vertex > fetch( Node n, hash64_t h = 0 )
    {
        return fmap( [this]( Node x ) { return this->vertex( x ); },
                     this->_fetch( n, h ) );
    }

    template< typename T = char >
    T *extension( Handle h ) {
        return reinterpret_cast< T* >( this->pool().dereference( h.b ) );
    }

    template < typename Graph >
    HcStore( Graph &g, int slack, This *m = nullptr )
        : Base( g.pool(), Hasher( g.pool(), slack ), m )
    { }

    STORE_ITERATOR;
private:
    Vertex vertex( Node n ) { return Vertex( *this, Handle( n, this->rank() ) ); }

#if 0
    IsNew< Vertex > fetch( Blob s, hash64_t h = 0 ) {
        return this->_fetch( s, h ).map(
            [this, s]( Blob found ) {
                this->pool().copy( found, s, this->slack() );
                return this->vertex( s );
            } );
    }

    IsNew< Vertex > store( Blob s, hash64_t h = 0 ) {
        // store just a stub containing state information
        Blob stub = this->pool().allocate( this->slack() + int( sizeof( hash64_t ) ) );
        this->pool().copy( s, stub, this->slack() );
        stubHash( stub ) = h;
        Node n;
        bool inserted;
        std::tie( n, inserted ) = Base::_store( stub, h );
        if ( !inserted )
            this->pool().free( stub );
        ASSERT( equal( s, stub ) );
        ASSERT)UNIMPLEMENTED(); // return std::make_tuple( Vertex( s, n ), inserted );
    }
#endif
};

template < typename TableProvider,
           typename _Generator, typename _Hasher, typename Statistics >
struct NTreeStore
    : StoreCommon< NTreeTable< TableProvider, _Generator, _Hasher >,
                   Statistics >
{
    using This = NTreeStore< TableProvider, _Generator, _Hasher, Statistics >;
    using Base = StoreCommon< NTreeTable< TableProvider, _Generator, _Hasher >, Statistics >;
    using Hasher = _Hasher;
    using Table = typename Base::Table;

    using Node = typename Base::InsertItem;
    using Vertex = _Vertex< This >;
    using Handle = TrivialHandle;

    using Root = typename Table::Root;

    template< typename Graph >
    NTreeStore( Graph& g, int slack, This *m = nullptr ) :
        Base( g.pool(), Hasher( g.pool(), slack ), m )
    {
        this->td._splitter = &g.base();
        this->td._pool = &g.pool();
    }

    Found< Vertex > store( Node n, hash64_t h = 0 ) {
        return fmap( [this, &n]( Root x ) {
                this->free( n );
                return this->vertex( x );
            }, this->_store( n, h ) );
    }

    Found< Vertex > fetch( Node n, hash64_t h = 0 )
    {
        return fmap( [this]( Root x ) { return this->vertex( x ); },
                     this->_fetch( n, h ) );
    }

    Blob unpack( Handle h, Pool *p ) {
        ASSERT( p );
        return Root( h.b ).reassemble( *p, this->slack() );
    }

    void free_unpacked( Node n, Pool *p, bool ) { ASSERT( p ); p->free( n ); }
    void free( Node n ) { this->pool().free( n ); }

    bool valid( Node n ) { return Base::valid( n ); }
    bool valid( Handle h ) { return this->pool().valid( h.b ); }
    bool valid( Vertex v ) { return valid( v.handle() ); }
    bool equal( Handle a, Handle b ) { return a.asNumber() == b.asNumber(); }
    bool equal( Node a, Node b ) { return Base::equal( a, b ); }

    hash64_t hash( Node n ) { return hash128( n ).first; }
    hash64_t hash( Handle n ) { return hash128( n ).first; }

    hash128_t hash128( Node n ) { return Base::hash128( n ); }
    hash128_t hash128( Handle h ) { return this->table().hash128( Root( h.b ) ); }

    int owner( Vertex v, hash64_t hint = 0 ) {
        return Base::owner( hint ? hint : hash128( v.node() ).second );
    }
    int owner( Node n, hash64_t hint = 0 ) { return Base::owner( n, hint ); }

    int knows( Handle h, hash64_t hint = 0 ) {
        return h.rank() == this->rank() && Base::knows( hint ? hint : hash128( h ).second );
    }
    int knows( Node n, hash64_t hint = 0 ) { return Base::knows( n, hint ); }

    Vertex vertex( Handle h ) { return Vertex( *this, h ); }

    template< typename T = char >
    T *extension( Handle h ) {
        return reinterpret_cast< T* >( Root( h.b ).slack( this->pool(), this->slack() ) );
    }

    STORE_ITERATOR;
private:
    Vertex vertex( Root n ) { return Vertex( *this, Handle( n.unwrap(), this->rank() ) ); }
};

}
}
#undef STORE_CLASS
#undef STORE_ITERATOR
#endif
