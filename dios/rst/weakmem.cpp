// -*- C++ -*- (c) 2015-2019 Vladimír Štill <xstill@fi.muni.cz>

#include <algorithm> // reverse iterator
#include <cstdarg>
#include <dios.h>
#include <util/map.hpp>
#include <sys/interrupt.h>
#include <sys/lart.h>
#include <sys/vmutil.h>
#include <sys/cdefs.h>
#include <dios/sys/kernel.hpp> // get_debug

#define _WM_INLINE __attribute__((__always_inline__, __flatten__))
#define _WM_NOINLINE __attribute__((__noinline__))
#define _WM_NOINLINE_WEAK __attribute__((__noinline__, __weak__))
#define _WM_INTERFACE __attribute__((__nothrow__, __noinline__, __flatten__)) __invisible extern "C"

namespace lart::weakmem {

struct CasRes { uint64_t value; bool success; };

struct Buffers;

_WM_INTERFACE int __lart_weakmem_buffer_size() noexcept;
_WM_INTERFACE Buffers *__lart_weakmem_state() noexcept;

_WM_INLINE
static char *baseptr( char *ptr ) noexcept {
    uintptr_t base = uintptr_t( ptr ) & ~_VM_PM_Off;
    return reinterpret_cast< char * >( base );
}

using MaskFlags = uint64_t;

static const MaskFlags setFlags = _LART_CF_RelaxedMemRuntime
                                | _VM_CF_IgnoreLoop;
static const MaskFlags restoreFlags = setFlags | _LART_CF_RelaxedMemCritSeen;

_WM_INTERFACE MaskFlags __lart_weakmem_mask_enter() noexcept {
    MaskFlags restore = MaskFlags( __vm_ctl_get( _VM_CR_Flags ) );
    __vm_ctl_flag( 0, setFlags );
    return restore;
}

_WM_INTERFACE void __lart_weakmem_mask_leave( MaskFlags restore ) noexcept {
    const MaskFlags flags = MaskFlags( __vm_ctl_get( _VM_CR_Flags ) );
    const bool do_interrupt = (flags & _LART_CF_RelaxedMemCritSeen)
                              && !(restore & _LART_CF_RelaxedMemRuntime);
    __vm_ctl_flag( /* clear: */ ((~restore) & restoreFlags),
                   /* set: */ (restore & restoreFlags) );

    if ( do_interrupt )
        __dios_reschedule();
}

void crit_seen() noexcept {
    __vm_ctl_flag( 0, _LART_CF_RelaxedMemCritSeen );
}

_WM_INLINE bool bypass( MaskFlags restore ) noexcept {
    return restore & (_VM_CF_DebugMode | _LART_CF_RelaxedMemRuntime);
}

_WM_INLINE bool kernel( MaskFlags restore ) noexcept {
    return restore & _VM_CF_KernelMode;
}

struct FullMask {
    _WM_INLINE FullMask() noexcept : restore( __lart_weakmem_mask_enter() ) { }
    _WM_INLINE ~FullMask() noexcept { __lart_weakmem_mask_leave( restore ); }
    _WM_INLINE bool bypass() const noexcept { return weakmem::bypass( restore ); }
    _WM_INLINE bool kernel() const noexcept { return weakmem::kernel( restore ); }

  private:
    MaskFlags restore;
};

template< typename T >
using ThreadMap = __dios::ArrayMap< __dios_task, T >;

template< typename T >
using Array = __dios::Array< T >;

template< typename It >
struct Range {
    using T = typename It::value_type;
    using iterator = It;
    using const_iterator = It;

    Range( It begin, It end ) noexcept : _begin( begin ), _end( end ) { }
    Range( const Range & ) noexcept = default;

    _WM_INLINE
    iterator begin() const noexcept { return _begin; }

    _WM_INLINE
    iterator end() const noexcept { return _end; }

  private:
    It _begin, _end;
};

template< typename It >
_WM_INLINE
static Range< It > range( It begin, It end ) noexcept { return Range< It >( begin, end ); }

template< typename T >
_WM_INLINE
static auto reversed( T &x ) noexcept { return range( x.rbegin(), x.rend() ); }

_WM_INLINE
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

_WM_INLINE
uint64_t _load( char *addr, uint32_t size ) noexcept {
    switch ( size ) {
        case 1: return *reinterpret_cast< uint8_t * >( addr );
        case 2: return *reinterpret_cast< uint16_t * >( addr );
        case 4: return *reinterpret_cast< uint32_t * >( addr );
        case 8: return *reinterpret_cast< uint64_t * >( addr );
        default: __dios_fault( _VM_F_Control, "Unhandled case" );
    }
    return 0;
}

struct BufferLine : brick::types::Ord {

