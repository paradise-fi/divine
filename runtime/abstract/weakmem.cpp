// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015,2017 Vladimír Štill <xstill@fi.muni.cz>

#include <abstract/weakmem.h>
#include <algorithm> // reverse iterator
#include <cstdarg>
#include <dios.h>

#define forceinline __attribute__((always_inline))

#define assume( x ) do { \
        if ( !(x) ) \
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, \
                          _VM_CF_Interrupted | _VM_CF_Cancel | _VM_CF_Mask, \
                          _VM_CF_Interrupted | _VM_CF_Cancel ); \
    } while ( false )

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgcc-compat"

namespace lart {
namespace weakmem {

void startFlusher( void * ) __lart_weakmem_bypass;

namespace {

bool subseteq( const MemoryOrder a, const MemoryOrder b ) __lart_weakmem_bypass forceinline {
    return (unsigned( a ) & unsigned( b )) == unsigned( a );
}

MemoryOrder minMemOrd() forceinline { return MemoryOrder( __lart_weakmem_min_ordering ); }
bool minIsAcqRel() forceinline { return subseteq( MemoryOrder::AcqRel, minMemOrd() ); }

template< typename Collection >
struct Reversed {
    using T = typename Collection::value_type;
    using iterator = typename Collection::reverse_iterator;
    using const_iterator = typename Collection::const_reverse_iterator;

    Reversed( Collection &data ) __lart_weakmem_bypass : data( data ) { }
    Reversed( const Reversed & ) __lart_weakmem_bypass = default;

    iterator begin() __lart_weakmem_bypass { return data.rbegin(); }
    iterator end() __lart_weakmem_bypass { return data.rend(); }
    const_iterator begin() const __lart_weakmem_bypass { return data.rbegin(); }
    const_iterator end() const __lart_weakmem_bypass { return data.rend(); }

    Collection &data;
};

template< typename T >
static Reversed< T > reversed( T &x ) __lart_weakmem_bypass { return Reversed< T >( x ); }

template< typename T >
static T *alloc( int n ) __lart_weakmem_bypass {
    return static_cast< T * >( __vm_obj_make( n * sizeof( T ) ) );
}

template< typename ItIn, typename ItOut >
ItOut uninitialized_move( ItIn first, ItIn last, ItOut out ) {
    return std::uninitialized_copy( std::make_move_iterator( first ), std::make_move_iterator( last ), out );
}

struct BufferLine {

    BufferLine() = default;
    BufferLine( MemoryOrder order ) { this->order = order; } // fence
    BufferLine( char *addr, uint64_t value, uint32_t bitwidth, MemoryOrder order ) :
        addr( addr ), value( value )
    {
        this->bitwidth = bitwidth;
        this->order = order;
    }

    bool isFence() const { return !addr; }
    bool isStore() const { return addr; }

    void store() {
        if ( !flushed && isStore() ) {
            switch ( bitwidth ) {
                case 1: case 8: store< uint8_t >(); break;
                case 16: store< uint16_t >(); break;
                case 32: store< uint32_t >(); break;
                case 64: store< uint64_t >(); break;
                default: __dios_fault( _VM_F_Control, "Unhandled case" );
            }
            flushed = true;
        }
    }

    template< typename T >
    void store() const {
        *reinterpret_cast< T * >( addr ) = T( value );
    }

    void observe() {
        // observers |= uint64_t( 1 ) << __dios_get_thread_handle();
        __builtin_unreachable();
    }

    bool observed() const {
        // return ( observers & (uint64_t( 1 ) << __dios_get_thread_handle()) ) != 0;
        __builtin_unreachable();
    }

    bool matches( char *const mem, uint32_t size ) const {
        return (mem <= addr && addr < mem + size) || (addr <= mem && mem < addr + bitwidth / 8);
    }

    char *addr = nullptr;
    uint64_t value = 0;
    union {
        uint64_t _init = 0; // make sure value is recognised as defined by DIVINE
        struct {
            bool flushed:1;
            uint32_t bitwidth:7;
            MemoryOrder order:8;
            uint64_t observers:48;
        };
    };
};

template< typename T >
struct Array {

    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator< iterator >;
    using const_reverse_iterator = std::reverse_iterator< const_iterator >;

