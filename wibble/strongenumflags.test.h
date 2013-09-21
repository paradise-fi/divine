#if __cplusplus >= 201103L

#include <wibble/strongenumflags.h>
#include <wibble/test.h>

using namespace wibble;

enum class A : unsigned char  { X = 1, Y = 2, Z = 4 };
enum class B : unsigned short { X = 1, Y = 2, Z = 4 };
enum class C : unsigned       { X = 1, Y = 2, Z = 4 };
enum class D : unsigned long  { X = 1, Y = 2, Z = 4 };

struct TestStrongEnumFlags {
    template< typename Enum >
    void testEnum() {
        StrongEnumFlags< Enum > e1;
        StrongEnumFlags< Enum > e2( Enum::X );

        assert( !e1 );
        assert( e2 );

        assert( e1 | e2 );
        assert( Enum::X | Enum::Y );
        assert( e2 | Enum::Z );
        assert( e2.has( Enum::X ) );

        assert( e2 & Enum::X );
        assert( !( Enum::X & Enum::Y ) );

        assert( Enum::X | Enum::Y | Enum::Z );
        assert( !( Enum::X & Enum::Y & Enum::Z ) );
        assert( ( Enum::X | Enum::Y | Enum::Z ) & Enum::X );
    }

    // we don't want to break classical enums and ints by out operators
    Test regression() {
        enum Classic { C_X = 1, C_Y = 2, C_Z = 4 };

        assert( C_X | C_Y | C_Z );
        assert( 1 | 2 | 4 );
        assert( C_X & 1 );
    }

    Test enum_uchar() { testEnum< A >(); }
    Test enum_ushort() { testEnum< B >(); }
    Test enum_uint() { testEnum< C >(); }
    Test enum_ulong() { testEnum< D >(); }
};

#endif