    BufferLine() noexcept = default;
    BufferLine( char *addr, uint64_t value, uint32_t size, MemoryOrder order ) noexcept :
        addr( addr ), value( value ), size( size ), order( order )
    { }

    _WM_INLINE
    void store() noexcept {
        switch ( size ) {
            case 1: *reinterpret_cast< uint8_t * >( addr ) = value; break;
            case 2: *reinterpret_cast< uint16_t * >( addr ) = value; break;
            case 4: *reinterpret_cast< uint32_t * >( addr ) = value; break;
            case 8: *reinterpret_cast< uint64_t * >( addr ) = value; break;
            case 0: break; // fence
            default: __dios_fault( _VM_F_Control, "Unhandled case" );
        }
        __vm_test_crit( addr, size, _VM_MAT_Store, &crit_seen );
    }

    _WM_INLINE
    bool matches( const char *const from, const char *const to ) const noexcept {
        return (from <= addr && addr < to) || (addr <= from && from < addr + size);
    }

    _WM_INLINE
    bool matches( const char *const mem, const uint32_t size ) const noexcept {
        return matches( mem, mem + size );
    }

    _WM_INLINE
    bool matches( BufferLine &o ) noexcept {
        return matches( o.addr, o.size );
    }

    enum class Status : int8_t {
        Normal, Committed, DependentCommitLater
    };

    char *addr = nullptr;
    uint64_t value = 0;

    uint8_t size = 0;
    MemoryOrder order = MemoryOrder::NotAtomic;
    Status status = Status::Normal;

    _WM_INLINE
    auto as_tuple() const noexcept {
        // note: ignore status in comparison, take into account only part
        // relevant to memory behaviour
        return std::tie( addr, value, size, order );
    }

    _WM_INLINE
    void dump() const noexcept {
        char buffer[] = "[0xdeadbeafdeadbeaf ← 0xdeadbeafdeadbeaf; 0B; CWA SC]";
        snprintf( buffer, sizeof( buffer ) - 1, "[0x%llx ← 0x%llx; %dB; %c%c%c%s]",
                  uint64_t( addr ), value, size,
                  status == Status::Committed ? 'C' : (status == Status::DependentCommitLater ? 'D' : ' '),
                  subseteq( MemoryOrder::WeakCAS, order ) ? 'W' : ' ',
                  subseteq( MemoryOrder::AtomicOp, order ) ? 'A' : ' ',
                  ordstr( order ) );
        __vm_trace( _VM_T_Text, buffer );
    }
};

using Status = BufferLine::Status;

static_assert( sizeof( BufferLine ) == 3 * sizeof( uint64_t ) );

struct Buffer : Array< BufferLine >
{
    Buffer() = default;

    _WM_INLINE
    BufferLine &newest() noexcept { return end()[ -1 ]; }

    _WM_INLINE
    BufferLine &oldest() noexcept { return *begin(); }

    _WM_INLINE
    void erase( const int i ) noexcept {
        erase( begin() + i );
    }

    _WM_INLINE
    void erase( iterator i ) noexcept {
        std::move( i + 1, end(), i );
        pop_back();
    }

    template< typename Drop >
    _WM_INLINE
    void evict( bool flushed_only, Drop drop ) noexcept {
        auto e = std::remove_if( begin(), end(), [=]( BufferLine &l ) {
            return drop( l ) && (!flushed_only || l.status == Status::Committed);
        } );
        _resize( e - begin() );
    }

    _WM_INLINE
    void shrink( size_t n ) noexcept {
        std::destroy( begin() + n, end() );
        _resize( n );
    }

    _WM_INLINE
    void dump() const noexcept {
        for ( auto &e : *this )
            e.dump();
    }
};

struct Buffers : ThreadMap< Buffer > {

    using Super = ThreadMap< Buffer >;

    _WM_INLINE
    Buffer *getIfExists( __dios_task h ) noexcept {
        auto it = find( h );
        return it != end() ? &it->second : nullptr;
    }

