// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include "memorder.h"
#include <algorithm> // reverse iterator

template< typename Collection >
struct Reversed {
    using T = typename Collection::value_type;
    using iterator = std::reverse_iterator< T * >;

    Reversed( Collection &data ) __LART_WM_DIRECT__ : data( data ) { }
    Reversed( const Reversed & ) __LART_WM_DIRECT__ = default;

    iterator begin() __LART_WM_DIRECT__ { return iterator( data.end() ); }
    iterator end() __LART_WM_DIRECT__ { return iterator( data.begin() ); }

    Collection &data;
};

template< typename T >
Reversed< T > reversed( T &x ) __LART_WM_DIRECT__ { return Reversed< T >( x ); }

template< typename T >
static T *__alloc( int n ) __LART_WM_DIRECT__ { return reinterpret_cast< T * >( __divine_malloc( n * sizeof( T ) ) ); }

struct __BufferHelper {

    // perform no initialization, first load/store can run before global ctors
    // so we will intialize if buffers == nullptr
    __BufferHelper() = default;

    void start() __LART_WM_DIRECT__ {
        __divine_new_thread( []( void *self ) {
                reinterpret_cast< __BufferHelper * >( self )->flusher();
            }, this );
    }

    ~__BufferHelper() __LART_WM_DIRECT__ {
        if ( buffers ) {
            for ( int i = 0, e = __divine_heap_object_size( buffers ) / sizeof( Buffer * ); i < e; ++i )
                __divine_free( buffers[ i ] );
            __divine_free( buffers );
            buffers = nullptr;
        }
    }

    void flusher() __LART_WM_DIRECT__ {
        while ( true ) {
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
            __divine_interrupt();
        }
    }

    struct BufferLine {
        BufferLine() __LART_WM_DIRECT__ : addr( nullptr ), value( 0 ), bitwidth( 0 ) { }
        BufferLine( void *addr, uint64_t value, int bitwidth ) __LART_WM_DIRECT__ :
            addr( addr ), value( value ), bitwidth( bitwidth )
        { }

        void store() __LART_WM_DIRECT__ {
            switch ( bitwidth ) {
                case 8: store< uint8_t >(); break;
                case 16: store< uint16_t >(); break;
                case 32: store< uint32_t >(); break;
                case 64: store< uint64_t >(); break;
                default: __divine_problem( Problem::Other, "Unhandled case" );
            }
        }

        template< typename T >
        void store() __LART_WM_DIRECT__ {
            *reinterpret_cast< T * >( addr ) = T( value );
        }

        void *addr;
        uint64_t value;
        int bitwidth;
    };

    struct Buffer {
        using value_type = BufferLine;

        Buffer( const Buffer & ) = delete;

        int size() __LART_WM_DIRECT__ {
            return !this ? 0 : __divine_heap_object_size( this ) / sizeof( BufferLine );
        }

        BufferLine &operator[]( int ix ) __LART_WM_DIRECT__ {
            assert( ix < size() );
            return begin()[ ix ];
        }

        BufferLine *begin() __LART_WM_DIRECT__ {
            return reinterpret_cast< BufferLine * >( this );
        }
        BufferLine *end() __LART_WM_DIRECT__ { return begin() + size(); }
    };

    Buffer *&get() __LART_WM_DIRECT__ {
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

    Buffer &cast( void *data ) __LART_WM_DIRECT__ { return *reinterpret_cast< Buffer * >( data ); }

    BufferLine pop() __LART_WM_DIRECT__ {
        return pop( get() );
    }

    BufferLine pop( Buffer *&buf ) __LART_WM_DIRECT__ {
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

    void push( const BufferLine &line ) __LART_WM_DIRECT__ {
        auto &buf = *get();
        const auto size = buf.size();
        auto &n = cast( __divine_malloc( sizeof( BufferLine ) * (size + 1) ) );
        __divine_memcpy( n.begin(), buf.begin(), size * sizeof( BufferLine ) );
        n[ size ] = line;
        __divine_free( &buf );
        get() = &n;
    }

    void drop() __LART_WM_DIRECT__ {
        __divine_free( get() );
        get() = nullptr;
    }

    Buffer **buffers;
    bool masked;
};

__BufferHelper __storeBuffers;

#define WM_MASK() __divine_interrupt_mask(); const bool recursive = __storeBuffers.masked; __storeBuffers.masked = true
#define WM_UNMASK() if ( !recursive ) {__storeBuffers.masked = false; __divine_interrupt_unmask(); __divine_interrupt(); } (void)(0)

void __dsb_store( void *addr, uint64_t value, int bitwidth ) {
    WM_MASK();
    __BufferHelper::BufferLine line( addr, value, bitwidth );
    if ( recursive ) {
        line.store();
        return;
    }

    __storeBuffers.push( line );
    if ( __storeBuffers.get()->size() > __STORE_BUFFER_SIZE )
        __storeBuffers.pop().store();
    WM_UNMASK();
}

void __dsb_flushOne() {
    WM_MASK();
    __storeBuffers.pop().store();
    WM_UNMASK();
}

void __dsb_flush() {
    WM_MASK();
    for ( auto &l : *__storeBuffers.get() )
        l.store();
    __storeBuffers.drop();
    WM_UNMASK();
}

uint64_t __dsb_load( void *addr, int bitwidth ) {
    WM_MASK();
    uint64_t val;
    auto buf = __storeBuffers.get();
    if ( buf ) {
        for ( const auto &it : reversed( *buf ) )
            if ( it.addr == addr ) {
                val = it.value;
                goto ret;
            }
    }
    switch ( bitwidth ) {
        case 8: val = *reinterpret_cast< uint8_t * >( addr ); break;
        case 16: val = *reinterpret_cast< uint16_t * >( addr ); break;
        case 32: val = *reinterpret_cast< uint32_t * >( addr ); break;
        case 64: val = *reinterpret_cast< uint64_t * >( addr ); break;
        default: __divine_problem( Problem::Other, "Unhandled case" );
    }
ret:
    WM_UNMASK();
    return val;
}
