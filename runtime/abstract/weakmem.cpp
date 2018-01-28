// -*- C++ -*- (c) 2015,2017 Vladimír Štill <xstill@fi.muni.cz>

#include <abstract/weakmem.h>
#include <algorithm> // reverse iterator
#include <cstdarg>
#include <dios.h>
#include <dios/lib/map.hpp>
#include <sys/interrupt.h>
#include <sys/lart.h>
#include <sys/vmutil.h>
#include <dios/kernel.hpp> // get_debug
#include <abstract/common.h> // weaken
#include <optional>

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

namespace lart::weakmem {

struct InterruptMask {
    InterruptMask() {
        restore = uint64_t( __vm_control( _VM_CA_Get, _VM_CR_Flags,
                                          _VM_CA_Bit, _VM_CR_Flags, wmFlags, wmFlags )
                          );
    }

    ~InterruptMask() {
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, restoreFlags, restore );
    }

    bool bypass() const {
        return restore & (_VM_CF_DebugMode | _LART_CF_RelaxedMemRuntime);
    }

    bool kernel() const {
        return restore & _VM_CF_KernelMode;
    }

  private:
    uint64_t restore;
    static const uint64_t restoreFlags = _VM_CF_Mask | _VM_CF_Interrupted
                                       | _VM_CF_KernelMode
                                       | _LART_CF_RelaxedMemRuntime;
    static const uint64_t wmFlags = _VM_CF_Mask | _LART_CF_RelaxedMemRuntime;
};

template< typename T >
using ThreadMap = __dios::ArrayMap< _DiOS_TaskHandle, T >;

template< typename T >
using Array = __dios::Array< T >;

namespace {

template< typename It >
struct Range {
    using T = typename It::value_type;
    using iterator = It;
    using const_iterator = It;

    Range( It begin, It end ) : _begin( begin ), _end( end ) { }
    Range( const Range & ) = default;

    iterator begin() const { return _begin; }
    iterator end() const { return _end; }

  private:
    It _begin, _end;
};

template< typename It >
static Range< It > range( It begin, It end ) { return Range< It >( begin, end ); }

template< typename T >
static auto reversed( T &x ) { return range( x.rbegin(), x.rend() ); }

template< typename T >
static T *alloc( int n ) {
    return static_cast< T * >( __vm_obj_make( n * sizeof( T ) ) );
}

template< typename ItIn, typename ItOut >
ItOut uninitialized_move( ItIn first, ItIn last, ItOut out ) {
    return std::uninitialized_copy( std::make_move_iterator( first ), std::make_move_iterator( last ), out );
}

static const char *ordstr( MemoryOrder mo ) {
    if ( subseteq( MemoryOrder::SeqCst, mo ) )
        return "SC";
    if ( subseteq( MemoryOrder::AcqRel, mo ) )
        return "AR";
    if ( subseteq( MemoryOrder::Acquire, mo ) )
        return "Acq";
    if ( subseteq( MemoryOrder::Release, mo ) )
        return "Rel";
    if ( subseteq( MemoryOrder::Monotonic, mo ) )
        return "Mon";
    if ( subseteq( MemoryOrder::Unordered, mo ) )
        return "U";
    return "N";
}

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

struct BufferLine : brick::types::Ord {

    BufferLine() = default;
    BufferLine( MemoryOrder order ) : order( order ) { } // fence
    BufferLine( char *addr, uint64_t value, uint32_t bitwidth, MemoryOrder order ) :
        addr( addr ), value( value ), bitwidth( bitwidth ), order( order )
    {
        assert( addr != abstract::weaken( addr ) );
    }

    bool isFence() const { return !addr; }
    bool isStore() const { return addr; }

    void store() {
        switch ( bitwidth ) {
            case 1: case 8: store< uint8_t >(); break;
            case 16: store< uint16_t >(); break;
            case 32: store< uint32_t >(); break;
            case 64: store< uint64_t >(); break;
            case 0: break; // fence
            default: __dios_fault( _VM_F_Control, "Unhandled case" );
        }
    }