    Array() = default;
    Array( const Array & ) = delete;
    Array( Array &&o ) : data( o.data ) { o.data = nullptr; }

    ~Array() { drop(); }

    Array &operator=( const Array & ) = delete;
    Array &operator=( Array &&o ) { std::swap( o.data, data ); return *this; }

    explicit operator bool() const { return data; }
    bool empty() const { return !data; }
    int size() const { return data ? __vm_obj_size( data ) / sizeof( T ) : 0; }

    T *begin() { return data; }
    T *end() { return data + size(); }
    const T *begin() const { return data; }
    const T *end() const { return data + size(); }

    reverse_iterator rbegin() { return reverse_iterator( end() ); }
    reverse_iterator rend() { return reverse_iterator( begin() ); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator( end() ); }
    const_reverse_iterator rend() const { return const_reverse_iterator( begin() ); }

    void resize( int sz ) {
        const int olds = size();
        if ( sz == 0 )
            drop();
        else if ( sz != olds ) {
            T *ndata = alloc< T >( sz );
            if ( data )
                uninitialized_move( data, data + std::min( olds, sz ), ndata );
            for ( int i = std::min( olds, sz ); i < sz; ++i )
                new ( ndata + i ) T();
            drop();
            data = ndata;
        }
    }

    T &operator[]( int i ) { return data[ i ]; }

  protected:
    void drop() {
        for ( auto &x : *this )
            x.~T();
        __vm_obj_free( data );
        data = nullptr;
    }

    T *data = nullptr;
};

struct Buffer : Array< BufferLine > {

    int storeCount() {
        return std::count_if( begin(), end(), []( BufferLine &l ) { return l.isStore(); } );
    }

    BufferLine &newest() { return end()[ -1 ]; }
    BufferLine &oldest() { return *begin(); }

    void push( BufferLine &&l ) {
        resize( size() + 1 );
        newest() = std::move( l );
    }

    void erase( const int i ) {
        auto oldsz = size();
        if ( oldsz <= 1 )
            drop();
        else {
            BufferLine *ndata = alloc< BufferLine >( oldsz - 1 );
            uninitialized_move( data, data + i, ndata );
            uninitialized_move( data + i + 1, end(), ndata + i );
            drop();
            data = ndata;
        }
    }

    void evict( char *const from, char *const to ) {
        auto matches = [=]( BufferLine &l ) { return !(from <= l.addr && l.addr < to); };
        int cnt = std::count_if( begin(), end(), matches );
        if ( cnt == 0 )
            drop();
        else if ( cnt < size() ) {
            BufferLine *ndata = alloc< BufferLine >( cnt );
            int i = 0;
            for ( auto &l : *this )
                if ( matches( l ) )
                    new ( ndata + i++ ) BufferLine( std::move( l ) );
            drop();
            data = ndata;
        }
    }

    void flushOne() __attribute__((__always_inline__, __flatten__)) {
        int sz = size();
        assume( sz > 0 );
        // If minimap ordering ic acquire-release, there is no point in
        // reordering, as it will sync anyway
        int i = sz == 1 || minIsAcqRel() ? 0 : __vm_choose( sz );

        BufferLine &entry = (*this)[ i ];

        // check that there are no older writes to overlapping memory and
        // check if there is any release earlier
        bool releaseOrAfterRelease = subseteq( MemoryOrder::Release, entry.order );
        for ( int j = 0; j < i; ++j ) {
            auto &other = (*this)[ j ];
            // don't flush stores after any mathcing stores
            assume( !other.matches( entry.addr, entry.bitwidth / 8 ) );
            // don't ever flush SC stores before other SC stores
            // assume( entry.order != MemoryOrder::SeqCst || other.order != MemoryOrder::SeqCst );
            if ( subseteq( MemoryOrder::Release, other.order ) )
                releaseOrAfterRelease = true;
        }
        entry.store();
        if ( !releaseOrAfterRelease || entry.order == MemoryOrder::Unordered )
            erase( i );
        if ( i == 0 )
            cleanOldAndFlushed();
    }

    void cleanOldAndFlushed() __attribute__((__always_inline__, __flatten__)) {
        while ( size() > 0 && (oldest().flushed || oldest().isFence()) )
            erase( 0 );
    }
};

struct BufferHelper : Array< Buffer > {

