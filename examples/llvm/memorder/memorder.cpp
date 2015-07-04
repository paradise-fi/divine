// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include "memorder.h"

template< typename Collection >
struct Reversed {
    using T = typename Collection::value_type;
    using iterator = std::reverse_iterator< T * >;

    Reversed( Collection &data ) : data( data ) { }

    iterator begin() { return iterator( data.end() ); }
    iterator end() { return iterator( data.begin() ); }

    Collection &data;
};

template< typename T >
Reversed< T > reversed( T &x ) { return Reversed< T >( x ); }

template< typename T >
static T *__alloc( int n ) { return reinterpret_cast< T * >( __divine_malloc( n * sizeof( T ) ) ); }

struct __BufferHelper {
    __BufferHelper() : buffers( nullptr ) {
        __divine_new_thread( []( void *self ) {
                reinterpret_cast< __BufferHelper * >( self )->flusher();
            }, this );
    }

    ~__BufferHelper() {
        if ( buffers ) {
            for ( int i = 0, e = __divine_heap_object_size( buffers ) / sizeof( Buffer * ); i < e; ++i )
                __divine_free( buffers[ i ] );
            __divine_free( buffers );
            buffers = nullptr;
        }
    }

    void flusher() {
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
        BufferLine() : addr( nullptr ), value( 0 ), bitwidth( 0 ) { }
        BufferLine( void *addr, uint64_t value, int bitwidth ) :
            addr( addr ), value( value ), bitwidth( bitwidth )
        { }

        void store() {
            switch ( bitwidth ) {
                case 8: store< uint8_t >(); break;
                case 16: store< uint16_t >(); break;
                case 32: store< uint32_t >(); break;
                case 64: store< uint64_t >(); break;
                default: __divine_problem( Problem::Other, "Unhandled case" );
            }
        }

        template< typename T >
        void store() {
            *reinterpret_cast< T * >( addr ) = T( value );
        }

        void *addr;
        uint64_t value;
        int bitwidth;
    };

    struct Buffer {
        using value_type = BufferLine;

        Buffer( const Buffer & ) = delete;

        int size() {
            return !this ? 0 : __divine_heap_object_size( this ) / sizeof( BufferLine );
        }

        BufferLine &operator[]( int ix ) {
            assert( ix < size() );
            return begin()[ ix ];
        }

        BufferLine *begin() {
            return reinterpret_cast< BufferLine * >( this );
        }
        BufferLine *end() { return begin() + size(); }
    };

    Buffer *&get() {
        int tid = __divine_get_tid();
        int cnt = !buffers ? 0 : __divine_heap_object_size( buffers ) / sizeof( Buffer * );
        if ( tid >= cnt ) {
            Buffer **n = __alloc< Buffer * >( tid + 1 );
            if ( buffers )
                std::copy( buffers, buffers + cnt, n );
            for ( int i = cnt; i <= tid; ++i )
                n[ i ] = nullptr;
            __divine_free( buffers );
            buffers = n;
        }
        return buffers[ tid ];
    }

    Buffer &cast( void *data ) { return *reinterpret_cast< Buffer * >( data ); }

    BufferLine pop() {
        return pop( get() );
    }

    BufferLine pop( Buffer *&buf ) {
        const auto size = buf->size();
        assert( size > 0 );
        BufferLine out = (*buf)[ 0 ];
        if ( size > 1 ) {
            auto &n = cast( __divine_malloc( sizeof( BufferLine ) * (size - 1) ) );
            std::copy( buf->begin() + 1, buf->end(), n.begin() );
            __divine_free( buf );
            buf = &n;
        } else {
            __divine_free( buf );
            buf = nullptr;
        }
        return out;
    }

    void push( BufferLine line ) {
        auto &buf = *get();
        const auto size = buf.size();
        auto &n = cast( __divine_malloc( sizeof( BufferLine ) * (size + 1) ) );
        std::copy( buf.begin(), buf.end(), n.begin() );
        n[ size ] = line;
        __divine_free( &buf );
        get() = &n;
    }

    void drop() {
        __divine_free( get() );
        get() = nullptr;
    }

    Buffer **buffers;
};

static __BufferHelper __storeBuffers;

void __dsb_store( void *addr, uint64_t value, int bitwidth ) {
    __divine_interrupt_mask();
    __storeBuffers.push( { addr, value, bitwidth } );
    if ( __storeBuffers.get()->size() > __STORE_BUFFER_SIZE )
        __storeBuffers.pop().store();
    __divine_interrupt_unmask();
    __divine_interrupt();
}

void __dsb_flushOne() {
    __divine_interrupt_mask();
    __storeBuffers.pop().store();
    __divine_interrupt_unmask();
    __divine_interrupt();
}

void __dsb_flush() {
    __divine_interrupt_mask();
    for ( auto l : *__storeBuffers.get() )
        l.store();
    __storeBuffers.drop();
    __divine_interrupt_unmask();
    __divine_interrupt();
}

uint64_t __dsb_load( void *addr, int bitwidth ) {
    __divine_interrupt_mask();
    uint64_t val;
    auto buf = __storeBuffers.get();
    if ( buf ) {
        for ( auto &it : reversed( *buf ) )
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
    __divine_interrupt_unmask();
    __divine_interrupt();
    return val;
}