    template< typename T >
    void store() {
        *reinterpret_cast< T * >( addr ) = T( value );
    }

    bool matches( const char *const from, const char *const to ) const {
        return (from <= addr && addr < to) || (addr <= from && from < addr + bitwidth / 8);
    }

    bool matches( const char *const mem, const uint32_t size ) const {
        return matches( mem, mem + size );
    }

    bool matches( BufferLine &o ) {
        return matches( o.addr, o.bitwidth / 8 );
    }

    enum class Status : int8_t {
        Keep, MaybeFlush, MustFlush
    };

    char *addr = nullptr;
    uint64_t value = 0;

    uint8_t bitwidth = 0;
    MemoryOrder order = MemoryOrder::NotAtomic;
    int16_t sc_seq = 0; // note: we can go negative in the flushing process
    int16_t at_seq = 0; // same here
    Status status = Status::Keep;

    auto as_tuple() const {
        // note: ignore status in comparison, take into account only part
        // relevant to memory behaviour
        return std::tie( addr, value, bitwidth, order, sc_seq, at_seq );
    }

    void dump() const {
        char buffer[] = "[0xdeadbeafdeadbeaf ← 0xdeadbeafdeadbeaf; 00 bit; WA SC;000;000]";
        snprintf( buffer, sizeof( buffer ) - 1, "[0x%llx ← 0x%llx; %d bit; %c%c%s;%d;%d]",
                  uint64_t( addr ), value, bitwidth,
                  subseteq( MemoryOrder::WeakCAS, order ) ? 'W' : ' ',
                  subseteq( MemoryOrder::AtomicOp, order ) ? 'A' : ' ',
                  ordstr( order ), at_seq, sc_seq );
        __vm_trace( _VM_T_Text, buffer );
    }
};

using Status = BufferLine::Status;

static_assert( sizeof( BufferLine ) == 3 * sizeof( uint64_t ) );

struct Buffer : Array< BufferLine > {

    struct ID {
        ID() = default;
        ID( Buffer &b, Buffer::iterator it ) : _b( &b ), _it( it ) { }

        auto &operator*() { return *_it; }
        auto *operator->() { return &*_it; }

        explicit operator bool() const { return _b; }
        operator Buffer::iterator() { return _it; }

        // NOTE: these operators represent partial order, so it can happen that
        // !(a < b) && !(b < a) && !(a == b)
        bool operator<( const ID &o ) const {
            if ( _b == o._b )
                return _it < o._it;
            if ( _it->sc_seq == 0 && o._it->sc_seq == 0
                    && (_it->addr != o._it->addr || (_it->at_seq == 0 && o._it->at_seq == 0)) )
                return false; // not explicitly ordered
            return _it->sc_seq < o._it->sc_seq
                || (_it->addr == o._it->addr && _it->at_seq < o._it->at_seq);
        }

        bool operator==( const ID &o ) const {
            return _b == o._b && _it == o._it;
        }

        bool operator>( const ID &o ) const {
            return o < *this;
        }

        Buffer &buffer() { return *_b; }

        Buffer *_b{};
        Buffer::iterator _it{};
    };

    Buffer() = default;

    int storeCount() {
        return std::count_if( begin(), end(), []( BufferLine &l ) { return l.isStore(); } );
    }

    BufferLine &newest() { return end()[ -1 ]; }
    BufferLine &oldest() { return *begin(); }

    void _push( BufferLine &&l ) {
        push_back( std::move( l ) );

        // there can be fence as oldest entry, so we need while here
        while ( storeCount() > __lart_weakmem_buffer_size() ) {
            oldest().store();
            erase( 0 );
            cleanOldAndFlushed();
        }
    }

    void erase( const int i ) {
        erase( begin() + i );
    }