    void flushOne( _DiOS_ThreadHandle which ) __attribute__((__noinline__, __flatten__)) __lart_weakmem_bypass {
        __dios::InterruptMask masked;
        auto *buf = getIfExists( which );
        assert( bool( buf ) );
        buf->flushOne();
    }

    Buffer *getIfExists( _DiOS_ThreadHandle ) {
        __builtin_unreachable();
    }

    Buffer &get( _DiOS_ThreadHandle i ) {
        Buffer *b = getIfExists( i );
        if ( !b ) {
            __builtin_unreachable();
            /*
            resize( i + 1 );
            b = getIfExists( i );
            // start flusher thread when store buffer for the
            // thread is first used
            */
            __dios_start_thread( &startFlusher, i, 0 );
        }
        return *b;
    }

    Buffer *getIfExists() { return getIfExists( __dios_get_thread_handle() ); }
    Buffer &get() { return get( __dios_get_thread_handle() ); }

    // mo must be other then Unordered
    template< bool strong = false >
    void waitForOther( char *const from, int size, const MemoryOrder mo ) {
        auto *local = getIfExists();
        for ( auto &sb : *this )
            if ( &sb != local )
                for ( auto &l : sb ) {
                    assume( !( // conditions for waiting
                        // wait for all SC operations, synchronize with them
                        ( mo == MemoryOrder::SeqCst && l.order == MemoryOrder::SeqCst )
                        || // strong: wait for non-SC atomic operations on matching location
                           // weak: monotonic does not require anything, it can be loaded from memory
                        ( strong && subseteq( MemoryOrder::Monotonic, mo ) && l.isStore() && !l.flushed && l.matches( from, size ) && subseteq( MemoryOrder::Monotonic, l.order ) )
                        || // strong: wait for observed fences and observed or matching release stores (including flushed ones)
                           // weak: only wait if data was flushed and therefore can be observed
                           // note: there can never be prefix of flushed data locations and fences in SB, therefore
                           //       flushed ==> there is something before which is not flushed and not fence
                        ( subseteq( MemoryOrder::Release, l.order ) && subseteq( MemoryOrder::Acquire, mo ) &&
                           ( l.observed() || ( l.matches( from, size ) && (strong || l.flushed) ) ) )
                    ) );
                }
    }

    void waitForFence( MemoryOrder mo ) {
        waitForOther( nullptr, 0, mo );
    }
    void registerLoad( char *const from, int size ) {
        for ( auto &sb : *this )
            for ( auto &l : sb )
                if ( l.flushed && l.matches( from, size ) && subseteq( MemoryOrder::Monotonic, l.order ) ) {
                    // go throught all release ops older than l and l, and observe them
                    for ( auto &f : sb ) {
                        if ( subseteq( MemoryOrder::Release, f.order ) )
                            f.observe();
                        if ( &f == &l )
                            break;
                    }
                }
    }
};

uint64_t load( char *addr, uint32_t bitwidth ) {
    switch ( bitwidth ) {
        case 1: case 8: return *reinterpret_cast< uint8_t * >( addr );
        case 16: return *reinterpret_cast< uint16_t * >( addr );
        case 32: return *reinterpret_cast< uint32_t * >( addr );
        case 64: return *reinterpret_cast< uint64_t * >( addr );
        default: __dios_fault( _VM_F_Control, "Unhandled case" );
    }
    return 0;
}

}

// avoid global ctors/dtors for BufferHelper
union BFH {
    BFH() : raw() { }
    ~BFH() { }
    void *raw;
    BufferHelper storeBuffers;
} __lart_weakmem;

void startFlusher( void *_which ) __lart_weakmem_bypass {
    _DiOS_ThreadHandle which = static_cast< _DiOS_ThreadHandle >( _which );

    while ( true )
        __lart_weakmem.storeBuffers.flushOne( which );
}
}
}

volatile int __lart_weakmem_buffer_size = 2;
volatile int __lart_weakmem_min_ordering = 0;

using namespace lart::weakmem;

