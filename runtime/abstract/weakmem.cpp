// -*- C++ -*- (c) 2015,2017 Vladimír Štill <xstill@fi.muni.cz>

#include <abstract/weakmem.h>
#include <algorithm> // reverse iterator
#include <cstdarg>
#include <dios.h>
#include <dios/lib/map.hpp>
#include <sys/lart.h>

#define forceinline __attribute__((always_inline))

#define assume( x ) do { \
        if ( !(x) ) \
            __vm_control( _VM_CA_Bit, _VM_CR_Flags, \
                          _VM_CF_Interrupted | _VM_CF_Cancel | _VM_CF_Mask, \
                          _VM_CF_Interrupted | _VM_CF_Cancel ); \
    } while ( false )

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgcc-compat"

static char *baseptr( char *ptr ) {
    uintptr_t base = uintptr_t( ptr ) & ~_VM_PM_Off;
    uintptr_t type = (base & _VM_PM_Type) >> _VM_PB_Off;
    if ( type == _VM_PT_Marked || type == _VM_PT_Weak )
        base = (base & ~_VM_PM_Type) | (_VM_PT_Heap << _VM_PB_Off);
    return reinterpret_cast< char * >( base );
}

namespace lart {
namespace weakmem {

struct InterruptMask {
    InterruptMask() {
        restore = uint64_t( __vm_control( _VM_CA_Get, _VM_CR_Flags,
                                          _VM_CA_Bit, _VM_CR_Flags, setFlags, setFlags )
                          ) & allFlags;
    }

    ~InterruptMask() {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, allFlags, restore );
    }

    bool kernelOrWM() const { return restore & (_VM_CF_KernelMode | _LART_CF_RelaxedMemRuntime); }

  private:
    uint64_t restore;
    static const uint64_t allFlags = _VM_CF_Mask | _VM_CF_Interrupted | _VM_CF_KernelMode | _LART_CF_RelaxedMemRuntime;
    static const uint64_t setFlags = _VM_CF_Mask | _LART_CF_RelaxedMemRuntime;
};

template< typename T >
using ThreadMap = __dios::ArrayMap< _DiOS_ThreadHandle, T >;

template< typename T >
using Array = __dios::Array< T >;

namespace {

bool subseteq( const MemoryOrder a, const MemoryOrder b ) forceinline {
    return (unsigned( a ) & unsigned( b )) == unsigned( a );
}

MemoryOrder minMemOrd() forceinline { return MemoryOrder( __lart_weakmem_min_ordering ); }
bool minIsAcqRel() forceinline { return subseteq( MemoryOrder::AcqRel, minMemOrd() ); }

template< typename Collection >
struct Reversed {
    using T = typename Collection::value_type;
    using iterator = std::reverse_iterator< typename Collection::iterator >;
    using const_iterator = std::reverse_iterator< typename Collection::const_iterator >;

    Reversed( Collection &data ) : data( data ) { }
    Reversed( const Reversed & ) = default;

    iterator begin() { return iterator( data.end() ); }
    iterator end() { return iterator( data.begin() ); }
    const_iterator begin() const { return const_iterator( data.end() ); }
    const_iterator end() const { return const_iterator( data.begin() ); }

    Collection &data;
};

template< typename T >
static Reversed< T > reversed( T &x ) { return Reversed< T >( x ); }

template< typename T >
static T *alloc( int n ) {
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
        observers = 1; // TODO
        // observers |= uint64_t( 1 ) << __dios_get_thread_handle();
    }

    bool observed() const {
        return observers; // TODO
        // return ( observers & (uint64_t( 1 ) << __dios_get_thread_handle()) ) != 0;
    }

    bool matches( const char *const from, const char *const to ) const {
        return (from <= addr && addr < to) || (addr <= from && from < addr + bitwidth / 8);
    }