    void erase( iterator i ) {
        std::move( i + 1, end(), i );
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

    ID find_ws_pre( iterator it ) {
        for ( reverse_iterator rb{ it }, end = rend(); rb != end; ++rb ) {
            if ( it->matches( *rb ) )
                return { *this, std::prev( rb.base() ) };
        }
        return { };
    }

    void _flush_one( __dios::Array< std::pair< Buffer *, Buffer::iterator > > &todo ) __attribute__((__always_inline__, __flatten__))
    {
        bool after_release = false;
        for ( auto entry = begin(), e = end(); !after_release && entry != e; ++entry )
        {
            // release stores, and anything after them, can be flushed only if
            // they are first
            after_release = subseteq( MemoryOrder::Release, entry->order );
            if ( after_release && entry != begin() )
                continue;

            if ( entry->sc_seq > 1 || entry->at_seq > 1 )
                continue;

            // check that there are no older writes to overlapping memory
            for ( auto it = begin(); it != entry; ++it ) {
                // don't flush stores after any mathcing stores
                if ( it->matches( entry->addr, entry->bitwidth / 8 ) )
                    goto next;
            }

            todo.emplace_back( this, entry );
          next:;
        }
    }

    void cleanOldAndFlushed() __attribute__((__always_inline__, __flatten__)) {
        while ( size() > 0 && oldest().isFence() )
            erase( 0 );
    }

    void dump() const {
        for ( auto &e : *this )
            e.dump();
    }
};

struct Buffers : ThreadMap< Buffer > {

    using Super = ThreadMap< Buffer >;

    bool flush_one() __attribute__((__noinline__, __flatten__)) {
        __dios::InterruptMask masked;
        if ( this->empty() )
            return false; // we have done everyhging we could

        __dios::Array< std::pair< Buffer *, Buffer::iterator > > todo;

        for ( auto &p : *this )
            p.second._flush_one( todo );

        if ( todo.empty() ) {
            return false; // buffer is deadlocked
        }

        auto c = __vm_choose( todo.size() );

        assert( c < todo.size() );
        auto *entry = todo[c].second;
        auto *buf = todo[c].first;

        entry->store();
        if ( entry->at_seq )
            fix_at_seq( *entry );
        if ( entry->sc_seq )
            fix_sc_seq( *entry );
        buf->erase( entry );
        if ( entry == buf->begin() )
            buf->cleanOldAndFlushed();
        return true;
    }

    Buffer *getIfExists( _DiOS_TaskHandle h ) {
        auto it = find( h );
        return it != end() ? &it->second : nullptr;
    }

    Buffer &get( _DiOS_TaskHandle tid ) {
        Buffer *b = getIfExists( tid );
        if ( !b ) {
            b = &emplace( tid, Buffer{} ).first->second;
        }
        return *b;
    }

    Buffer *getIfExists() { return getIfExists( __dios_get_task_handle() ); }
    Buffer &get() { return get( __dios_get_task_handle() ); }

    template< typename T, T v >
    struct Const {
        template< typename... Args >
        T operator()( Args &&... ) const { return v; }
    };

    template< typename Self, typename Yield, typename Filter >
    static void _for_each( Self &self, Yield yield, Filter filt )
    {
        for ( auto &p : self ) {
            if ( filt( p.first ) )
                for ( auto &l : p.second )
                    yield( p.first, p.second, l );
        }
    }

    template< typename Yield, typename Filter = Const< bool, true > >
    void for_each( Yield y, Filter filt = Filter() ) { _for_each( *this, y, filt ); }

    template< typename Yield, typename Filter = Const< bool, true > >
    void for_each( Yield y, Filter filt = Filter() ) const { _for_each( *this, y, filt ); }

    using Super::find;

    template< typename Pred >
    Buffer::ID find( Pred pred )
    {
        for ( auto &p : *this ) {
            for ( auto it = p.second.begin(), end = p.second.end(); it != end; ++it ) {
                if ( pred( *it ) ) {
                    return Buffer::ID{ p.second, it };
                }
            }
        }
        return Buffer::ID{ };
    }

    template< typename Filter = Const< bool, true > >
    int16_t get_next_sc_seq( Filter filt = Filter() ) const
    {
        int16_t max = 0;
        for_each( [&]( auto &, auto &, auto &l ) { max = std::max( max, l.sc_seq ); }, filt );
        return max + 1;
    }

