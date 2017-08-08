#include <abstract/sym.h>
#include <abstract/common.h>
#include <dios.h>

using namespace sym;
using abstract::Tristate;
using abstract::__new;
using abstract::mark;
using abstract::weaken;

template< typename T, typename ... Args >
static Formula *__newf( Args &&...args ) {
    void *ptr = __vm_obj_make( sizeof( T ) );
    new ( ptr ) T( std::forward< Args >( args )... );
    return static_cast< Formula * >( ptr );
}

struct PCFragment {
    PCFragment( Formula *assume, PCFragment *next ) : assume( assume ), next( next ) { }

    Formula *assume = nullptr;
    PCFragment *next = nullptr;
};

struct State {
    int counter = 0;
    PCFragment *pcFragments = nullptr;
};

State state;

Formula **__abstract_sym_alloca( int bitwidth ) {
    return __new< Formula * >( mark( __newf< Variable >( Type{ Type::Int, bitwidth }, state.counter++ ) ) );
}

Formula *__abstract_sym_load( Formula **a, int bitwidth ) {
    if ( bitwidth > 64 )
        UNREACHABLE_F( "Integer too long: %d bits", bitwidth );
    auto *val = *a;
    if ( val->type.bitwidth() > bitwidth ) {
        return __abstract_sym_trunc( val, bitwidth );
    } else if ( val->type.bitwidth() < bitwidth ) {
        UNREACHABLE_F( "Loading of %d bit value from %d bit abstract value is not supported (yet).",
                       bitwidth, val->type.bitwidth() );
    }
    return mark( val );
}

void __abstract_sym_store( Formula *val, Formula **ptr ) {
    *ptr = weaken( val );
}

Formula *__abstract_sym_lift( int64_t val, int bitwidth ) {
    if ( bitwidth > 64 )
        UNREACHABLE_F( "Integer too long: %d bits", bitwidth );
    return mark( __newf< Constant >( Type{ Type::Int, bitwidth }, val ) );
}

#define BINARY( suff, op ) Formula *__abstract_sym_ ## suff( Formula *a, Formula *b ) { \
    return mark( __newf< Binary >( Op::op, a->type, weaken( a ), weaken( b ) ) ); \
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

#define CAST( suff, op ) Formula *__abstract_sym_ ## suff( Formula *a, int bitwidth ) { \
    Type t = a->type; \
    t.bitwidth( bitwidth ); \
    return mark( __newf< Unary >( Op::op, t, weaken( a ) ) ); \
}

CAST( trunc, Trunc );
CAST( zext, ZExt );
CAST( sext, SExt );
CAST( bitcast, BitCast );

#define ICMP( suff, op ) Formula *__abstract_sym_icmp_ ## suff( Formula *a, Formula *b ) { \
    Type i1( Type::Int, 1 ); \
    return mark( __newf< Binary >( Op::op, i1, weaken( a ), weaken( b ) ) ); \
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

Tristate *__abstract_sym_bool_to_tristate( Formula * ) {
    // TODO: pattern matching for trivial cases of True/False
    return __new< Tristate >( Tristate::Unknown );
}

Formula *__abstract_sym_assume( Formula *value, Formula *constraint, bool assume ) {
    Formula *wconstraint = weaken( constraint );
    if ( !assume )
        wconstraint = __newf< Unary >( Op::BoolNot, wconstraint->type, wconstraint );
    state.pcFragments = weaken( __new< PCFragment >( wconstraint, state.pcFragments ) );
    __vm_trace( _VM_T_Alg, state.pcFragments );
    return mark( __newf< Assume >( weaken( value ), wconstraint ) );
}
