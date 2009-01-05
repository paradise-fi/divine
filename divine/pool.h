// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // for assert
#include <wibble/sys/mutex.h> // for assert
#include <pthread.h>
#include <deque>
#include <iostream>

#ifndef DIVINE_POOL_H
#define DIVINE_POOL_H


namespace divine {

#ifdef DISABLE_POOLS
struct Pool {

    Pool();
    Pool( const Pool & );
    ~Pool();

    char *alloc( size_t s ) {
        return new char[ s ];
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
};

#else

struct Pool {
    struct Block
    {
        size_t size;
        char *start;
        Block() : size( 0 ), start( 0 ) {}
    };

    struct Group
    {
        size_t item, used, total;
        char **free; // reuse allocation
        char *current; // tail allocation
        char *last; // bumper
        std::vector< Block > blocks;
        Group()
            : item( 0 ), used( 0 ), total( 0 ),
              free( 0 ), current( 0 ), last( 0 )
        {}
    };

    typedef std::vector< Group > Groups;
    size_t m_groupCount; // for FFI
    Groups m_groups;

    Pool();
    Pool( const Pool & );
    ~Pool();

    void newBlock( Group *g )
    {
        Block b;
        size_t s = std::min( 4 * g->blocks.back().size,
                             size_t( 4 * 1024 * 1024 ) );
        b.size = s;
        b.start = new char[ s ];
        g->current = b.start;
        g->last = b.start + s;
        g->total += s;
        g->blocks.push_back( b );
    }

    size_t adjustSize( size_t bytes )
    {
        // round up to a value divisible by 4
        bytes += 3;
        bytes = ~bytes;
        bytes |= 3;
        bytes = ~bytes;
        return bytes;
    }

    Group *group( size_t bytes )
    {
        if ( bytes / 4 >= m_groups.size() )
            return 0;
        assert_eq( bytes % 4, 0 );
        if ( m_groups[ bytes / 4 ].total )
            return &(m_groups[ bytes / 4 ]);
        else
            return 0;
    }

    Group *createGroup( size_t bytes )
    {
        assert( bytes % 4 == 0 );
        Group g;
        g.item = bytes;
        Block b;
        b.size = 4096 * bytes;
        b.start = new char[ b.size ];
        g.blocks.push_back( b );
        g.current = b.start;
        g.last = b.start + b.size;
        g.total = b.size;
        m_groups.resize( std::max( bytes / 4 + 1, m_groups.size() ), Group() );
        m_groupCount = m_groups.size();
        assert_eq( m_groups[ bytes / 4 ].total, 0 );
        m_groups[ bytes / 4 ] = g;
        assert_neq( g.total, 0 );
        assert( group( bytes ) );
        assert_eq( group( bytes )->total, g.total );
        return group( bytes );
    }

    char *alloc( size_t bytes )
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
        g->used += bytes;
        assert( g->used <= g->total );

        return ret;
    }

    size_t pointerSize( char *_ptr )
    {
        return 0; // replace with magic
    }

    void release( Group *g, char *ptr, size_t size )
    {
        assert( g );
        assert( g->item == size );
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
        assert( g->used >= size );
        g->used -= size;
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
        g->total += size;
        release( g, ptr, size );
    }
};

#endif

std::ostream &operator<<( std::ostream &o, const Pool &p );

struct GlobalPools {
    pthread_key_t m_pool_key;
    typedef std::deque< Pool * > Available;
    typedef std::vector< Pool * > Vector;
    Available m_available;
    wibble::sys::Mutex m_mutex;

    static GlobalPools *s_instance;
    static pthread_once_t s_once;

    static GlobalPools &instance();
    static Pool* getSpecific();
    static void setSpecific( Pool* );

    static wibble::sys::Mutex &mutex() {
        return instance().m_mutex;
    }

    static Available &available() {
        return instance().m_available;
    }

    GlobalPools() : m_mutex( true ) {}

    static void pool_key_reclaim( void *p );
    static void init_instance();

    static void add( Pool *p );
    static void remove( Pool *p );
    static Pool *force( Pool *p );
    static Pool *get();
};

template< typename T >
class Allocator
{
public:
    typedef size_t     size_type;
    typedef ptrdiff_t  difference_type;
    typedef T*         pointer;
    typedef const T*   const_pointer;
    typedef T&         reference;
    typedef const T&   const_reference;
    typedef T          value_type;

    template<typename T1>
    struct rebind
    { typedef Allocator<T1> other; };

    Pool *m_pool;

    Allocator() throw() {
        m_pool = GlobalPools::get();
    }

    Allocator(const Allocator&o) throw() {
        m_pool = o.m_pool;
    }

    template<typename T1>
    Allocator(const Allocator<T1> &o) throw() {
        m_pool = o.m_pool;
    }

    ~Allocator() throw() {}

    pointer address(reference x) const { return &x; }
    const_pointer address(const_reference x) const { return &x; }

    // NB: __n is permitted to be 0.  The C++ standard says nothing
    // about what the return value is when __n == 0.
    pointer allocate( size_type n, const void* = 0 )
    { 
        return m_pool->alloc( sizeof( T ) * n );
    }

    // __p is not permitted to be a null pointer.
    void deallocate( pointer __p, size_type n )
    {
        return m_pool->steal( __p, sizeof( T ) * n );
    }
};

template<typename T>
inline bool operator==(const Allocator<T> &a, const Allocator<T> &b)
{
    return a.m_pool == b.m_pool;
}
  
template<typename T>
inline bool operator!=(const Allocator<T> &a, const Allocator<T> &b)
{
    return a.m_pool != b.m_pool;
}

}

namespace std {

template<>
inline void swap< divine::Pool >( divine::Pool &a, divine::Pool &b )
{
#ifndef DISABLE_POOLS
    swap( a.m_groups, b.m_groups );
#endif
}

}

#endif