    template< typename Filter = Const< bool, true > >
    int16_t get_next_at_seq( char *addr, int bitwidth, Filter filt = Filter() ) const
    {
        int16_t max = 0;
        for_each( [&]( auto &, auto &, auto &l ) {
                if ( l.addr == addr && l.bitwidth == bitwidth ) {
                    if ( l.at_seq > max ) {
                        max = l.at_seq;
                    }
                } else if ( l.matches( addr, bitwidth / 8 ) ) {
                    __dios_fault( _VM_F_Memory, "Atomic operations over overlapping, "
                            "but not same memory locations are not allowed" );
                }
            }, filt );
        return max + 1;
    }

    void fix_at_seq( BufferLine &entry )
    {
        for_each( [&]( auto &, auto &, auto &l ) {
                if ( l.addr == entry.addr && l.bitwidth == entry.bitwidth && l.at_seq ) {
                    l.at_seq -= entry.at_seq;
                }
            } );
    }

    void fix_sc_seq( BufferLine &entry )
    {
        for_each( [&]( auto &, auto &, auto &l ) {
                if ( l.sc_seq ) {
                    l.sc_seq -= entry.sc_seq;
                }
            } );
    }

    void push( Buffer &b, BufferLine &&line )
    {
        if ( subseteq( MemoryOrder::AtomicOp, line.order ) )
            line.at_seq = get_next_at_seq( line.addr, line.bitwidth );
        if ( subseteq( MemoryOrder::SeqCst, line.order ) )
            line.sc_seq = get_next_sc_seq();
        b._push( std::move( line ) );
    }

    Buffer::ID find_sc_pre( BufferLine &line ) {
        if ( line.sc_seq > 1 )
            return find( [seq = line.sc_seq - 1]( auto &l ) { return l.sc_seq == seq; } );
        return { };
    }

    Buffer::ID find_at_pre( BufferLine &line ) {
        if ( line.at_seq > 1 )
            return find( [&, seq = line.at_seq - 1]( auto &l ) {
                    return l.addr == line.addr && l.at_seq == seq;
                } );
        return { };
    }

    void dump() {
        auto *hids = __dios::have_debug() ? &__dios::get_debug().hids : nullptr;
        for ( auto &p : *this ) {
            if ( !p.second.empty() ) {
                auto tid = abstract::weaken( p.first );
                int nice_id = [&] { if ( !hids ) return -1;
                                    auto it = hids->find( tid );
                                    return it == hids->end() ? -1 : it->second;
                                  }();

                char buffer[] = "thread 0xdeadbeafdeadbeaf*: ";
                snprintf( buffer, sizeof( buffer ) - 1, "thread: %d%s ",
                                  nice_id,
                                  p.first == __dios_get_task_handle() ? "*:" : ": " );
                __vm_trace( _VM_T_Text, buffer );
                p.second.dump();
            }
        }
    }

    void read_barrier( MemoryOrder mo, _DiOS_TaskHandle tid ) noexcept {
        read_barrier( nullptr, 0, mo, tid );
    }

    void read_barrier( char *addr, int bitwidth, MemoryOrder mo, _DiOS_TaskHandle tid ) noexcept
    {
        if ( size() == 1 && begin()->first == tid )
            return; // only our thread has something in the buffer
        const bool seq_cst = subseteq( MemoryOrder::SeqCst, mo );
        const bool at = subseteq( MemoryOrder::AtomicOp, mo );
        bool dirty = false;
        for_each( [&]( auto &btid, auto &, auto &l ) {
                if ( seq_cst && l.sc_seq ) {
                    // wait for SC atomics from other buffers
                    l.status = tid == btid ? Status::MaybeFlush : Status::MustFlush;
                    dirty = true;
                }
                if ( at && l.at_seq && (!addr || l.matches( addr, bitwidth / 8 )) ) {
                    // atomic ops from other buffers
                    l.status = tid == btid ? Status::MaybeFlush : Status::MustFlush;
                    dirty = true;
                }
                if ( l.matches( addr, bitwidth / 8 ) )
                    dirty = true;
            } );

        if ( dirty )
            flush_dirty( tid );
    }