    _WM_INLINE
    Buffer &get( __dios_task tid ) noexcept {
        Buffer *b = getIfExists( tid );
        if ( !b ) {
            b = &emplace( tid, Buffer{} ).first->second;
        }
        return *b;
    }

    _WM_INLINE
    Buffer *getIfExists() noexcept { return getIfExists( __dios_this_task() ); }

    _WM_INLINE
    Buffer &get() noexcept { return get( __dios_this_task() ); }

    template< typename T, T v >
    struct Const {
        template< typename... Args >
        _WM_INLINE
        T operator()( Args &&... ) const noexcept { return v; }
    };

    template< typename Self, typename Yield, typename Filter >
    _WM_INLINE
    static void _for_each( Self &self, Yield yield, Filter filt ) noexcept
    {
        for ( auto &p : self ) {
            if ( filt( p.first ) )
                for ( auto &l : p.second )
                    yield( p.first, p.second, l );
        }
    }

    template< typename Yield, typename Filter = Const< bool, true > >
    _WM_INLINE
    void for_each( Yield y, Filter filt = Filter() ) noexcept {
        _for_each( *this, y, filt );
    }

    template< typename Yield, typename Filter = Const< bool, true > >
    _WM_INLINE
    void for_each( Yield y, Filter filt = Filter() ) const noexcept {
        _for_each( *this, y, filt );
    }

    _WM_INLINE
    void push_tso( __dios_task tid, Buffer &b, BufferLine &&line ) noexcept
    {
        b.push_back( std::move( line ) );

        if ( __lart_weakmem_buffer_size() == 0 )
            return; // not bounded

        if ( b.size() > __lart_weakmem_buffer_size() ) {
            auto &oldest = b.oldest();
            tso_load< true >( oldest.addr, oldest.size, tid );
            oldest.store();
            b.erase( 0 );
        }
    }

    _WM_INLINE
    void dump() noexcept
    {
        auto *hids = __dios::have_debug() ? &__dios::get_debug().hids : nullptr;
        for ( auto &p : *this ) {
            if ( !p.second.empty() ) {
                auto tid = p.first;
                int nice_id = [&]() noexcept { if ( !hids ) return -1;
                                    auto it = hids->find( tid );
                                    return it == hids->end() ? -1 : it->second;
                                  }();

                char buffer[] = "thread 0xdeadbeafdeadbeaf*: ";
                snprintf( buffer, sizeof( buffer ) - 1, "thread: %d%s ",
                                  nice_id,
                                  p.first == __dios_this_task() ? "*:" : ": " );
                __vm_trace( _VM_T_Text, buffer );
                p.second.dump();
            }
        }
    }

    _WM_INLINE
    void flush( __dios_task tid, Buffer &buf ) noexcept
    {
        for ( auto &l : buf ) {
            tso_load< true >( l.addr, l.size, tid );
            l.store();
        }
        buf.clear();
    }

    _WM_INLINE
    void flush( Buffer &buf, BufferLine *entry, char *addr, int size ) noexcept {
        int kept = 0;
        auto move = [to_move = buf.begin()]( BufferLine &e ) mutable {
            if ( &e != to_move )
                *to_move = e;
            ++to_move;
        };
        for ( auto &e : buf )
        {
            if ( &e > entry ) {
                move( e );
                ++kept;
                continue;
            }
            if ( e.matches( addr, size ) ) {
                e.store();
                continue;
            }
            e.status = Status::Committed;
            ++kept;
            move( e );
        }
        if ( kept == 0 )
            buf.clear();
        else
            buf.shrink( kept );
    }