void __lart_weakmem_store( char *addr, uint64_t value, uint32_t bitwidth, __lart_weakmem_order _ord ) noexcept {
    __dios::InterruptMask masked;
    if ( !addr )
        __dios_fault( _VM_F_Memory, "weakmem.store: invalid address" );
    if ( bitwidth <= 0 || bitwidth > 64 )
        __dios_fault( _VM_F_Memory, "weakmem.store: invalid bitwidth" );

    MemoryOrder ord = MemoryOrder( _ord );
    BufferLine line{ addr, value, bitwidth, ord };
    // bypass store buffer if acquire-release is minimal ordering (and
    // therefore the memory model is at least TSO) and the memory location
    // is thread-private
    if ( minIsAcqRel() && false /* __divine_is_private( addr ) */ ) {
        line.store();
        return;
    }
    auto &buf = __lart_weakmem.storeBuffers.get();
    buf.push( std::move( line ) );
    // there can be fence as oldest entry, so we need while here
    while ( buf.storeCount() > __lart_weakmem_buffer_size ) {
        buf.oldest().store();
        buf.cleanOldAndFlushed();
    }
}

void __lart_weakmem_fence( __lart_weakmem_order _ord ) noexcept {
    __dios::InterruptMask masked;
    MemoryOrder ord = MemoryOrder( _ord );
    if ( subseteq( MemoryOrder::Release, ord ) ) { // write barrier
        if ( auto *buf = __lart_weakmem.storeBuffers.getIfExists() ) { // no need for fence if there is nothing in SB
            if ( buf->storeCount() > 0 ) {
                if ( buf->newest().isFence() )
                    buf->newest().order = MemoryOrder( uint32_t( buf->newest().order ) | uint32_t( ord ) );
                else
                    buf->push( { ord } );
            }
        }
    }
    if ( subseteq( MemoryOrder::Acquire, ord ) ) // read barrier
        __lart_weakmem.storeBuffers.waitForFence( ord );
}

void __lart_weakmem_sync( char *addr, uint32_t bitwidth, __lart_weakmem_order _ord ) noexcept {
    __dios::InterruptMask masked;
    MemoryOrder ord = MemoryOrder( _ord );
    if ( subseteq( MemoryOrder::Monotonic, ord ) && (!addr || ord == MemoryOrder::SeqCst  || !false /* __divine_is_private( addr ) */) )
        __lart_weakmem.storeBuffers.waitForOther< true >( addr, bitwidth / 8, ord );
}

union I64b {
    uint64_t i64;
    char b[8];
};

uint64_t __lart_weakmem_load( char *addr, uint32_t bitwidth, __lart_weakmem_order _ord ) noexcept {
    __dios::InterruptMask masked;
    if ( !addr )
        __dios_fault( _VM_F_Memory, "weakmem.load: invalid address" );
    if ( bitwidth <= 0 || bitwidth > 64 )
        __dios_fault( _VM_F_Control, "weakmem.load: invalid bitwidth" );

    MemoryOrder ord = MemoryOrder( _ord );

    // first wait, SequentiallyConsistent loads have to synchronize with all SC stores
    if ( false /* __divine_is_private( addr ) */ && ord != MemoryOrder::SeqCst ) { // private -> not in any store buffer
        return load( addr, bitwidth );
    }
    if ( ord != MemoryOrder::Unordered ) {
        __lart_weakmem.storeBuffers.registerLoad( addr, bitwidth / 8 );
        __lart_weakmem.storeBuffers.waitForOther( addr, bitwidth / 8, ord );
    }
    // fastpath for SC loads (after synchrnonization)
    if ( false /* __divine_is_private( addr ) */ ) { // private -> not in any store buffer
        return load( addr, bitwidth );
    }

    I64b val = { .i64 = 0 };
    I64b mval = { .i64 = load( addr, bitwidth ) }; // always attempt load from memory to check for invalidated memory
    bool bmask[8] = { false };
    bool any = false;
    Buffer *buf = __lart_weakmem.storeBuffers.getIfExists();
    if ( buf ) {
        for ( const auto &it : reversed( *buf ) ) { // go from newest entry
            if ( it.flushed )
                continue;

            if ( !any && it.addr == addr && it.bitwidth >= bitwidth )
                return it.value & (uint64_t(-1) >> (64 - bitwidth));
            if ( it.matches( addr, bitwidth / 8 ) ) {
                I64b bval = { .i64 = it.value };
                const int shift = intptr_t( it.addr ) - intptr_t( addr );
                if ( shift >= 0 ) {
                    for ( unsigned i = shift, j = 0; i < bitwidth / 8 && j < it.bitwidth / 8; ++i, ++j )
                        if ( !bmask[ i ] ) {
                            val.b[ i ] = bval.b[ j ];
                            bmask[ i ] = true;
                            any = true;
                        }
                } else {
                    for ( unsigned i = 0, j = -shift; i < bitwidth / 8 && j < it.bitwidth / 8; ++i, ++j )
                        if ( !bmask[ i ] ) {
                            val.b[ i ] = bval.b[ j ];
                            bmask[ i ] = true;
                            any = true;
                        }
                }
            }
        }
    }
    if ( any ) {
        for ( unsigned i = 0; i < bitwidth / 8; ++i )
            if ( !bmask[ i ] )
                val.b[ i ] = mval.b[ i ];
        return val.i64;
    }
    return mval.i64;
}