    void flush_dirty( _DiOS_TaskHandle tid ) {
        for_each( [&]( auto &btid, auto &, auto &l ) {
                if ( l.status == Status::Keep && (!tid || btid != tid) )
                    l.status = Status::MaybeFlush;
            } );

        // flush
        bool proceed = true;
        do {
            if ( !flush_one() ) {
                break;
            }
            auto st = flush_status();
            if ( st == FlushStatus::AllDone )
                proceed = false;
            else if ( st == FlushStatus::CanProceed )
                proceed = __vm_choose( 2 );
        } while ( proceed );

        for_each( []( auto &, auto &, auto &l ) {
            assert( l.status != Status::MustFlush );
            l.status = Status::Keep;
        } );
    }

    enum class FlushStatus {
        ContinueFlusing, CanProceed, AllDone
    };

    FlushStatus flush_status() const noexcept {
        FlushStatus st = FlushStatus::AllDone;
        for_each( [&st]( auto &, auto &, auto &l ) {
                if ( l.status == Status::MustFlush )
                    st = FlushStatus::ContinueFlusing;
                else if ( st == FlushStatus::AllDone && l.status == Status::MaybeFlush )
                    st = FlushStatus::CanProceed;
            } );
        return st;
    }

    _DiOS_TaskHandle flusher = nullptr;
};

}
}

// avoid global ctors/dtors for Buffers
union BFH {
    BFH() : raw() { }
    ~BFH() { }
    void *raw;
    lart::weakmem::Buffers storeBuffers;
} __lart_weakmem;

/* weak here is to prevent optimizer from eliminating calls to these functions
 * -- they will be replaced by weakmem transformation */
__attribute__((__weak__)) int __lart_weakmem_buffer_size() { return 2; }
__attribute__((__weak__)) int __lart_weakmem_min_ordering() { return 0; }

void __lart_weakmem_dump() noexcept
    __attribute__((__annotate__("divine.debugfn"),__noinline__,__weak__))
{
    __lart_weakmem.storeBuffers.dump();
}

extern "C" void __lart_weakmem_debug_fence() noexcept __attribute__((__noinline__,__weak__))
{
    // This fence is called only at the beginning of *debugfn* functions,
    // it does not need to keep store buffers consistent, debug functions will
    // not see them, it just needs to make all entries of this thread visible.
    // Also, it is uninterruptible by being part of *debugfn*
    auto *tid = __dios_get_task_handle();
    if ( !tid )
        return;
    auto *buf = __lart_weakmem.storeBuffers.getIfExists( tid );
    if ( !buf )
        return;

    for ( auto &l : *buf )
        l.store();
    buf->clear();
}

using namespace lart::weakmem;

void __lart_weakmem_store( char *addr, uint64_t value, uint32_t bitwidth,
                           __lart_weakmem_order _ord, InterruptMask &masked )
                           noexcept __attribute__((__always_inline__))
{
    if ( !addr )
        __dios_fault( _VM_F_Memory, "weakmem.store: invalid address" );
    if ( bitwidth <= 0 || bitwidth > 64 )
        __dios_fault( _VM_F_Memory, "weakmem.store: invalid bitwidth" );

    MemoryOrder ord = MemoryOrder( _ord );
    BufferLine line{ addr, value, bitwidth, ord };
    // bypass store buffer if acquire-release is minimal ordering (and
    // therefore the memory model is at least TSO) and the memory location
    // is thread-private
    if ( masked.bypass() ) {
        line.store();
        return;
    }
    auto tid = __dios_get_task_handle();
    if ( !tid || masked.kernel() ) { // scheduler
        line.store();
        return;
    }
    auto &buf = __lart_weakmem.storeBuffers.get( tid );
    __lart_weakmem.storeBuffers.push( buf, std::move( line ) );
}

void __lart_weakmem_store( char *addr, uint64_t value, uint32_t bitwidth,
                           __lart_weakmem_order _ord ) noexcept
{
    InterruptMask masked;
    __lart_weakmem_store( addr, value, bitwidth, _ord, masked );
}