    template< bool skip_local = false >
    _WM_INLINE
    void tso_load( char *addr, int size, __dios_task tid ) noexcept
    {
        const int sz = this->size();
        bool dirty = false;
        int todo[sz];
        bool has_committed[sz];
        int needs_commit = 0;
        int i = 0, choices = 1;
        for ( auto &p : *this ) {
            todo[i] = 1;
            has_committed[i] = false;
            const bool nonloc = p.first != tid;
            if ( skip_local && !nonloc )
                goto next;
            for ( auto &e : p.second ) {
                if ( e.matches( addr, size ) ) {
                    dirty = dirty || nonloc;
                    if ( !skip_local && e.status == Status::Committed ) {
                        has_committed[i] = true;
                        ++needs_commit;
                    }
                    else
                        todo[i]++;
                }
            }
            choices *= todo[i];
          next:
            ++i;
        }
        if ( !dirty )
            return;

        int c = __vm_choose( choices );
        if ( needs_commit == 0 && c == 0 ) // shortcut if we are not flushing anything
            return;

        int cnt = 0;
        for ( int i = 0; i < sz; ++i ) {
            const int m = todo[i];
            cnt += (todo[i] = c % m) != 0 || has_committed[i];
            c /= m;
        }

        auto flush_buf = [&]( Buffer &buf, int last_flushed_idx ) noexcept {
            int lineid = 0;
            BufferLine *line = nullptr;
            for ( auto it = buf.begin(); it != buf.end(); ++it, ++lineid ) {
                if ( it->matches( addr, size ) )
                    line = it;
                else
                    continue;
                if ( it->status == Status::Committed )
                    continue;
                if ( lineid == last_flushed_idx )
                    break;
            }
            assert( line );
            flush( buf, line, addr, size );
        };

        // TODO: partial stores
        int last_id = __vm_choose( cnt );
        Buffer *last = nullptr;
        int last_flushed_idx_of_last = 0;

        for ( int i = 0, id = 0; i < sz; ++i ) {
            if ( todo[i] == 0 && !has_committed[i] )
                continue;
            auto &buf = begin()[i].second;
            if ( last_id == id ) {
                last = &buf;
                last_flushed_idx_of_last = todo[i];
                continue;
            }
            flush_buf( buf, todo[i] );
            ++id;
        }
        assert( last );
        flush_buf( *last, last_flushed_idx_of_last );
    }

