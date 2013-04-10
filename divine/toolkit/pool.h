// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // for assert
#include <vector>
#include <iostream>

#include <divine/toolkit/blob.h>

#ifndef DIVINE_POOL_H
#define DIVINE_POOL_H

namespace divine {

struct BlobDereference {

    BlobHeader& header( Blob& blob ) const
    {
        return blob.header();
    }

    const BlobHeader& header( const Blob& blob ) const
    {
        return blob.header();
    }

    template< typename T >
    T& get( Blob blob, int off = 0 )
    {
        return blob.get<T>(off);
    }

    template< typename T >
    const T& get( const Blob blob, int off = 0 ) const
    {
        return blob.get< T >( off );
    }

    template< typename T >
    int get( Blob blob, int off, T& t ) const
    {
        return blob.get(off, t);
    }

    template< typename T >
    int put( Blob& blob, int off, T t ) const
    {
        return blob.put(off, t);
    }

    void copyTo( const Blob& source, Blob& where ) const
    {
        source.copyTo( where );
    }

    inline void copyTo( const Blob& source, Blob& where, int length )
    {
        copyTo( source, where, 0, 0, length );
    }

    void copyTo( const Blob& source, Blob& where, int sourceStart,
            int whereStart, int length )
    {
        source.copyTo( where, sourceStart, whereStart, length );
    }

    template< typename O >
    O write32( const Blob& blob, O o ) const
    {
        return blob.write32( o );
    }

    void setSize( Blob& blob, size_t size ) const
    {
        blob.setSize( size );
    }

    void clear( Blob& blob, int from = 0, int to = 0, char pattern = 0 ) const
    {
        blob.clear( from, to, pattern );
    }

    int size( const Blob& blob ) const
    {
        return blob.size();
    }

    char* data( const Blob& blob ) const
    {
        return blob.data();
    }

    char* pointer( const Blob blob ) const
    {
        return blob.pointer();
    }

    int32_t* pointer32( const Blob& blob ) const
    {
        return blob.pointer32();
    }

    int compare( const Blob& x, const Blob& y, int b, int e ) const
    {
        return x.compare( y, b, e );
    }

    int compare( const Blob& a, const Blob& b ) const
    {
        return compare( a, b, 0, std::max( size( a ), size( b ) ) );
    }

    int equal( const Blob& x, const Blob& y ) const
    {
        int cmp = compare( x, y, 0, std::max( size( x ), size( y ) ) );
        return cmp == 0;
    }

#ifndef DIVINE_EMBED
    hash_t hash( const Blob& blob ) const
    {
        return blob.hash();
    }

    hash_t hash( const Blob& blob, int from, int to, uint32_t salt = 0 ) const
    {
        return blob.hash( from, to, salt );
    }

#endif

};

struct FakePool : public BlobDereference {

    FakePool() {}
    FakePool( const FakePool & ) {}
    ~FakePool() {}

    char *allocate( size_t s ) {
        return new char[ s ];
    }

    template< typename T >
    void deallocate( T *_ptr, size_t size = 0 )
    {
        return steal( _ptr, size );
    }

    template< typename T >
    void free( T *_ptr, size_t size = 0 )
    {
        delete[] static_cast< char * >( _ptr );
    }

    template< typename T >
    void steal( T *_ptr, size_t size = 0 ) {
        delete[] static_cast< char * >( _ptr );
    }

    void free( Blob& blob )
    {
        blob.free( *this );
    }

};

#ifndef O_POOLS

typedef FakePool Pool;

#else

struct Pool : public BlobDereference {
    struct Block
    {
        size_t size;
        char *start;
        Block() : size( 0 ), start( 0 ) {}
    };

    struct Group
    {
        int item;
        int used, total; /* XXX: Deprecated, but part of CESMI ABI. */
        char **free; // reuse allocation
        char *current; // tail allocation
        char *last; // bumper
        std::vector< Block > blocks;
        Group()
            : item( 0 ), used( 0 ), total( 0 ),
              free( 0 ), current( 0 ), last( 0 )
        {}
    };

