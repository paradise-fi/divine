// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <weakmem.h>
#include <algorithm> // reverse iterator

#define forceinline __attribute__((always_inline))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgcc-compat"

template< typename Collection >
struct Reversed {
    using T = typename Collection::value_type;
    using iterator = std::reverse_iterator< T * >;

    Reversed( Collection &data ) _lart_weakmem_bypass_ : data( data ) { }
    Reversed( const Reversed & ) _lart_weakmem_bypass_ = default;

    iterator begin() _lart_weakmem_bypass_ { return iterator( data.end() ); }
    iterator end() _lart_weakmem_bypass_ { return iterator( data.begin() ); }

    Collection &data;
};

template< typename T >
Reversed< T > reversed( T &x ) _lart_weakmem_bypass_ { return Reversed< T >( x ); }

template< typename T >
static T *__alloc( int n ) _lart_weakmem_bypass_ { return reinterpret_cast< T * >( __divine_malloc( n * sizeof( T ) ) ); }

struct __BufferHelper {

    // perform no initialization, first load/store can run before global ctors
    // so we will intialize if buffers == nullptr
    __BufferHelper() = default;

    void start() _lart_weakmem_bypass_ {
        __divine_new_thread( &__BufferHelper::startFlusher, this );
    }

    static void startFlusher( void *self ) _lart_weakmem_bypass_ {
        reinterpret_cast< __BufferHelper * >( self )->flusher();
    }

    ~__BufferHelper() _lart_weakmem_bypass_ {
        if ( buffers ) {
            for ( int i = 0, e = __divine_heap_object_size( buffers ) / sizeof( Buffer * ); i < e; ++i )
                __divine_free( buffers[ i ] );
            __divine_free( buffers );
            buffers = nullptr;
        }
    }

    void flusher() _lart_weakmem_bypass_ {
        while ( true ) {
            __divine_interrupt();
            __divine_interrupt_mask();
            if ( buffers ) {
                int tid = __divine_choice( __divine_heap_object_size( buffers ) / sizeof( Buffer * ) );
                auto b = buffers[ tid ];
                if ( b->size() ) {
                    auto line = pop( buffers[ tid ] );
                    line.store();
                }
            }
            __divine_interrupt_unmask();
        }
    }

    struct BufferLine {
        BufferLine() _lart_weakmem_bypass_ : addr( nullptr ), value( 0 ), bitwidth( 0 ) { }
        BufferLine( void *addr, uint64_t value, int bitwidth ) _lart_weakmem_bypass_ :
            addr( reinterpret_cast< char * >( addr ) ), value( value ), bitwidth( bitwidth )
        { }

        void store() _lart_weakmem_bypass_ forceinline {
            switch ( bitwidth ) {
                case 1: case 8: store< uint8_t >(); break;
                case 16: store< uint16_t >(); break;
                case 32: store< uint32_t >(); break;
                case 64: store< uint64_t >(); break;
                default: __divine_problem( Problem::Other, "Unhandled case" );
            }
        }

        template< typename T >
        void store() _lart_weakmem_bypass_ forceinline {
            *reinterpret_cast< T * >( addr ) = T( value );
        }

        char *addr;
        uint64_t value;
        int bitwidth;
    };

    struct Buffer {
        using value_type = BufferLine;

        Buffer( const Buffer & ) = delete;

        int size() _lart_weakmem_bypass_ forceinline {
            return !this ? 0 : __divine_heap_object_size( this ) / sizeof( BufferLine );
        }

        BufferLine &operator[]( int ix ) _lart_weakmem_bypass_ forceinline {
            assert( ix < size() );
            return begin()[ ix ];
        }

        BufferLine *begin() _lart_weakmem_bypass_ forceinline {
            return reinterpret_cast< BufferLine * >( this );
        }
        BufferLine *end() _lart_weakmem_bypass_ forceinline { return begin() + size(); }
    };

    Buffer *&get() _lart_weakmem_bypass_ forceinline {
        int tid = __divine_get_tid();
        int cnt = !buffers ? 0 : __divine_heap_object_size( buffers ) / sizeof( Buffer * );
        if ( tid >= cnt ) {
            Buffer **n = __alloc< Buffer * >( tid + 1 );
            if ( buffers )
                __divine_memcpy( n, buffers, cnt * sizeof( Buffer * ) );
            else
                start(); // intialize flushing thread
            for ( int i = cnt; i <= tid; ++i )
                n[ i ] = nullptr;
            __divine_free( buffers );
            buffers = n;
        }
        return buffers[ tid ];
    }

    Buffer &cast( void *data ) _lart_weakmem_bypass_ forceinline { return *reinterpret_cast< Buffer * >( data ); }

    BufferLine pop() _lart_weakmem_bypass_ forceinline {
        return pop( get() );
    }

    BufferLine pop( Buffer *&buf ) _lart_weakmem_bypass_ forceinline {
        const auto size = buf->size();
        assert( size > 0 );
        BufferLine out = (*buf)[ 0 ];
        if ( size > 1 ) {
            auto &n = cast( __divine_malloc( sizeof( BufferLine ) * (size - 1) ) );
            __divine_memcpy( n.begin(), buf->begin() + 1, (size - 1) * sizeof( BufferLine ) );
            __divine_free( buf );
            buf = &n;
        } else {
            __divine_free( buf );
            buf = nullptr;
        }
        return out;
    }

