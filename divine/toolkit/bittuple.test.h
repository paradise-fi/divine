// -*- C++ -*-

#include <wibble/test.h>
#include <divine/toolkit/bittuple.h>

struct TestBitTuple {
    using U10 = BitField< unsigned, 10 >;
    using T10_10 = BitTuple< U10, U10 >;

    Test mask() {
        /* only works on little endian machines ... */
        assert_eq( 0xFF00, ::mask( 8, 8 ) );
        assert_eq( 0xF000, ::mask( 12, 4 ) );
        assert_eq( 0x0F00, ::mask( 8, 4 ) );
        assert_eq( 0b111100, ::mask( 2, 4 ) );
        assert_eq( 0b11100, ::mask( 2, 3 ) );
    }

    Test bitcopy() {
        uint32_t a = 42, b = 11;
        ::bitcopy( BitPointer( &a ), BitPointer( &b ), 32 );
        assert_eq( a, b );
        a = 0xFF00;
        ::bitcopy( BitPointer( &a ), BitPointer( &b, 8 ), 24 );
        assert_eq( b, 0xFF0000 | 42 );
        a = 0;
        ::bitcopy( BitPointer( &b, 8 ), BitPointer( &a ), 24 );
        assert_eq( a, 0xFF00 );
        ::bitcopy( BitPointer( &a, 8 ), BitPointer( &b, 8 ), 8 );

        a = 0x3FF;
        b = 0;
        ::bitcopy( BitPointer( &a, 0 ), BitPointer( &b, 0 ), 10 );
        assert_eq( b, 0x3FF );
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
        assert_eq( a, 5 );
        assert_eq( b, 7 );
    }

    Test big() {
        BitTuple< BitField< uint64_t, 63 >, BitField< uint64_t, 63 > > x;
        assert_eq( x.bitwidth, 126 );
        assert_eq( x.offset< 0 >(), 0 );
        assert_eq( x.offset< 1 >(), 63 );
        get< 0 >( x ).set( (1ll << 62) + 7 );
        assert_eq( get< 0 >( x ), (1ll << 62) + 7 );
        assert_eq( get< 1 >( x ), 0 );
        get< 0 >( x ).set( 0 );
        get< 1 >( x ).set( (1ll << 62) + 7 );
        assert_eq( get< 0 >( x ), 0 );
        assert_eq( get< 1 >( x ), (1ll << 62) + 7 );
        get< 0 >( x ).set( (1ll << 62) + 11 );
        assert_eq( get< 0 >( x ), (1ll << 62) + 11 );
        assert_eq( get< 1 >( x ), (1ll << 62) + 7 );
    }

    Test structure() {
        BitTuple< BitField< std::pair< uint64_t, uint64_t >, 120 >, BitField< uint64_t, 63 > > x;
        auto v = std::make_pair( (1ul << 62) + 7, 33ul );
        assert_eq( x.bitwidth, 183 );
        assert_eq( x.offset< 0 >(), 0 );
        assert_eq( x.offset< 1 >(), 120 );
        get< 1 >( x ).set( 333 );
        assert_eq( get< 1 >( x ), 333 );
        get< 0 >( x ).set( v );
        assert_eq( get< 1 >( x ), 333 );
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
        assert_eq( get< 0 >( a ), 5 );
        assert_eq( get< 1 >( a ), 7 );
        assert_eq( get< 0 >( b ), 13 );
        assert_eq( get< 1 >( b ), 533 );
        assert_eq( get< 2 >( x ), 7 );
    }
};
