/* TAGS: min c++ */
#include <atomic>
#include <cstdint>
#include <cassert>

#define OPERATOR( T ) \
    friend bool operator==( T a, T b ) { \
        return a.x == b.x; \
    }

template< typename T >
struct Def {
    T operator()() { return T(); }
};

template< typename T >
struct Def< T * > {
    T *operator()() { return nullptr; }
};

template< typename T >
T def() { return Def< T >()(); }

template< typename T >
struct Conv {
    using R = T;
    T operator()( int i ) { return i; }
};

template< typename T >
struct Conv< T * > {
    using R = std::ptrdiff_t;
    R operator()( int i ) { return i; }
};

template< typename T >
auto conv( int i ) -> typename Conv< T >::R {
    return Conv< T >()( i );
}

struct St8 {
    char x;

    OPERATOR( St8 )
};

struct St16 {
    int16_t x;

    OPERATOR( St16 )
};

struct St32 {
    int32_t x;

    OPERATOR( St32 )
};

struct St64 {
    int64_t x;

    OPERATOR( St64 )
};

struct Large {
    int64_t x;
    int64_t y[ 15 ];

    OPERATOR( Large )
};

template< typename T >
void genericIT() {
    std::atomic< T > defcon;
    std::atomic< T > initcon{ def< T >() };
    std::atomic< T > macroinit = ATOMIC_VAR_INIT( def< T >() );
    std::atomic< T > braceinit = { def< T >() };

    defcon = def< T >();
    assert( defcon.is_lock_free() );
    assert( std::atomic_is_lock_free( &defcon ) );

    std::atomic_init( &initcon, def< T >() );

    defcon.store( def< T >() );
    assert( defcon.load() == def< T >() );
    assert( defcon == def< T >() );
    assert( defcon.exchange( def< T >() ) == def< T >() );

    T t{};
    assert( defcon.compare_exchange_strong( t, def< T >() ) );
    defcon.compare_exchange_weak( t, def< T >() );

    std::atomic< T > *ptr = &defcon;
    std::atomic_store( ptr, def< T >() );
    std::atomic_store_explicit( ptr, def< T >(), std::memory_order_seq_cst );

    assert( std::atomic_load( ptr ) == def< T >() );
    assert( std::atomic_load_explicit( ptr, std::memory_order_seq_cst ) == def< T >() );

    assert( std::atomic_exchange( ptr, def< T >() ) == def< T >() );
    assert( std::atomic_exchange_explicit( ptr, def< T >(), std::memory_order_seq_cst ) == def< T >() );

    assert( std::atomic_compare_exchange_strong( ptr, &t, def< T >() ) );
    assert( std::atomic_compare_exchange_strong_explicit( ptr, &t, def< T >(),
                std::memory_order_seq_cst, std::memory_order_seq_cst ) );
    std::atomic_compare_exchange_weak( ptr, &t, def< T >() );
    std::atomic_compare_exchange_weak_explicit( ptr, &t, def< T >(),
                std::memory_order_seq_cst, std::memory_order_seq_cst );
}

template< typename T >
void ptrIT() {
    genericIT< T >();

    std::atomic< T > at( 0 );
    const T p = at;

    assert( at.fetch_add( 1 ) == p );
    assert( at == p + 1 );
    assert( ++at == p + 2 );
    assert( at == p + 2 );
    assert( at++ == p + 2 );
    assert( at == p + 3 );
    assert( ( at += 2 ) == p + 5 );
    assert( at == p + 5 );

    assert( at.fetch_sub( 2 ) == p + 5 );
    assert( at == p + 3 );
    assert( --at == p + 2 );
    assert( at == p + 2 );
    assert( at-- == p + 2 );
    assert( at == p + 1 );
    assert( ( at -= 1 ) == 0 );
    assert( at == 0 );

    at += 10;
    assert( ( at = p ) == p );
}

