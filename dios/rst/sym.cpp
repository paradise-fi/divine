#include <rst/sym.h>
#include <rst/domains.h>
#include <rst/common.h>
#include <rst/formula.h>
#include <rst/lart.h>
#include <dios.h>

#include <cstdarg>
#include <type_traits>

using namespace lart::sym;
using abstract::Tristate;
using abstract::__new;
using abstract::mark;
using abstract::weaken;
using abstract::taint;
using abstract::peek_object;
using abstract::poke_object;

extern "C" uint64_t __rst_taint_i64()
{
    return __tainted;
}

template< typename T, typename ... Args >
static Formula *__newf( Args &&...args ) {
    void *ptr = __vm_obj_make( sizeof( T ) );
    new ( ptr ) T( std::forward< Args >( args )... );
    return static_cast< Formula* >( ptr );
}

struct State
{
    int counter = 0;
    Formula *constraints = nullptr;
};

State __sym_state;

extern "C" void __sym_formula_dump()
{
    __vm_trace( _VM_T_Text, "not implemented" );
/*
    Formula *pcf = __sym_state.constraints;
    while ( pcf != NULL )
    {
        TODO trace pcf->binary.left
        pcf = pcf->binary.right;
    }
*/
}

template< typename T, typename Lift >
T __sym_val_impl( Lift lift ) {
    auto *ptr = lift( sizeof( T ) * 8, 0 );
    __lart_stash( reinterpret_cast< uintptr_t >( ptr ) );
    return taint< T >();
}

template< typename T >
T __sym_val_int_impl() {
    return __sym_val_impl< T, decltype( __sym_lift ) >( __sym_lift );
}

template< typename T >
T __sym_val_float_impl() {
    return __sym_val_impl< T, decltype( __sym_lift_float ) >( __sym_lift_float );
}

extern "C" {
    _SYM uint8_t __sym_val_i8() { return __sym_val_int_impl< uint8_t >(); }
    _SYM uint16_t __sym_val_i16() { return __sym_val_int_impl< uint16_t >(); }
    _SYM uint32_t __sym_val_i32() { return __sym_val_int_impl< uint32_t >(); }
    _SYM uint64_t __sym_val_i64() { return __sym_val_int_impl< uint64_t >(); }

    _SYM float __sym_val_float32() { return __sym_val_float_impl< float >(); }
    _SYM double __sym_val_float64() { return __sym_val_float_impl< double >(); }
}

Formula *__sym_lift( int bitwidth, int argc, ... ) {
    if ( bitwidth > 64 )
        _UNREACHABLE_F( "Type too long: %d bits", bitwidth );
    if ( !argc ) {
        return mark( __newf< Variable >( Type{ Type::Int, bitwidth }, __sym_state.counter++ ) );
    }
    if ( argc > 1 )
        _UNREACHABLE_F( "Lifting of more values is not yet supported." );

    va_list args;
    va_start( args, argc );
    return mark( __newf< Constant >( Type{ Type::Int, bitwidth }, va_arg( args, int64_t ) ) );
}

Formula *__sym_lift_float( int bitwidth, int argc, ... ) {
    if ( bitwidth > 64 )
        _UNREACHABLE_F( "Type too long: %d bits", bitwidth );
    if ( !argc ) {
        return mark( __newf< Variable >( Type{ Type::Float, bitwidth }, __sym_state.counter++ ) );
    }
    if ( argc > 1 )
        _UNREACHABLE_F( "Lifting of more values is not yet supported." );

    va_list args;
    va_start( args, argc );
    return mark( __newf< Constant >( Type{ Type::Float, bitwidth }, va_arg( args, float ) ) );

}

#define BINARY( suff, op ) Formula *__sym_ ## suff( Formula *a, Formula *b ) \
{                                                                                     \
    return mark( __newf< Binary >( Op::op, a->type(), weaken( a ), weaken( b ) ) );   \
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

BINARY( fadd, FpAdd )
BINARY( fsub, FpSub )
BINARY( fmul, FpMul )
BINARY( fdiv, FpDiv )
BINARY( frem, FpRem )

#define CAST( suff, op ) Formula *__sym_ ## suff( Formula *a, int bitwidth ) \
{                                                                                     \
    Type t = a->type();                                                               \
    t.bitwidth( bitwidth );                                                           \
    return mark( __newf< Unary >( Op::op, t, weaken( a ) ) );                         \
}

CAST( trunc, Trunc );
CAST( zext, ZExt );
CAST( sext, SExt );
CAST( bitcast, BitCast );

CAST( fptrunc, FPTrunc )
CAST( fpext, FPExt )

#define ICMP( suff, op ) Formula *__sym_icmp_ ## suff( Formula *a, Formula *b ) { \
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

#define FCMP( suff, op ) Formula *__sym_fcmp_ ## suff( Formula *a, Formula *b ) { \
    Type i1( Type::Int, 1 ); \
    return mark( __newf< Binary >( Op::op, i1, weaken( a ), weaken( b ) ) ); \
}

FCMP( fcfalse, FpFalse );
FCMP( oeq, FpOEQ );
FCMP( ogt, FpOGT );
FCMP( oge, FpOGE );
FCMP( olt, FpOLT );
FCMP( ole, FpOLE );
FCMP( one, FpONE );
FCMP( ord, FpORD );
FCMP( uno, FpUNO );
FCMP( ueq, FpUEQ );
FCMP( ugt, FpUGT );
FCMP( uge, FpUGE );
FCMP( ult, FpULT );
FCMP( ule, FpULE );
FCMP( une, FpUNE );
FCMP( fctrue, FpTrue );

__invisible Formula *__sym_ptrtoint( Formula *v )
{
    return v;
}

Tristate __sym_bool_to_tristate( Formula * )
{
    // TODO: pattern matching for trivial cases of True/False
    return Tristate::Value::Unknown;
}

__invisible Formula *__sym_assume( Formula *value, Formula *constraint, bool assume )
{
    Formula *wconstraint = weaken( constraint );
    if ( !assume )
        wconstraint = weaken( __newf< Unary >( Op::BoolNot, wconstraint->type(), wconstraint ) );
    __sym_state.constraints = mark( __newf< Binary >( Op::Constraint, wconstraint->type(), wconstraint,
                                                      weaken( __sym_state.constraints ) ) );
    __vm_trace( _VM_T_Assume, __sym_state.constraints );
    return value;
}

void __sym_freeze( Formula *formula, void *addr ) {
    if ( abstract::tainted( *static_cast< char * >( addr ) ) ) {
        peek_object< Formula >( addr )->refcount_decrement();
    }

    formula->refcount_increment();
    poke_object< Formula >( formula, addr );
}

Formula* __sym_thaw( void *addr, int bw ) {
    Formula *ret = peek_object< Formula >( addr );

    if ( ret->type().bitwidth() < bw ) {
        if ( ret->type().type() == Type::Int )
            return __sym_zext( ret, bw );
        else if ( ret->type().type() == Type::Float )
            return __sym_fpext( ret, bw );
        _UNREACHABLE_F( "Unsupported type for thawing." );
    } else if ( ret->type().bitwidth() > bw ) {
        if ( ret->type().type() == Type::Int )
            return __sym_trunc( ret, bw );
        else if ( ret->type().type() == Type::Float )
            return __sym_fptrunc( ret, bw );
        _UNREACHABLE_F( "Unsupported type for thawing." );
    } else {
        return ret;
    }
}

extern "C" void __sym_cleanup(void) {
    auto *frame = __dios_this_frame()->parent;
    __cleanup_orphan_formulae( frame );
}
}