    template< typename... Args >
    void evict( Buffer *local, Args... args ) {
        for ( auto &p : *this )
            p.second.evict( &p.second != local, args... );
    }
};

_WM_NOINLINE_WEAK extern "C" Buffers *__lart_weakmem_state() noexcept { return nullptr; }
_WM_INTERFACE void __lart_weakmem_init() noexcept {
    new ( __lart_weakmem_state() ) Buffers();
}

/* weak here is to prevent optimizer from eliminating calls to these functions
 * -- they will be replaced by weakmem transformation */
_WM_NOINLINE_WEAK extern "C" int __lart_weakmem_buffer_size() noexcept { return 32; }

_WM_NOINLINE_WEAK __attribute__((__annotate__("divine.debugfn")))
extern "C" void __lart_weakmem_dump() noexcept
{
    __lart_weakmem_state()->dump();
}

_WM_INTERFACE
void __lart_weakmem_debug_fence() noexcept
{
    // This fence is called only at the beginning of *debugfn* functions,
    // it does not need to keep store buffers consistent, debug functions will
    // not see them, it just needs to make all entries of this thread visible.
    // Also, it is uninterruptible by being part of *debugfn*
    auto *tid = __dios_this_task();
    if ( !tid )
        return;
    auto *buf = __lart_weakmem_state()->getIfExists( tid );
    if ( !buf )
        return;
    for ( auto &l : *buf )
        l.store();
    buf->clear();
}

_WM_INTERFACE
void __lart_weakmem_store( char *addr, uint64_t value, uint32_t size,
                           MemoryOrder ord, MaskFlags mask )
                           noexcept
{
    BufferLine line{ addr, value, size, ord };

    if ( bypass( mask ) || kernel( mask ) ) {
        line.store();
        return;
    }

    assert( __dios_pointer_get_type( addr ) != _VM_PT_Weak );
    // we are running without memory interrupt instrumentation and the other
    // threads have to see we have written something to our buffer (for the
    // sake of lazy loads)
    __vm_test_crit( addr, size, _VM_MAT_Store, &crit_seen );

    auto tid = __dios_this_task();
    auto &buf = __lart_weakmem_state()->get( tid );
    if ( subseteq( MemoryOrder::SeqCst, ord ) || subseteq( MemoryOrder::AtomicOp, ord ) ) {
        __lart_weakmem_state()->flush( tid, buf );
        line.store();
    }
    else
        __lart_weakmem_state()->push_tso( tid, buf, std::move( line ) );
}

_WM_INTERFACE
void __lart_weakmem_fence( MemoryOrder ord, MaskFlags mask ) noexcept
{
    if ( bypass( mask ) || kernel( mask ) )
        return; // should not be called recursivelly

    auto tid = __dios_this_task();
    auto *buf = __lart_weakmem_state()->getIfExists( tid );
    if ( !buf )
        return;

    if ( subseteq( MemoryOrder::SeqCst, ord ) )
        __lart_weakmem_state()->flush( tid, *buf );
}

union I64b {
    uint64_t i64;
    char b[8];
};

_WM_INLINE
static uint64_t doLoad( Buffer *buf, char *addr, uint32_t size ) noexcept
{
    I64b val = { .i64 = 0 };
    // always attempt load from memory to check for invalidated memory and idicate
    // load to tau reduction
    I64b mval = { .i64 = _load( addr, size ) };
    bool bmask[8] = { false };
    bool any = false;
    if ( buf ) {

        for ( const auto &it : reversed( *buf ) ) { // go from newest entry

            if ( !any && it.addr == addr && it.size >= size )
            {
                return it.value & (uint64_t(-1) >> (64 - (size * 8)));
            }
            if ( it.matches( addr, size ) ) {
                I64b bval = { .i64 = it.value };
                const int shift = intptr_t( it.addr ) - intptr_t( addr );
                if ( shift >= 0 ) {
                    for ( unsigned i = shift, j = 0; i < size && j < it.size; ++i, ++j )
                        if ( !bmask[ i ] ) {
                            val.b[ i ] = bval.b[ j ];
                            bmask[ i ] = true;
                            any = true;
                        }
                } else {
                    for ( unsigned i = 0, j = -shift; i < size && j < it.size; ++i, ++j )
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
        for ( unsigned i = 0; i < size; ++i )
            if ( !bmask[ i ] )
                val.b[ i ] = mval.b[ i ];
        return val.i64;
    }
    return mval.i64;
}

_WM_INTERFACE
uint64_t __lart_weakmem_load( char *addr, uint32_t size, MemoryOrder,
                              MaskFlags mask )
                              noexcept
{
    if ( bypass( mask ) || kernel( mask )  )
        return _load( addr, size );

    // other threads need to see we are loading somethig
    __vm_test_crit( addr, size, _VM_MAT_Load, &crit_seen );

    auto tid = __dios_this_task();
    Buffer *buf = __lart_weakmem_state()->getIfExists( tid );

    if ( __lart_weakmem_state()->size() != 1
         || __lart_weakmem_state()->begin()->first != tid )
        __lart_weakmem_state()->tso_load( addr, size, tid );

    return doLoad( buf, addr, size );
}

_WM_INTERFACE
CasRes __lart_weakmem_cas( char *addr, uint64_t expected, uint64_t value, uint32_t size,
                           MemoryOrder ordSucc, MemoryOrder ordFail, MaskFlags mask ) noexcept
{
    auto loaded = __lart_weakmem_load( addr, size, ordFail, mask );

    if ( loaded != expected
            || ( !bypass( mask ) && subseteq( MemoryOrder::WeakCAS, ordFail ) && __vm_choose( 2 ) ) )
        return { loaded, false };

    // TODO: when implementing NSW, make sure we order properly with _ordSucc

    __lart_weakmem_store( addr, value, size, ordSucc, mask );
    return { value, true };
}

_WM_INTERFACE
void __lart_weakmem_cleanup( int32_t cnt, ... ) noexcept
{
    FullMask mask;
    if ( mask.bypass() || mask.kernel() )
        return; // should not be called recursivelly

    va_list ptrs;
    va_start( ptrs, cnt );

    Buffer *buf = __lart_weakmem_state()->getIfExists();

    for ( int i = 0; i < cnt; ++i ) {
        char *ptr = va_arg( ptrs, char * );
        if ( !ptr )
            continue;

        const auto id = baseptr( ptr );
        __lart_weakmem_state()->evict( buf, [id]( BufferLine &l ) noexcept {
                            return baseptr( l.addr ) == id;
                        } );
    }
    va_end( ptrs );
}

_WM_INTERFACE
void __lart_weakmem_resize( char *ptr, uint32_t newsize ) noexcept {
    FullMask mask;
    if ( mask.bypass() || mask.kernel() )
        return; // should not be called recursivelly
    uint32_t orig = __vm_obj_size( ptr );
    if ( orig <= newsize )
        return;

    Buffer *buf = __lart_weakmem_state()->getIfExists();
    auto from = baseptr( ptr ) + newsize;
    __lart_weakmem_state()->evict( buf, [from]( BufferLine &l ) noexcept {
                        return l.addr >= from;
                    } );
}

} // namespace lart::weakmem