template< typename T >
void integralIT() {
    ptrIT< T >();

    std::atomic< T > at( 42 );
    T t = 42;

    assert( std::atomic_fetch_add( &at, conv< T >( 10 ) ) == t );
    assert( at == t + 10 );
    assert( std::atomic_fetch_add_explicit( &at,  T( 10 ), std::memory_order_seq_cst ) == t + 10 );
    assert( at == t + 20 );

    assert( std::atomic_fetch_sub( &at, T( 10 ) ) == t + 20 );
    assert( at == t + 10 );
    assert( std::atomic_fetch_sub_explicit( &at, T( 10 ), std::memory_order_seq_cst ) == t + 10 );
    assert( at == t );

    at.exchange( 0xff );
    t = 0xff;

    assert( at.fetch_and( 0x0f ) == t );
    assert( at == ( t &= 0x0f ) );
    assert( at.fetch_and( 0x01, std::memory_order_seq_cst ) == t );
    assert( at == ( t &= 0x01 ) );

    assert( t == T( 0x01 ) );

    assert( at.fetch_or( 0x0f ) == t );
    assert( at == ( t |= 0x0f ) );
    assert( at.fetch_or( 0xff, std::memory_order_seq_cst ) == t );
    assert( at == ( t |= 0xff ) );

    assert( t == T( 0xff ) );

    assert( at.fetch_xor( 0xf0 ) == t );
    assert( at == ( t ^= 0xf0 ) );
    assert( at.fetch_xor( 0xff, std::memory_order_seq_cst ) == t );
    assert( at == ( t ^= 0xff ) );

    assert( t == T( 0xf0 ) );

    assert( at.compare_exchange_strong( t, 0xff ) );

    std::atomic< T > at2{ T( at ) };

    assert( at.fetch_and( 0x0f ) == std::atomic_fetch_and( &at2, T( 0x0f ) ) );
    assert( at.fetch_and( 0x01 ) == std::atomic_fetch_and_explicit( &at2, T( 0x01 ), std::memory_order_seq_cst ) );

    assert( at.fetch_or( 0x0f ) == std::atomic_fetch_or( &at2, T( 0x0f ) ) );
    assert( at.fetch_or( 0xff ) == std::atomic_fetch_or_explicit( &at2, T( 0xff ), std::memory_order_seq_cst ) );

    assert( at.fetch_xor( 0xf0 ) == std::atomic_fetch_xor( &at2, T( 0xf0 ) ) );
    assert( at.fetch_xor( 0xff ) == std::atomic_fetch_xor_explicit( &at2, T( 0xff ), std::memory_order_seq_cst ) );

    while ( !at.compare_exchange_weak( t, 0xff ) ) { }
    while ( !std::atomic_compare_exchange_weak( &at2, &t, T( 0xff ) ) ) { }
    t = 0xff;

    assert( ( at &= 0x0f ) == ( t &= 0x0f ) );
    assert( ( at |= 0x71 ) == ( t |= 0x71 ) );
    assert( ( at ^= 0xf0 ) == ( t ^= 0xf0 ) );
}

std::atomic_flag fstatic = ATOMIC_FLAG_INIT;

void atomicFlagIT() {
    std::atomic_flag f = ATOMIC_FLAG_INIT;

    assert( !f.test_and_set() );
    assert( f.test_and_set() );
    f.clear();
    assert( !f.test_and_set( std::memory_order_seq_cst ) );
    f.clear( std::memory_order_seq_cst );

    assert( !std::atomic_flag_test_and_set( &f ) );
    assert( std::atomic_flag_test_and_set_explicit( &f, std::memory_order_seq_cst ) );
    std::atomic_flag_clear( &f );
    assert( !std::atomic_flag_test_and_set( &f ) );
    std::atomic_flag_clear_explicit( &f, std::memory_order_seq_cst );
    assert( !std::atomic_flag_test_and_set( &f ) );
}

int main( void ) {

    atomicFlagIT();
    genericIT< bool >();

    integralIT< char >();
    integralIT< int16_t >();
    integralIT< int32_t >();
    integralIT< int64_t >();

    integralIT< unsigned char >();
    integralIT< uint16_t >();
    integralIT< uint32_t >();
    integralIT< uint64_t >();

    genericIT< St8 >();
    genericIT< St16 >();
    genericIT< St32 >();
    genericIT< St64 >();

    ptrIT< bool * >();
    ptrIT< char * >();
    ptrIT< int16_t * >();
    ptrIT< int32_t * >();
    ptrIT< int64_t * >();

    ptrIT< St8 * >();
    ptrIT< St16 * >();
    ptrIT< St32 * >();
    ptrIT< St64 * >();

    ptrIT< Large * >();
}
