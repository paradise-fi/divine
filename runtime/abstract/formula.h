#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#ifdef __cplusplus
#include <utility>
#include <string>
#include <array>
#include <iostream>
#endif

#ifdef __divine__
#include <dios.h>
#define UNREACHABLE_F(...) do { \
        __dios_trace_f( __VA_ARGS__ ); \
        __dios_fault( _VM_F_Assert, "unreachable called" ); \
        __builtin_unreachable(); \
    } while ( false )
#else
#include <bricks-assert>
#endif

#ifndef __BVEC_H__
#define __BVEC_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#if 0
/*
 * Create a new formula node given the opration and its arguments
 * in can be called in the following ways:
 * - __sym_mk_op( Op::Variable, type, bitwidth, int varId )
 * - __sym_mk_op( Op::Constant, type, bitwidth, int64_t value )
 * - __sym_mk_op( any_unary_op, type, bitwidth, Formula *child )
 * - __sym_mk_op( any_binary_op, type, bitwidth, Formula *left_child, Formula *right_child )
 */
void *__sym_mk_op( int op, int type, int bitwidth ... );
#endif
char *__sym_formula_to_string( void *root );

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace sym {

using VarID = int32_t;

enum class Op : uint16_t {
    Invalid,

    Variable,
    Constant,

    FirstUnary,
    BitCast = FirstUnary,

    SExt,
    ZExt,
    Trunc,

    IntToPtr,
    PtrToInt,

    FPExt,
    FPTrunc,
    FPToSInt,
    FPToUInt,
    SIntToFP,
    UIntToFP,
    LastUnary = UIntToFP,

    FirstBinary,
    Add = FirstBinary,
    Sub,
    Mul,
    UDiv,
    SDiv,
    URem,
    SRem,

    FAdd,
    FSub,
    FMul,
    FDiv,
    FRem,

    Shl,
    LShr,
    AShr,
    And,
    Or,
    Xor,

    // Icmp
    EQ,
    NE,
    UGT,
    UGE,
    ULT,
    ULE,
    SGT,
    SGE,
    SLT,
    SLE,

    // Fcmp
    FcFalse, // no comparison, always returns false
    FcOEQ, // ordered and equal
    FcOGT, // ordered and greater than
    FcOGE, // ordered and greater than or equal
    FcOLT, // ordered and less than
    FcOLE, // ordered and less than or equal
    FcONE, // ordered and not equal
    FcORD, // ordered (no nans)
    FcUEQ, // unordered or equal
    FcUGT, // unordered or greater than
    FcUGE, // unordered or greater than or equal
    FcULT, // unordered or less than
    FcULE, // unordered or less than or equal
    FcUNE, // unordered or not equal
    FcUNO, // unordered (either nans)
    FcTrue, // no comparison, always returns true

    LastBinary = FcTrue,

    Assume
};


inline bool isUnary( Op x ) {
    return x >= Op::FirstUnary && x <= Op::LastUnary;
}

inline bool isBinary( Op x ) {
    return x >= Op::FirstBinary && x <= Op::LastBinary;
}

std::string toString( Op x );

struct Type {
    enum T : uint8_t {
        Int,
        Float
    };

    static const int bwmask = 0x7fff;

    Type() = default;
    Type( T g, int bitwidth ) : _data( (g << 15) | bitwidth ) { }

    T type() { return T( _data >> 15 ); }
    int bitwidth() { return _data & bwmask; }
    void bitwidth( int x ) {
        _data &= ~bwmask;
        _data |= bwmask & x;
    }

    uint16_t _data;
};

std::string toString( Type );

static_assert( sizeof( VarID ) == 4, "" );
static_assert( sizeof( Type ) == 2, "" );

struct Variable {
    Variable( Type type, VarID id ) : op( Op::Variable ), type( type ), id( id ) { }

    Op op;     // 16B
    Type type; // 16B
    VarID id;  // 32B
};

static_assert( sizeof( Variable ) == 8, "" );

union ValueU {
//    ValueT raw;
    uint8_t i8;
    uint16_t i16;
    uint32_t i32;
    uint64_t i64;
    float fp32;
    double fp64;
//    long double fp80;
};

struct Constant {
    Constant( Type type, uint64_t value ) :
        op( Op::Constant ), type( type ), value( value )
    { }

    Op op;     // 16B
    Type type; // 16B
    uint64_t value;
//    std::array< uint32_t, 3 > value; // 96B - enough to hold long double
};

static_assert( sizeof( Constant ) == 16, "" );

template< typename Formula >
struct Unary_ {
    Unary_( Op op, Type t, Formula child ) : op( op ), type( t ), child( child ) { }

    Op op;          // 16B
    Type type;      // 16B
    Formula child;  // 64B
};

static_assert( sizeof( Unary_< void * > ) == 16, "" );

template< typename Formula >
struct Binary_ {
    Binary_( Op op, Type t, Formula left, Formula right ) :
        op( op ), type( t ), left( left ), right( right )
    { }

    Op op;          // 16B
    Type type;      // 16B
    Formula left;   // 64B
    Formula right;  // 64B
};

static_assert( sizeof( Binary_< void * > ) == 24, "" );

template< typename Formula >
struct Assume_ {

    Assume_( Formula value, Formula constraint ) :
        op( Op::Assume ), type( value->type ), value( value ), constraint( constraint )
    { }

    Op op;
    Type type;
    Formula value;
    Formula constraint;
};

union Formula;

#ifdef __divine__
using Binary = Binary_< Formula * >;
using Unary = Unary_< Formula * >;
using Assume = Assume_< Formula * >;
#else
using Unary = Unary_< std::unique_ptr< Formula > >;
using Binary = Binary_< std::unique_ptr< Formula > >;
using Assume = Assume_< std::unique_ptr< Formula > >;
#endif

union Formula {
    Formula() : op( Op::Invalid ) { }

    struct {
        Op op;
        Type type;
    };
    Variable var;
    Constant con;
    Unary unary;
    Binary binary;
    Assume assume;
};

std::string toString( const Formula *f );
inline std::string toString( const Formula &f ) {
    return toString( &f );
}

template< typename Read,
          typename Variable, typename Constant, typename Unary, typename Binary, typename Assume >
struct Walker {

    Walker( Read read, Variable var, Constant con, Unary unary, Binary binary, Assume assume ) :
        read( read ), var( var ), con( con ), unary( unary ), binary( binary ), assume( assume )
    { }

    template< typename T >
    auto walk( T formula ) {
        auto r = read( formula );
        Op op = r.template read< Op >();
        auto t = r.template read< Type >();

        if ( op == Op::Variable ) {
            auto v = r.template read< VarID >();
            return var( t, v );
        }
        if ( op == Op::Constant ) {
            r.shift( 4 );
            ValueU val;
            val.i64 = r.template read< uint64_t >();
            assert( t.type() == Type::Int );
            if ( t.bitwidth() <= 8 )
                return con( t, val.i8 );
            if ( t.bitwidth() <= 16 )
                return con( t, val.i16 );
            if ( t.bitwidth() <= 32 )
                return con( t, val.i32 );
            if ( t.bitwidth() <= 64 )
                return con( t, val.i64 );
            UNREACHABLE_F( "Integer too long: %d bits", t.bitwidth() );
        }
        if ( isUnary( op ) ) {
            r.shift( 4 );
            auto child = walk( r.template read< T >() );
            return unary( op, t, child );
        }
        if ( isBinary( op ) ) {
            r.shift( 4 );
            auto left = walk( r.template read< T >() );
            auto right = walk( r.template read< T >() );
            return binary( op, t, left, right );
        }
        if ( op == Op::Assume ) {
            r.shift( 4 );
            auto val = walk( r.template read< T >() );
            auto constraint = walk( r.template read< T >() );
            return assume( val, constraint );
        }
        UNREACHABLE_F( "Unknow operation %d", op );
    }

    Read read;
    Variable var;
    Constant con;
    Unary unary;
    Binary binary;
    Assume assume;
};

template< typename RawRead >
struct Reader {

    explicit Reader( RawRead rr ) : raw( rr ) { }

    struct R {

        R( const char *ptr ) : ptr( ptr ) { }

        template< typename T >
        T read() {
            union {
                T val;
                char arr[ sizeof( T ) ];
            };
            std::copy( ptr, ptr + sizeof( T ), arr );
            ptr += sizeof( T );
            return val;
        }

        void shift( size_t s ) {
            ptr += s;
        }

        const char *ptr;
    };

    template< typename F >
    R operator()( F formula ) {
        return R( raw( formula ) );
    }

    RawRead raw;
};

template< typename RawRead >
Reader< RawRead > reader( RawRead &&r ) {
    return Reader< RawRead >{ r };
}

template< typename T, typename Read,
          typename Variable, typename Constant, typename Unary, typename Binary, typename Assume >
auto traverseFormula( T formula, Read read, Variable var, Constant con, Unary un, Binary bin, Assume assume )
{
    Walker< Read, Variable, Constant, Unary, Binary, Assume > walker( read, var, con, un, bin, assume );
    return walker.walk( formula );
}

inline const char *vptrToCptr( const void *v ) { return static_cast< const char * >( v ); }

template< typename T,
          typename Variable, typename Constant, typename Unary, typename Binary, typename Assume >
auto traverseNativeFormula( T formula, Variable var, Constant con, Unary un, Binary bin, Assume assume ) {
    return traverseFormula( formula, reader( vptrToCptr ), var, con, un, bin, assume );
}

} // namespace sym

#endif

#endif // __BVEC_H__
