#include <rst/star.h>

#include <type_traits>

using namespace abstract::star;

using abstract::Tristate;

template <typename E>
constexpr auto to_underlying(E e) noexcept {
    return static_cast< std::underlying_type_t<E> >( e );
}

template< typename T >
T __star_val_impl() {
    auto val = __star_lift( sizeof( T ) * 8, 0 );
    __lart_stash( to_underlying( val ) );
    return abstract::taint< T >();
}

extern "C" {
    _STAR uint8_t __star_val_i8() { return __star_val_impl< uint8_t >(); }
    _STAR uint16_t __star_val_i16() { return __star_val_impl< uint16_t >(); }
    _STAR uint32_t __star_val_i32() { return __star_val_impl< uint32_t >(); }
    _STAR uint64_t __star_val_i64() { return __star_val_impl< uint64_t >(); }
}

Unit __star_lift( int, int, ... ) { return Unit(); }

#define BINARY( suff, op ) Unit __star_ ## suff( Unit, Unit ) { \
    return Unit();                                              \
}

BINARY( add, Add );
BINARY( sub, Sub );
BINARY( mul, Mul );
BINARY( sdiv, SDiv );
BINARY( udiv, UDiv );
BINARY( urem, URem );
BINARY( srem, SRem );
BINARY( and, And );
BINARY( or, Or );
BINARY( xor, Xor );
BINARY( shl, Shl );
BINARY( lshr, LShr );
BINARY( ashr, AShr );

#define CAST( suff, op ) Unit __star_ ## suff( Unit, int ) { \
    return Unit();                                           \
}

CAST( trunc, Trunc );
CAST( zext, ZExt );
CAST( sext, SExt );
CAST( bitcast, BitCast );

#define ICMP( suff, op ) Unit __star_icmp_ ## suff( Unit, Unit ) { \
    return Unit();                                                 \
}

ICMP( eq, EQ );
ICMP( ne, NE );
ICMP( ugt, UGT );
ICMP( uge, UGE );
ICMP( ule, ULE );
ICMP( ult, ULT );
ICMP( sgt, SGT );
ICMP( sge, SGE );
ICMP( sle, SLE );
ICMP( slt, SLT );

Tristate __star_bool_to_tristate( Unit ) {
    return Tristate::Value::Unknown;
}

Unit __star_assume( Unit, Unit, bool ) {
    return Unit();
}

void __star_freeze( Unit, void* ) {}

Unit __star_thaw( void*, int ) { return Unit(); }

extern "C" void __star_stash( Unit ) { }

extern "C" Unit __star_unstash() { return Unit(); }

extern "C" void __star_cleanup(void) { }