    // for FFI
    int m_groupCount;
    int m_groupStructSize;
    Group *m_groupArray;

    typedef std::vector< Group > Groups;
    Groups m_groups;

    Pool();
    Pool( const Pool & );
    ~Pool();

    // ignore assignments
    Pool &operator=( const Pool & ) { return *this; }

    void newBlock( Group *g )
    {
        Block b;
        int s = std::min( 4 * int( g->blocks.back().size ),
                          4 * 1024 * 1024 );
        b.size = s;
        b.start = new char[ s ];
        g->current = b.start;
        g->last = b.start + s;
        g->blocks.push_back( b );
    }

    int adjustSize( int bytes )
    {
        // round up to a value divisible by 4
        bytes += 3;
        bytes = ~bytes;
        bytes |= 3;
        bytes = ~bytes;
        return bytes;
    }

    Group *group( int bytes )
    {
        if ( bytes / 4 >= int( m_groups.size() ) )
            return 0;
        assert_eq( bytes % 4, 0 );
        if ( m_groups[ bytes / 4 ].total )
            return &(m_groups[ bytes / 4 ]);
        else
            return 0;
    }

    void ffi_update() {
        m_groupCount = m_groups.size();
        m_groupArray = &m_groups.front();
    }

    Group *createGroup( int bytes )
    {
        assert( bytes % 4 == 0 );
        Group g;
        g.item = bytes;
        Block b;
        b.size = std::min( std::max( 1024 * 1024, bytes ), 4096 * bytes );
        b.start = new char[ b.size ];
        g.blocks.push_back( b );
        g.current = b.start;
        g.last = b.start + b.size;
        g.total = 1;
        m_groups.resize( std::max( bytes / 4 + 1, int( m_groups.size() ) ), Group() );

        ffi_update();

        assert_eq( m_groups[ bytes / 4 ].total, 0 );
        m_groups[ bytes / 4 ] = g;
        assert_neq( g.total, 0 );
        assert( group( bytes ) );
        return group( bytes );
    }

    char *allocate( size_t bytes )
    {
        assert( bytes );
        bytes = adjustSize( bytes );
        char *ret = 0;

        Group *g = group( bytes );
        if (!g)
            g = createGroup( bytes );

        if ( g->free ) {
            ret = reinterpret_cast< char * >( g->free );
            g->free = *reinterpret_cast< char *** >( ret );
        } else {
            if ( g->current + bytes > g->last ) {
                newBlock( g );
            }
            ret = g->current;
            g->current += bytes;
        }

        return ret;
    }

    size_t pointerSize( char * )
    {
        assert_die();
        return 0; // replace with magic
    }

    void release( Group *g, char *ptr, int size )
    {
        assert( g );
        assert_eq( g->item, size );
        char ***ptr3 = reinterpret_cast< char *** >( ptr );
        char **ptr2 = reinterpret_cast< char ** >( ptr );
        *ptr3 = g->free;
        g->free = ptr2;
    }

    template< typename T >
    void free( T *_ptr, size_t size = 0 )
    {   // O(1) free
        if ( !_ptr ) return; // noop
        char *ptr = reinterpret_cast< char * >( _ptr );
        if ( !size )
            size = pointerSize( ptr );
        else {
            assert( size );
            size = adjustSize( size );
        }
        assert( size );
        Group *g = group( size );
        release( g, ptr, size );
    }

    template< typename T >
    void steal( T *_ptr, size_t size = 0 )
    {
        char *ptr = reinterpret_cast< char * >( _ptr );
        if ( !size )
            size = pointerSize( ptr );
        else {
            assert( size );
            size = adjustSize( size );
        }
        assert( size );
        Group *g = group( size );
        if ( !g )
            g = createGroup( size );
        assert( g );
        release( g, ptr, size );
    }