    bool matches( const char *const mem, const uint32_t size ) const {
        return matches( mem, mem + size );
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

struct Buffer : Array< BufferLine > {

    int storeCount() {
        return std::count_if( begin(), end(), []( BufferLine &l ) { return l.isStore(); } );
    }

    BufferLine &newest() { return end()[ -1 ]; }
    BufferLine &oldest() { return *begin(); }

    void push( BufferLine &&l ) {
        push_back( std::move( l ) );
    }

    void erase( const int i ) {
        std::move( begin() + i + 1, end(), begin() + i );
        pop_back();
    }

    void evict( char *const ptr ) {
        const auto id = baseptr( ptr );
        evict( [id]( BufferLine &l ) { return baseptr( l.addr ) != id; } );
    }

    void evict( char *from, char *to ) {
        evict( [from, to]( BufferLine &l ) {
                return !( l.addr >= from && l.addr + l.bitwidth / 8 <= to );
            } );
    }

    template< typename Filter >
    void evict( Filter filter ) {
        int cnt = std::count_if( begin(), end(), filter );
        if ( cnt == 0 )
            clear();
        else if ( cnt < size() )
        {
            auto dst = begin();
            for ( auto &l : *this ) {
                if ( filter( l ) ) {
                    if ( dst != &l )
                        *dst = std::move( l );
                    ++dst;
                }
            }
        }
        _resize( cnt );
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

struct Buffers : ThreadMap< Buffer > {

    void flushOne( _DiOS_ThreadHandle which ) __attribute__((__noinline__, __flatten__)) {
        InterruptMask masked;
        auto *buf = getIfExists( which );
        assert( bool( buf ) );
        buf->flushOne();
    }

    Buffer *getIfExists( _DiOS_ThreadHandle h ) {
        auto it = find( h );
        return it != end() ? &it->second : nullptr;
    }

    Buffer &get( _DiOS_ThreadHandle tid ) {
        Buffer *b = getIfExists( tid );
        if ( !b ) {
            b = &emplace( tid, Buffer{} ).first->second;
            // start flusher thread when store buffer for the thread is first used
            __dios_start_thread( &__lart_weakmem_flusher_main, tid, 0 );
        }
        return *b;
    }

    Buffer *getIfExists() { return getIfExists( __dios_get_thread_handle() ); }
    Buffer &get() { return get( __dios_get_thread_handle() ); }

    // mo must be other then Unordered
    template< bool strong = false >
    void waitForOther( char *const from, int size, const MemoryOrder mo ) {
        auto tid = __dios_get_thread_handle();
        for ( auto &sb : *this )
            if ( sb.first != tid )
                for ( auto &l : sb.second ) {
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
            for ( auto &l : sb.second )
                if ( l.flushed && l.matches( from, size ) && subseteq( MemoryOrder::Monotonic, l.order ) ) {
                    // go throught all release ops older than l and l, and observe them
                    for ( auto &f : sb.second ) {
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

// avoid global ctors/dtors for Buffers
union BFH {
    BFH() : raw() { }
    ~BFH() { }
    void *raw;
    Buffers storeBuffers;
} __lart_weakmem;

}
}

static bool direct( void * ) { return false; }

volatile int __lart_weakmem_buffer_size = 2;
volatile int __lart_weakmem_min_ordering = 0;

using namespace lart::weakmem;

void __lart_weakmem_flusher_main( void *_which ) {
    _DiOS_ThreadHandle which = static_cast< _DiOS_ThreadHandle >( _which );

    while ( true )
        __lart_weakmem.storeBuffers.flushOne( which );
}

void __lart_weakmem_store( char *addr, uint64_t value, uint32_t bitwidth, __lart_weakmem_order _ord ) noexcept {
    InterruptMask masked;
    if ( !addr )
        __dios_fault( _VM_F_Memory, "weakmem.store: invalid address" );
    if ( bitwidth <= 0 || bitwidth > 64 )
        __dios_fault( _VM_F_Memory, "weakmem.store: invalid bitwidth" );

    MemoryOrder ord = MemoryOrder( _ord );
    BufferLine line{ addr, value, bitwidth, ord };
    // bypass store buffer if acquire-release is minimal ordering (and
    // therefore the memory model is at least TSO) and the memory location
    // is thread-private
    if ( masked.kernelOrWM() || ( minIsAcqRel() && direct( addr ) ) ) {
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
    InterruptMask masked;
    if ( masked.kernelOrWM() )
        return; // should not be called recursivelly
    MemoryOrder ord = MemoryOrder( _ord );
    if ( subseteq( MemoryOrder::Release, ord ) ) { // write barrier
        auto tid = __dios_get_thread_handle();
        auto bufit = __lart_weakmem.storeBuffers.find( tid );
        if ( bufit != __lart_weakmem.storeBuffers.end() ) { // no need for fence if there is nothing in SB
            if ( bufit->second.storeCount() > 0 ) {
                for ( auto &e : bufit->second )
                    e.store();
                // __lart_weakmem.storeBuffers.erase( bufit ); // TODO
                bufit->second.clear();
            }
        }
    }
    if ( subseteq( MemoryOrder::Acquire, ord ) ) // read barrier
        ; // TODO: none for PSO, needed for NSW and RMO
}

void __lart_weakmem_sync( char *addr, uint32_t bitwidth, __lart_weakmem_order _ord ) noexcept {
    InterruptMask masked;
    if ( masked.kernelOrWM() )
        return; // should not be called recursivelly
    MemoryOrder ord = MemoryOrder( _ord );
    if ( subseteq( MemoryOrder::Monotonic, ord ) && (!addr || ord == MemoryOrder::SeqCst  || !false /* __divine_is_private( addr ) */) )
        __lart_weakmem.storeBuffers.waitForOther< true >( addr, bitwidth / 8, ord );
}

union I64b {
    uint64_t i64;
    char b[8];
};

uint64_t __lart_weakmem_load( char *addr, uint32_t bitwidth, __lart_weakmem_order _ord ) noexcept {
    InterruptMask masked;
    if ( !addr )
        __dios_fault( _VM_F_Memory, "weakmem.load: invalid address" );
    if ( bitwidth <= 0 || bitwidth > 64 )
        __dios_fault( _VM_F_Control, "weakmem.load: invalid bitwidth" );

    MemoryOrder ord = MemoryOrder( _ord );

    // first wait, SequentiallyConsistent loads have to synchronize with all SC stores
    if ( masked.kernelOrWM() || ( direct( addr ) && ord != MemoryOrder::SeqCst ) ) { // private -> not in any store buffer
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

void __lart_weakmem_cleanup( int32_t cnt, ... ) noexcept {
    InterruptMask masked;
    if ( masked.kernelOrWM() )
        return; // should not be called recursivelly
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
        buf->evict( ptr );
    }
    va_end( ptrs );
}

void __lart_weakmem_resize( char *ptr, uint32_t newsize ) noexcept {
    InterruptMask masked;
    if ( masked.kernelOrWM() )
        return; // should not be called recursivelly
    uint32_t orig = __vm_obj_size( ptr );
    if ( orig <= newsize )
        return;

    Buffer *buf = __lart_weakmem.storeBuffers.getIfExists();
    if ( !buf )
        return;
    auto base = baseptr( ptr );
    buf->evict( base + newsize, base + orig );
}

#pragma GCC diagnostic pop