template< int adv >
void internal_memcpy( volatile char *dst, char *src, size_t n ) forceinline {
    constexpr int deref = adv == 1 ? 0 : -1;
    static_assert( adv == 1 || adv == -1, "" );

    while ( n ) {
        // we must do copying in block of 8 if we can, otherwise pointers will
        // be lost
        if ( n >= 8 && uintptr_t( dst ) % 8 == 0 && uintptr_t( src ) % 8 == 0 ) {
            size_t an = n / 8;
            n -= an * 8;
            volatile int64_t *adst = reinterpret_cast< volatile int64_t * >( dst );
            int64_t *asrc = reinterpret_cast< int64_t * >( src );
            for ( ; an; --an, asrc += adv, adst += adv )
                adst[ deref ] = asrc[ deref ];
            dst = reinterpret_cast< volatile char * >( adst );
            src = reinterpret_cast< char * >( asrc );
        } else {
            dst[ deref ] = src[ deref ];
            --n; dst += adv; src += adv;
        }
    }
}

void __lart_weakmem_memmove( char *_dst, const char *_src, size_t n ) noexcept {
    if ( !_dst )
        __dios_fault( _VM_F_Memory, "invalid dst in memmove" );
    if ( !_src )
        __dios_fault( _VM_F_Memory, "invalid src in memmove" );

    volatile char *dst = const_cast< volatile char * >( reinterpret_cast< char * >( _dst ) );
    char *src = reinterpret_cast< char * >( const_cast< char * >( _src ) );
    if ( dst < src )
        internal_memcpy< 1 >( dst, src, n );
    else if ( dst > src )
        internal_memcpy< -1 >( dst + n, src + n, n );
}

void __lart_weakmem_memcpy( char *_dst, const char *_src, size_t n ) noexcept {
    if ( !_dst )
        __dios_fault( _VM_F_Memory, "invalid dst in memcpy" );
    if ( !_src )
        __dios_fault( _VM_F_Memory, "invalid src in memcpy" );

    assert( _src < _dst ? _src + n < _dst : _dst + n < _src );

    volatile char *dst = const_cast< volatile char * >( _dst );
    char *src = const_cast< char * >( _src );
    internal_memcpy< 1 >( dst, src, n );
}

void __lart_weakmem_memset( char *_dst, int c, size_t n ) noexcept {
    if ( !_dst )
        __dios_fault( _VM_F_Memory, "invalid dst in memset" );

    volatile char *dst = const_cast< volatile char * >( _dst );
    for ( ; n; --n, ++dst )
        *dst = c;
}

void __lart_weakmem_cleanup( int cnt, ... ) noexcept {
    __dios::InterruptMask masked;
    if ( cnt <= 0 )
        __dios_fault( _VM_F_Control, "invalid cleanup count" );

    va_list ptrs;
    va_start( ptrs, cnt );

    Buffer *buf = __lart_weakmem.storeBuffers.getIfExists();
    if ( !buf )
        return;

    for ( int i = 0; i < cnt; ++i ) {
        char *ptr = va_arg( ptrs, char * );
        if ( !ptr )
            continue;
        buf->evict( ptr, ptr + __vm_obj_size( ptr ) );
    }
    va_end( ptrs );
}

#pragma GCC diagnostic pop