    void push( const BufferLine &line ) _lart_weakmem_bypass_ forceinline {
        auto &buf = *get();
        const auto size = buf.size();
        auto &n = cast( __divine_malloc( sizeof( BufferLine ) * (size + 1) ) );
        __divine_memcpy( n.begin(), buf.begin(), size * sizeof( BufferLine ) );
        n[ size ] = line;
        __divine_free( &buf );
        get() = &n;
    }

    void drop() _lart_weakmem_bypass_ forceinline {
        __divine_free( get() );
        get() = nullptr;
    }

    Buffer **buffers;
};

__BufferHelper __storeBuffers;

volatile int __lart_weakmem_buffer_size = 2;

void __lart_weakmem_store_tso( void *addr, uint64_t value, int bitwidth ) {
    __divine_interrupt_mask();
    __BufferHelper::BufferLine bl{ addr, value, bitwidth };
    if ( __divine_is_private( addr ) )
        bl.store();
    else {
        __storeBuffers.push( bl );
        if ( __storeBuffers.get()->size() > __lart_weakmem_buffer_size )
            __storeBuffers.pop().store();
    }
}

void __lart_weakmem_flush() {
    __divine_interrupt_mask();
    for ( auto &l : *__storeBuffers.get() )
        l.store();
    __storeBuffers.drop();
}

union I64b {
    uint64_t i64;
    char b[8];
};

uint64_t __lart_weakmem_load_tso( void *_addr, int bitwidth ) {
    __divine_interrupt_mask();
    if ( __divine_is_private( _addr ) ) {
        switch ( bitwidth ) {
            case 1: case 8: return *reinterpret_cast< uint8_t * >( _addr );
            case 16: return *reinterpret_cast< uint16_t * >( _addr );
            case 32: return *reinterpret_cast< uint32_t * >( _addr );
            case 64: return *reinterpret_cast< uint64_t * >( _addr );
            default: __divine_problem( Problem::Other, "Unhandled case" );
        }
    }

    I64b val = { .i64 = 0 };
    char *addr = reinterpret_cast< char * >( _addr );
    bool bmask[8] = { false };
    bool any = false;
    auto buf = __storeBuffers.get();
    if ( buf ) {
        for ( const auto &it : reversed( *buf ) ) {
            if ( !any && it.addr == addr && it.bitwidth >= bitwidth )
                return it.value & (uint64_t(-1) >> (64 - bitwidth));
            if ( addr + (bitwidth / 8) > it.addr && addr < it.addr + (it.bitwidth / 8) ) {
                I64b bval = { .i64 = it.value };
                const int shift = intptr_t( it.addr ) - intptr_t( addr );
                if ( shift >= 0 ) {
                    for ( int i = shift, j = 0; i < bitwidth / 8 && j < it.bitwidth / 8; ++i, ++j )
                        if ( !bmask[ i ] ) {
                            val.b[ i ] = bval.b[ j ];
                            bmask[ i ] = true;
                            any = true;
                        }
                } else {
                    for ( int i = 0, j = -shift; i < bitwidth / 8 && j < it.bitwidth / 8; ++i, ++j )
                        if ( !bmask[ i ] ) {
                            val.b[ i ] = bval.b[ j ];
                            bmask[ i ] = true;
                            any = true;
                        }
                }
                bool all = true;
            }
        }
    }
    I64b mval;
    switch ( bitwidth ) {
        case 1: case 8: mval.i64 = *reinterpret_cast< uint8_t * >( addr ); break;
        case 16: mval.i64 = *reinterpret_cast< uint16_t * >( addr ); break;
        case 32: mval.i64 = *reinterpret_cast< uint32_t * >( addr ); break;
        case 64: mval.i64 = *reinterpret_cast< uint64_t * >( addr ); break;
        default: __divine_problem( Problem::Other, "Unhandled case" );
    }
    if ( any ) {
        for ( int i = 0; i < bitwidth / 8; ++i )
            if ( !bmask[ i ] )
                val.b[ i ] = mval.b[ i ];
        return val.i64;
    }
    return mval.i64;
}

uint64_t __lart_weakmem_load_pso( void *addr, int bitwidth ) {
    __divine_problem( 1, "unimplemented" );
}
void __lart_weakmem_store_pso( void *addr, uint64_t value, int bitwidth ) {
    __divine_problem( 1, "unimplemented" );
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

void __lart_weakmem_memmove( void *_dst, const void *_src, size_t n ) {
    volatile char *dst = const_cast< volatile char * >( reinterpret_cast< char * >( _dst ) );
    char *src = reinterpret_cast< char * >( const_cast< void * >( _src ) );
    if ( dst < src )
        internal_memcpy< 1 >( dst, src, n );
    else if ( dst > src )
        internal_memcpy< -1 >( dst + n, src + n, n );
}

void __lart_weakmem_memcpy( void *_dst, const void *_src, size_t n ) {
    volatile char *dst = const_cast< volatile char * >( reinterpret_cast< char * >( _dst ) );
    char *src = reinterpret_cast< char * >( const_cast< void * >( _src ) );
    internal_memcpy< 1 >( dst, src, n );
}

void __lart_weakmem_memset( void *_dst, int c, size_t n ) {
    volatile char *dst = const_cast< volatile char * >( reinterpret_cast< char * >( _dst ) );
    for ( ; n; --n, ++dst )
        *dst = c;
}

#pragma GCC diagnostic pop
