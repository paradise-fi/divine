// -*- C++ -*-

#include <wibble/test.h>
#include <divine/toolkit/bittuple.h>

using namespace divine;

struct TestBitTuple {
    using U10 = BitField< unsigned, 10 >;
    using T10_10 = BitTuple< U10, U10 >;

    int bitcount( uint32_t word ) {
        int i = 0;
        while ( word ) {
            if ( word & 1 )
                ++i;
            word >>= 1;
        }
        return i;
    }

    Test mask() {
        /* only works on little endian machines ... */
        assert_eq( 0xFF00u, ::mask( 8, 8 ) );
        assert_eq( 0xF000u, ::mask( 12, 4 ) );
        assert_eq( 0x0F00u, ::mask( 8, 4 ) );
        assert_eq( 60u, ::mask( 2, 4 ) );// 0b111100
        assert_eq( 28u, ::mask( 2, 3 ) );// 0b11100
    }

    Test bitcopy() {
        uint32_t a = 42, b = 11;
        ::bitcopy( BitPointer( &a ), BitPointer( &b ), 32 );
        assert_eq( a, b );
        a = 0xFF00;
        ::bitcopy( BitPointer( &a ), BitPointer( &b, 8 ), 24 );
        assert_eq( b, 0xFF0000u | 42u );
        a = 0;
        ::bitcopy( BitPointer( &b, 8 ), BitPointer( &a ), 24 );
        assert_eq( a, 0xFF00u );
        ::bitcopy( BitPointer( &a, 8 ), BitPointer( &b, 8 ), 8 );

        a = 0x3FF;
        b = 0;
        ::bitcopy( BitPointer( &a, 0 ), BitPointer( &b, 0 ), 10 );
        assert_eq( b, 0x3FFu );
    }

    Test field() {
        int a = 42, b = 0;
        typedef BitField< int, 10 > F;
        F::Virtual f;
        f.fromReference( BitPointer( &b ) );
        f.set( a );
        assert_eq( a, 42 );
        assert_eq( a, f );
    }

    Test basic() {
        T10_10 x;
        assert_eq( T10_10::bitwidth, 20 );
        assert_eq( T10_10::offset< 0 >(), 0 );
        assert_eq( T10_10::offset< 1 >(), 10 );
        auto a = get< 0 >( x );
        auto b = get< 1 >( x );
        a.set( 5 );
        b.set( 7 );
        assert_eq( a, 5u );
        assert_eq( b, 7u );
    }

    Test big() {
        BitTuple< BitField< uint64_t, 63 >, BitField< uint64_t, 63 > > x;
        assert_eq( x.bitwidth, 126 );
        assert_eq( x.offset< 0 >(), 0 );
        assert_eq( x.offset< 1 >(), 63 );
        get< 0 >( x ).set( (1ull << 62) + 7 );
        assert_eq( get< 0 >( x ), (1ull << 62) + 7 );
        assert_eq( get< 1 >( x ), 0u );
        get< 0 >( x ).set( 0 );
        get< 1 >( x ).set( (1ull << 62) + 7 );
        assert_eq( get< 0 >( x ), 0u );
        assert_eq( get< 1 >( x ), (1ull << 62) + 7 );
        get< 0 >( x ).set( (1ull << 62) + 11 );
        assert_eq( get< 0 >( x ), (1ull << 62) + 11 );
        assert_eq( get< 1 >( x ), (1ull << 62) + 7 );
    }

    Test structure() {
        BitTuple< BitField< std::pair< uint64_t, uint64_t >, 120 >, BitField< uint64_t, 63 > > x;
        auto v = std::make_pair( (uint64_t( 1 ) << 62) + 7, uint64_t( 33 ) );
        assert_eq( x.bitwidth, 183 );
        assert_eq( x.offset< 0 >(), 0 );
        assert_eq( x.offset< 1 >(), 120 );
        get< 1 >( x ).set( 333 );
        assert_eq( get< 1 >( x ), 333u );
        get< 0 >( x ).set( v );
        assert_eq( get< 1 >( x ), 333u );
        assert( get< 0 >( x ).get() == v );
    }

    Test nested() {
        typedef BitTuple< T10_10, T10_10, BitField< unsigned, 3 > > X;
        X x;
        assert_eq( X::bitwidth, 43 );
        assert_eq( X::offset< 0 >(), 0 );
        assert_eq( X::offset< 1 >(), 20 );
        assert_eq( X::offset< 2 >(), 40 );
        auto a = get< 0 >( x );
        auto b = get< 1 >( x );
        get< 0 >( a ).set( 5 );
        get< 1 >( a ).set( 7 );
        get< 0 >( b ).set( 13 );
        get< 1 >( b ).set( 533 );
        get< 2 >( x ).set( 15 ); /* we expect to lose the MSB */
        assert_eq( get< 0 >( a ), 5u );
        assert_eq( get< 1 >( a ), 7u );
        assert_eq( get< 0 >( b ), 13u );
        assert_eq( get< 1 >( b ), 533u );
        assert_eq( get< 2 >( x ), 7u );
    }

