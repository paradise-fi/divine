

#include <wibble/test.h> // for assert
#include <divine/toolkit/bitoperations.h>

using namespace divine::bitops;

struct TestBitOperations {

    static_assert( 15 == compiletime::fill( 8 ), "divine::bitops::compiletime::fill is not constexpr" );
    static_assert( 8 == compiletime::MSB( 511 ), "divine::bitops::compiletime::fill is not constexpr");

    template< typename C, typename T >
    void fillTest( C c, T stop ) {
        assert_eq( c( 0 ), T( 0 ) );
        for ( T i = 1; i < stop; ++i ) {
            T x = c( i );

            assert( x >= i );
            assert( x < ( i << 1 ) );
            while( x )
                x >>= 1;
            assert_eq( x, T( 0 ) );
        }
    }

    template< typename T >
    void fillType( T stop ) {
        fillTest( fill< T >, stop );
    }

    template< typename T >
    void fillTypeCT( T stop ) {
        fillTest( compiletime::fill< T >, stop );
    }

    Test fillAll() {
        fillType< unsigned char >( 127 );
        fillType< unsigned short >( 32767 );
        fillType< unsigned int >( 1024*1024 );
    }

    Test fillAllCompileTime() {
        fillTypeCT< unsigned char >( 127 );
        fillTypeCT< unsigned short >( 32767 );
        fillTypeCT< unsigned int >( 1024*1024 );
    }

    template< typename C, typename T >
    void MSBTest( C c, T stop ) {
        assert_eq( c( 1 ), unsigned( 0 ) );
        for ( T i = 2; i < stop; ++i ) { // not defined for zero
            unsigned x = c( i );
            assert( (T(1) << x) <= i );
            assert( (T(2) << x) > i );
        }
    }

    template< typename T >
    void MSBType( T stop ) {
        MSBTest( MSB< T >, stop );
    }

    template< typename T >
    void MSBTypeCT( T stop ) {
        MSBTest( compiletime::MSB< T >, stop );
    }

    Test MSBAll() {
        MSBType< unsigned char >( 127 );
        MSBType< unsigned short >( 32767 );
        MSBType< unsigned int >( 1024 * 1024 );
        MSBType< unsigned long >( 1024 * 1024 );
        MSBType< unsigned long long >( 1024 * 1024 );
    }

    Test MSBAllCompileTime() {
        MSBTypeCT< unsigned char >( 127 );
        MSBTypeCT< unsigned short >( 32767 );
        MSBTypeCT< unsigned int >( 1024 * 1024 );
        MSBTypeCT< unsigned long >( 1024 * 1024 );
        MSBTypeCT< unsigned long long >( 1024 * 1024 );
    }
};