void __lart_weakmem_fence( __lart_weakmem_order _ord ) noexcept {
    InterruptMask masked;
    if ( masked.bypass() )
        return; // should not be called recursivelly

    MemoryOrder ord = MemoryOrder( _ord );
    auto tid = __dios_get_task_handle();
    auto *buf = __lart_weakmem.storeBuffers.getIfExists( tid );
    if ( !buf )
        return;

    // TODO: barrier too strong and causes brokend ordering of atomic operations
    if ( subseteq( MemoryOrder::SeqCst, ord ) ) {
        for ( auto &l : *buf )
            l.store();
        buf->clear();
        return;
    }
    if ( subseteq( MemoryOrder::Release, ord ) ) { // write barrier
        __lart_weakmem.storeBuffers.push( *buf, BufferLine{ MemoryOrder::Release } );
    }
    if ( subseteq( MemoryOrder::Acquire, ord ) ) // read barrier
        __lart_weakmem.storeBuffers.read_barrier( ord, tid );
}

union I64b {
    uint64_t i64;
    char b[8];
};

static uint64_t doLoad( Buffer *buf, char *addr, uint32_t bitwidth )
{
    I64b val = { .i64 = 0 };
    I64b mval = { .i64 = load( addr, bitwidth ) }; // always attempt load from memory to check for invalidated memory
    bool bmask[8] = { false };
    bool any = false;
    if ( buf ) {

        for ( const auto &it : reversed( *buf ) ) { // go from newest entry

            if ( !any && it.addr == addr && it.bitwidth >= bitwidth ) {

                return it.value & (uint64_t(-1) >> (64 - bitwidth));
            }
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

uint64_t __lart_weakmem_load( char *addr, uint32_t bitwidth, __lart_weakmem_order _ord,
                              InterruptMask &masked )
                              noexcept __attribute__((__always_inline__))
{
    if ( !addr )
        __dios_fault( _VM_F_Memory, "weakmem.load: invalid address" );
    if ( bitwidth <= 0 || bitwidth > 64 )
        __dios_fault( _VM_F_Control, "weakmem.load: invalid bitwidth" );

    MemoryOrder ord = MemoryOrder( _ord );

    // first wait, SequentiallyConsistent loads have to synchronize with all SC stores
    if ( masked.bypass() )
    { // private -> not in any store buffer
        return load( addr, bitwidth );
    }

    auto *tid = __dios_get_task_handle();
    if ( !tid || masked.kernel() ) { // scheduler
        return load( addr, bitwidth );
    }
    Buffer *buf = __lart_weakmem.storeBuffers.getIfExists( tid );

    __lart_weakmem.storeBuffers.read_barrier( addr, bitwidth, ord, tid );

    return doLoad( buf, addr, bitwidth );
}

uint64_t __lart_weakmem_load( char *addr, uint32_t bitwidth, __lart_weakmem_order _ord ) noexcept {
    InterruptMask masked;
    return __lart_weakmem_load( addr, bitwidth, _ord, masked );
}

CasRes __lart_weakmem_cas( char *addr, uint64_t expected, uint64_t value, uint32_t bitwidth,
                           __lart_weakmem_order _ordSucc, __lart_weakmem_order _ordFail ) noexcept
{
    InterruptMask masked;

    auto loaded = __lart_weakmem_load( addr, bitwidth, _ordFail, masked );

    if ( loaded != expected
            || ( subseteq( MemoryOrder::WeakCAS, MemoryOrder( _ordFail ) ) && __vm_choose( 2 ) ) )
        return { loaded, false };

    // TODO: when implementing NSW, make sure we order properly with _ordSucc

    __lart_weakmem_store( addr, value, bitwidth, _ordSucc );
    return { value, true };
}

void __lart_weakmem_cleanup( int32_t cnt, ... ) noexcept {
    InterruptMask masked;
    if ( masked.bypass() )
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
    if ( masked.bypass() )
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