    Test locked() {
        BitTuple<
            BitField< int, 15 >,
            BitLock,
            BitField< int, 16 >
        > bt;

        get< 1 >( bt ).lock();

        assert_eq( get< 0 >( bt ), 0 );
        assert_eq( get< 2 >( bt ), 0 );
        assert( get< 1 >( bt ).locked() );
        assert( get< 0 >( bt ).word() );

        get< 0 >( bt ) = 1;
        get< 2 >( bt ) = 1;

        assert_eq( get< 0 >( bt ), 1 );
        assert_eq( get< 2 >( bt ), 1 );

        assert_eq( bitcount( get< 0 >( bt ).word() ), 3 );

        get< 1 >( bt ).unlock();
        assert_eq( get< 0 >( bt ), 1 );
        assert_eq( get< 2 >( bt ), 1 );
        assert( !get< 1 >( bt ).locked() );

        assert_eq( bitcount( get< 0 >( bt ).word() ), 2 );

        get< 0 >( bt ) = 0;
        get< 2 >( bt ) = 0;
        assert( !get< 0 >( bt ).word() );
    }

    Test assign() {
        BitTuple<
            BitField< bool, 1 >,
            BitField< int, 6 >,
            BitField< bool, 1 >
        > tuple;

        get< 0 >( tuple ) = true;
        get< 2 >( tuple ) = get< 0 >( tuple );
        assert( get< 2 >( tuple ).get() );
    }

    struct OperatorTester {
        int value;
        int expected;
        OperatorTester &operator++() { assert_die(); return *this; }
        OperatorTester operator++( int ) { assert_die(); return *this; }
        OperatorTester &operator--() { assert_die(); return *this; }
        OperatorTester &operator--( int ) { assert_die(); return *this; }
        OperatorTester &operator+=( int ) { assert_die(); return *this; }
        OperatorTester &operator-=( int ) { assert_die(); return *this; }
        OperatorTester &operator*=( int ) { assert_die(); return *this; }
        OperatorTester &operator/=( int ) { assert_die(); return *this; }
        OperatorTester &operator%=( int ) { assert_die(); return *this; }
        void test() { assert_eq( value, expected ); }
        void set( int v, int e ) { value = v; expected = e; }
    };
    struct TPrI : OperatorTester {
        TPrI &operator++() { ++value; return *this; }
    };
    struct TPoI : OperatorTester {
        TPoI operator++( int ) { auto r = *this; value++; return r; }
    };
    struct TPrD : OperatorTester {
        TPrD &operator--() { --value; return *this; }
    };
    struct TPoD : OperatorTester {
        TPoD operator--( int ) { auto r = *this; value--; return r; }
    };
    struct TPlO : OperatorTester {
        TPlO &operator+=( int v ) { value += v; return *this; }
    };
    struct TMO : OperatorTester {
        TMO &operator-=( int v ) { value -= v; return *this; }
    };
    struct TPoO : OperatorTester {
        TPoO &operator*=( int v ) { value *= v; return *this; }
    };
    struct TSO : OperatorTester {
        TSO &operator/=( int v ) { value /= v; return *this; }
    };
    struct TPrO : OperatorTester {
        TPrO &operator%=( int v ) { value %= v; return *this; }
    };

    template< int N, typename BT, typename L >
    void checkOperator( BT &bt, int v, int e, L l ) {
        auto t = get< N >( bt ).get();
        t.set( v, e );
        get< N >( bt ) = t;
        l( get< N >( bt ) );
        get< N >( bt ).get().test();
    }

#define CHECK( N, bt, v, e, test ) checkOperator< N >( bt, v, e, []( decltype( get< N >( bt ) ) item ) { test; } )

    Test operators() {
        BitTuple<
            BitField< bool, 4 >,
            BitField< TPrI >,// ++v
            BitField< TPoI >,// v++
            BitField< TPrD >,// --v
            BitField< TPoD >,// v--
            BitField< TPlO >,// v+=
            BitField< TMO >,// v-=
            BitField< TPoO >,// v*=
            BitField< TSO >,// v/=
            BitField< TPrO >,// v%=
            BitField< bool, 4 >
        > bt;

        CHECK( 1, bt, 0, 1, ++item );
        CHECK( 2, bt, 0, 1, item++ );
        CHECK( 3, bt, 0, -1, --item );
        CHECK( 4, bt, 0, -1, item-- );
        CHECK( 5, bt, 0, 5, item += 5 );
        CHECK( 6, bt, 0, -5, item -= 5 );
        CHECK( 7, bt, 2, 14, item *= 7 );
        CHECK( 8, bt, 42, 6, item /= 7 );
        CHECK( 9, bt, 42, 9, item %= 11 );
    }
#undef CHECK
};
