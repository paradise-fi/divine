/*
 * (c) 2017 Vladimír Štill <xstill@fi.muni.cz>
 * (c) 2018 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file defines the interface between various lart-provided program
 * transformations and their runtime support code (which resides in dios/pts).
 */

#ifndef _SYS_LART_H_
#define _SYS_LART_H_

#ifdef __divine__
#include <sys/divm.h>
#else
#include <divine/vm/divm.h>
#endif

#if __cplusplus >= 201402L
#include <type_traits>

namespace lart::weakmem
{

    enum class MemoryOrder : uint8_t
    {
        NotAtomic = 0,
        Unordered  = (1 << 0),
        Monotonic  = (1 << 1) | Unordered,
        Acquire    = (1 << 2) | Monotonic,
        Release    = (1 << 3) | Monotonic,
        AcqRel     = Acquire | Release,
        SeqCst     = (1 << 4) | AcqRel,
        AtomicOp   = (1 << 5),
        WeakCAS    = (1 << 6),
    };

    inline MemoryOrder operator|( MemoryOrder a, MemoryOrder b )
    {
        using uint = typename std::underlying_type< MemoryOrder >::type;
        return MemoryOrder( uint( a ) | uint( b ) );
    }

    inline MemoryOrder operator&( MemoryOrder a, MemoryOrder b )
    {
        using uint = typename std::underlying_type< MemoryOrder >::type;
        return MemoryOrder( uint( a ) & uint( b ) );
    }

    inline bool subseteq( MemoryOrder a, MemoryOrder b )
    {
        return a == (a & b);
    }

}

namespace lart::sym
{

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

}

#endif

#ifdef __cplusplus
extern "C" {
#endif

// there are 16 flags reserved for transformations, see <sys/divm.h>
static const uint64_t _LART_CF_RelaxedMemRuntime = _VM_CFB_Transform << 0;

#ifdef __cplusplus
}
#endif

#endif // _SYS_LART_H_
