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
#include <brick-assert>
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

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace sym {

using VarID = int32_t;

enum class Op : uint16_t
{
    Invalid,

    Variable,
    Constant,

    BitCast, FirstUnary = BitCast,

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

    BoolNot,
    Extract, LastUnary = Extract,

    Add, FirstBinary = Add,
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
    FcOEQ,   // ordered and equal
    FcOGT,   // ordered and greater than
    FcOGE,   // ordered and greater than or equal
    FcOLT,   // ordered and less than
    FcOLE,   // ordered and less than or equal
    FcONE,   // ordered and not equal
    FcORD,   // ordered (no nans)
    FcUEQ,   // unordered or equal
    FcUGT,   // unordered or greater than
    FcUGE,   // unordered or greater than or equal
    FcULT,   // unordered or less than
    FcULE,   // unordered or less than or equal
    FcUNE,   // unordered or not equal
    FcUNO,   // unordered (either nans)
    FcTrue,  // no comparison, always returns true
    Constraint, // behaves as binary logical and

    Concat, LastBinary = Concat,
};


inline bool isUnary( Op x )
{
    return x >= Op::FirstUnary && x <= Op::LastUnary;
}

inline bool isBinary( Op x )
{
    return x >= Op::FirstBinary && x <= Op::LastBinary;
}

std::string toString( Op x );

struct Type
{
    enum T : uint8_t
    {
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
    Unary_( Op op, Type t, unsigned short from, unsigned short to, Formula child ) :
        op( op ), type( t ), from( from ), to( to ), child( child )
    { }

    Op op;          // 16B
    Type type;      // 16B
    unsigned short from, to; // 32B, only for Extract
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

union Formula;

#ifdef __divine__
using Binary = Binary_< Formula * >;
using Unary = Unary_< Formula * >;
#else
using Unary = Unary_< divine::vm::HeapPointer >;
using Binary = Binary_< divine::vm::HeapPointer >;
#endif

union Formula
{
    Formula() : hdr{ Op::Invalid, Type() } { }

    struct
    {
        Op op;
        Type type;
    } hdr;

    Op op() const { return hdr.op; }
    Type type() const { return hdr.type; }

    Variable var;
    Constant con;
    Unary unary;
    Binary binary;
};

std::string toString( const Formula *f );
inline std::string toString( const Formula &f ) {
    return toString( &f );
}

} // namespace sym

#endif

#endif // __BVEC_H__