    template< typename T >
    void deallocate( T *_ptr, size_t size = 0 )
    {
        return steal( _ptr, size );
    }

    void free( Blob& blob )
    {
        blob.free( *this );
    }

};

#endif


// Since < and == operator of blob is to be removed, we want comparer which will work with STL
struct BlobComparerBase {
protected:
    const Pool* m_pool;

    BlobComparerBase( Pool& pool ) : m_pool( &pool )
    { }
};

struct BlobComparerEQ : public BlobComparerBase {

    BlobComparerEQ( Pool& pool ) : BlobComparerBase( pool )
    { }

    bool operator()( const Blob& a, const Blob& b ) const {
        return m_pool->equal( a, b );
    }

    template< typename T >
    bool operator()( const std::pair<Blob, T>& a, const std::pair<Blob, T>& b) const {
        return m_pool->equal( a.first, b.first ) && a.second == b.second;
    }

    bool operator()( const std::tuple<>&, const std::tuple<>& ) const {
        return true;
    }

    template< typename... TS >
    bool operator()( const std::tuple< TS... >& a, const std::tuple< TS... >& b ) const {
        return compareTuple< sizeof...( TS ), 0, TS... >( a, b );
    }

    template< size_t size, size_t i, typename... TS >
    typename std::enable_if< ( size > i ), bool >::type compareTuple(
            const std::tuple< TS... >& a, const std::tuple< TS... >& b ) const
    {
        bool x = (*this)( std::get< i >( a ), std::get< i >( b ) );
        return x
            ? compareTuple< size, i + 1, TS... >( a, b )
            : false;
    }

    template< size_t size, size_t i, typename... TS >
    typename std::enable_if< ( size <= i ), bool >::type compareTuple(
            const std::tuple< TS... >&, const std::tuple< TS... >& ) const
    {
        return true;
    }

    template< typename T >
    bool operator()( const T& a, const T& b ) const {
        return a == b;
    }
};

struct BlobComparerLT : public BlobComparerBase {
    const BlobComparerEQ eq;

    BlobComparerLT( Pool& pool ) : BlobComparerBase( pool ), eq( pool )
    { }

    bool operator()( const Blob& a, const Blob& b ) const {
        int cmp = m_pool->compare( a, b );
        return cmp < 0;
    }

    // lexicongraphic tuple ordering
    template< typename T >
    bool operator()( const std::pair<Blob, T>& a, const std::pair<Blob, T>& b) const {
        int cmp = m_pool->compare( a.first, b.first );
        return cmp == 0 ? a.second < b.second : cmp < 0;
    }

    bool operator()( const std::tuple<>&, const std::tuple<>& ) const {
        return false;
    }

    template< typename... TS >
    bool operator()( const std::tuple< TS... >& a, const std::tuple< TS... >& b ) const {
        return compareTuple< sizeof...( TS ), 0, TS... >( a, b );
    }

    template< size_t size, size_t i, typename... TS >
    typename std::enable_if< ( size > i ), bool >::type compareTuple(
            const std::tuple< TS... >& a, const std::tuple< TS... >& b ) const
    {
        bool x = eq( std::get< i >( a ), std::get< i >( b ) );
        return x
            ? compareTuple< size, i + 1, TS... >( a, b )
            : (*this)( std::get< i >( a ), std::get< i >( b ) );
    }

    template< size_t size, size_t i, typename... TS >
    typename std::enable_if< ( size <= i ), bool >::type compareTuple(
            const std::tuple< TS... >&, const std::tuple< TS... >& ) const
    {
        return false;
    }

    template< typename T >
    bool operator()( const T& a, const T& b ) const {
        return a < b;
    }
};


std::ostream &operator<<( std::ostream &o, const Pool &p );

}

namespace std {

template<>
inline void swap< divine::Pool >( divine::Pool &a, divine::Pool &b )
{
#ifdef O_POOLS
    swap( a.m_groups, b.m_groups );
#endif
}

}

#endif
