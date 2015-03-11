// -*- C++ -*- (c) 2012-2014 Petr Rockai

#define NO_RTTI

#include <divine/llvm/machine.h>
#include <divine/llvm/program.h>

#include <divine/toolkit/list.h>

#include <brick-data.h>

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#include <llvm/Support/GetElementPtrTypeIterator.h>
#include <llvm/Support/CallSite.h>
#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Target/TargetData.h>
#else
  #include <llvm/IR/DataLayout.h>
  #define TargetData DataLayout
#endif

#include <algorithm>
#include <cmath>
#include <type_traits>

#ifdef _WIN32
#include <float.h>
#define isnan _isnan
#endif

#ifndef DIVINE_LLVM_EXECUTION_H
#define DIVINE_LLVM_EXECUTION_H

namespace llvm {
template<typename T> class generic_gep_type_iterator;
typedef generic_gep_type_iterator<User::const_op_iterator> gep_type_iterator;
}

namespace divine {
namespace llvm {

using ::llvm::dyn_cast;
using ::llvm::cast;
using ::llvm::isa;
using ::llvm::ICmpInst;
using ::llvm::FCmpInst;
using ::llvm::AtomicRMWInst;
namespace Intrinsic = ::llvm::Intrinsic;
using ::llvm::CallSite;
using ::llvm::Type;
using brick::types::Unit;

template< int, int > struct Eq;
template< int i > struct Eq< i, i > { typedef int Yes; };

template< typename As, typename A, typename B >
list::Cons< As *, B > consPtr( A *a, B b ) {
    return list::Cons< As *, B >( reinterpret_cast< As * >( a ), b );
}

template< typename X >
struct UnPtr {};
template< typename X >
struct UnPtr< X * > { typedef X T; };

template< int I, typename Cons >
auto deconsptr( Cons c ) -> typename UnPtr< typename list::ConsAt< I >::template T< Cons > >::T &
{
    return *c.template get< I >();
}

#define MATCH(l, ...) template< typename F, typename X, \
        typename = typename Eq< l, X::length >::Yes > \
    auto match( F &f, X x ) -> decltype( f( __VA_ARGS__ ) ) \
    { static_cast< void >( x ); return f( __VA_ARGS__ ); }

MATCH( 0, /**/ )
MATCH( 1, deconsptr< 0 >( x ) )
MATCH( 2, deconsptr< 1 >( x ), deconsptr< 0 >( x ) )
MATCH( 3, deconsptr< 2 >( x ), deconsptr< 1 >( x ), deconsptr< 0 >( x ) )
MATCH( 4, deconsptr< 3 >( x ), deconsptr< 2 >( x ), deconsptr< 1 >( x ), deconsptr< 0 >( x ) )

#undef MATCH

template< typename T >
auto rem( brick::types::Preferred, T a, T b )
    -> decltype( a % b )
{
    return a % b;
}

template< typename T >
auto rem( brick::types::NotPreferred, T a, T b )
    -> decltype( std::fmod( a, b ) )
{
    return std::fmod( a, b );
}

/* Dummy implementation of a ControlContext, useful for Evaluator for
 * control-flow-free snippets (like ConstantExpr). */
struct ControlContext {
    bool jumped;
    Choice choice;
    void enter( int ) { ASSERT_UNREACHABLE( "" ); }
    void leave() { ASSERT_UNREACHABLE( "" ); }
    machine::Frame &frame( int /*depth*/ = 0 ) { ASSERT_UNREACHABLE( "" ); }
    machine::Flags &flags() { ASSERT_UNREACHABLE( "" ); }
    void problem( Problem::What, Pointer = Pointer() ) { ASSERT_UNREACHABLE( "" ); }
    PC &pc() { ASSERT_UNREACHABLE( "" ); }
    int new_thread( PC, Maybe< Pointer >, MemoryFlag ) { ASSERT_UNREACHABLE( "" ); }
    int stackDepth() { ASSERT_UNREACHABLE( "" ); }
    int threadId() { ASSERT_UNREACHABLE( "" ); }
    int threadCount() { ASSERT_UNREACHABLE( "" ); }
    void switch_thread( int ) { ASSERT_UNREACHABLE( "" ); }
    void dump() {}
};

template< typename X >
struct Dummy {
    static X &v() { ASSERT_UNREACHABLE( "" ); }
};

/*
 * A relatively efficient evaluator for the LLVM instruction set. Current
 * semantics are derived from C++ semantics, which is not always entirely
 * correct. TBD.
 *
 * EvalContext provides access to the register file and memory. It needs to
 * provide at least TargetData, dereference( Value ), dereference( Pointer )
 * and malloc( int ).
 *
 * BEWARE. The non-const references in argument lists of Implementations are
 * necessary to restrict matching to exact same types, since const references
 * and lvalues allow conversions through temporaries to happen. This is
 * undesirable. Please watch out to not modify right-hand values accidentally.
 */
template < typename EvalContext, typename ControlContext >
struct Evaluator
{
    ProgramInfo &info;
    EvalContext &econtext;
    ControlContext &ccontext;

    Evaluator( ProgramInfo &i, EvalContext &e, ControlContext &c )
        : info( i ), econtext( e ), ccontext( c )
    {}

    bool is_signed;

    typedef ::llvm::Instruction LLVMInst;
    ProgramInfo::Instruction *_instruction;
    ProgramInfo::Instruction &instruction() { return *_instruction; }


    using ValueRefs = brick::data::SmallVector< ValueRef, 4 >;
    using MemoryFlags = brick::data::SmallVector< MemoryFlag, 4 >;

    ValueRefs values, result;
    brick::data::SmallVector< MemoryFlags, 4 > flags;

    struct Implementation {
        typedef Unit T;
        Evaluator< EvalContext, ControlContext > *_evaluator;
        Evaluator< EvalContext, ControlContext > &evaluator() { return *_evaluator; }
        ProgramInfo::Instruction i() { return _evaluator->instruction(); }
        EvalContext &econtext() { return _evaluator->econtext; }
        ControlContext &ccontext() { return _evaluator->ccontext; }
        ::llvm::TargetData &TD() { return econtext().TD; }
        MemoryFlag resultFlag( MemoryFlags ) { return MemoryFlag::Data; }
    };

    template< typename X >
    char *dereference( X x ) { return econtext.dereference( x ); }

    template< typename... X >
    static Unit declcheck( X... ) { return Unit(); }

    std::vector< int > pointerId( bool nonempty = false ) {
        auto r = econtext.pointerId( cast< ::llvm::Instruction >( instruction().op ) );
        if ( nonempty && !r.size() )
            return std::vector< int >{ 0 };
        return r;
    }

    void checkPointsTo() { // TODO: check aa_use as well
        auto &r = instruction().result();

        if ( instruction().result().type == ProgramInfo::Value::Void )
            return;

        auto mflag = econtext.memoryflag( r );
        if ( !mflag.valid() || mflag.get() != MemoryFlag::HeapPointer )
            return; /* nothing to do */

        auto p = *reinterpret_cast< Pointer * >( econtext.dereference( r ) );
        if ( p.null() )
            return;

        int actual = econtext.pointerId( p );
        if ( !actual )
            return;

        auto expected = pointerId();
        for ( auto i: expected )
            if ( i == actual )
                return;

        ccontext.problem( Problem::PointsToViolated, p );
    }

    /******** Arithmetic & comparisons *******/

    struct BinaryOperator : Implementation {
        MemoryFlag resultFlag( MemoryFlags x )
        {
            if ( x[1] == MemoryFlag::Uninitialised || x[2] == MemoryFlag::Uninitialised )
                return MemoryFlag::Uninitialised;
            if ( x[1] == MemoryFlag::HeapPointer || x[2] == MemoryFlag::HeapPointer )
                return MemoryFlag::HeapPointer;
            return MemoryFlag::Data;
        }
    };

    struct Arithmetic : BinaryOperator {
        static const int arity = 3;
        template< typename X = int >
        auto operator()( X &r = Dummy< X >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> decltype( declcheck( a + b, rem( brick::types::Preferred(), a, b ) ) )
        {
            switch( this->i().opcode ) {
                case LLVMInst::FAdd:
                case LLVMInst::Add: r = a + b; return Unit();
                case LLVMInst::FSub:
                case LLVMInst::Sub: r = a - b; return Unit();
                case LLVMInst::FMul:
                case LLVMInst::Mul: r = a * b; return Unit();
                case LLVMInst::FDiv:
                case LLVMInst::SDiv:
                case LLVMInst::UDiv:
                    if ( !b )
                        this->ccontext().problem( Problem::DivisionByZero );
                    r = b ? (a / b) : 0;
                    return Unit();
                case LLVMInst::FRem:
                case LLVMInst::URem:
                case LLVMInst::SRem:
                    if ( !b )
                        this->ccontext().problem( Problem::DivisionByZero );
                    r = b ? rem( brick::types::Preferred(), a, b ) : 0;
                    return Unit();
                default:
                    ASSERT_UNREACHABLE_F( "invalid arithmetic opcode %d", this->i().opcode );
            }
        }
    };

    struct ArithmeticWithOverflow : BinaryOperator {
        static const int arity = 4;
        int operation;

        ArithmeticWithOverflow( int op ) : operation( op ) {}

        template< typename X = int, typename Y = uint8_t >
        auto operator()( X &r = Dummy< X >::v(),
                         Y &overflow = Dummy< Y >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> decltype( declcheck( a + b, overflow = true ) )
        {
            overflow = false;

            switch ( operation ) {
                case Intrinsic::umul_with_overflow:
                    ASSERT( !this->evaluator().is_signed );
                    r = a * b;
                    if ( a > 0 && b > 0 && ( r < a || r < b ) )
                        overflow = true;
                    return Unit();
                case Intrinsic::uadd_with_overflow:
                    ASSERT( !this->evaluator().is_signed );
                    r = a + b;
                    if ( r < a || r < b )
                        overflow = true;
                    return Unit();
                case Intrinsic::usub_with_overflow:
                    if ( a < b )
                        overflow = true;
                    r = a - b;
                    return Unit();
                case Intrinsic::smul_with_overflow:
                case Intrinsic::sadd_with_overflow:
                case Intrinsic::ssub_with_overflow:
                    ASSERT( this->evaluator().is_signed );
                default:
                    ASSERT_UNREACHABLE_F( "invalid .with.overflow operation %d", operation );
            }
        }
    };

    struct Bitwise : BinaryOperator {
        static const int arity = 3;
        template< typename X = int >
        auto operator()( X &r = Dummy< X >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> decltype( declcheck( a | b ) )
        {
            switch( this->i().opcode ) {
                case LLVMInst::And:  r = a & b; return Unit();
                case LLVMInst::Or:   r = a | b; return Unit();
                case LLVMInst::Xor:  r = a ^ b; return Unit();
                case LLVMInst::Shl:  r = a << b; return Unit();
                case LLVMInst::AShr:  // XXX?
                case LLVMInst::LShr:  r = a >> b; return Unit();
                default:
                    ASSERT_UNREACHABLE_F( "invalid bitwise opcode %d", this->i().opcode );
            }
        }
    };

    struct Select : Implementation {
        int _selected;
        static const int arity = 4;
        template< typename R = int, typename C = int >
        auto operator()( R &r = Dummy< R >::v(),
                         C &a = Dummy< C >::v(),
                         R &b = Dummy< R >::v(),
                         R &c = Dummy< R >::v() )
            -> decltype( declcheck( r = a ? b : c ) )
        {
            _selected = a ? 1 : 2;
            r = a ? b : c;
            return Unit();
        }
        MemoryFlag resulFlag( MemoryFlags x ) { return x[ _selected ]; }
    };

    struct ICmp : Implementation {
        static const int arity = 3;
        template< typename R = uint8_t, typename X = int >
        auto operator()( R &r = Dummy< R >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> typename std::enable_if< sizeof( R ) == 1, decltype( declcheck( r = a < b ) ) >::type
        {
            auto p = dyn_cast< ICmpInst >( this->i().op )->getPredicate();
            switch ( p ) {
                case ICmpInst::ICMP_EQ:  r = a == b; return Unit();
                case ICmpInst::ICMP_NE:  r = a != b; return Unit();
                case ICmpInst::ICMP_ULT:
                case ICmpInst::ICMP_SLT: r = a < b; return Unit();
                case ICmpInst::ICMP_UGT:
                case ICmpInst::ICMP_SGT: r = a > b; return Unit();
                case ICmpInst::ICMP_ULE:
                case ICmpInst::ICMP_SLE: r = a <= b; return Unit();
                case ICmpInst::ICMP_UGE:
                case ICmpInst::ICMP_SGE: r = a >= b; return Unit();
                default: ASSERT_UNREACHABLE_F( "unexpected icmp op %d", p );
            }
        }
    };

    struct FCmp : Implementation {
        static const int arity = 3;
        template< typename R = uint8_t, typename X = int >
        auto operator()( R &r = Dummy< R >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> typename std::enable_if< sizeof( R ) == 1,
                                        decltype( declcheck( r = isnan( a ) && isnan( b ) ) ) >::type
        {
            auto p = dyn_cast< FCmpInst >( this->i().op )->getPredicate();
            switch ( p ) {
                case FCmpInst::FCMP_FALSE: r = false; return Unit();
                case FCmpInst::FCMP_TRUE:  r = true;  return Unit();
                case FCmpInst::FCMP_ORD:   r = !isnan( a ) && !isnan( b ); return Unit();
                case FCmpInst::FCMP_UNO:   r = isnan( a ) || isnan( b );   return Unit();

                case FCmpInst::FCMP_UEQ:
                case FCmpInst::FCMP_UNE:
                case FCmpInst::FCMP_UGE:
                case FCmpInst::FCMP_ULE:
                case FCmpInst::FCMP_ULT:
                case FCmpInst::FCMP_UGT:
                    if ( isnan( a ) || isnan( b ) ) {
                        r = true;
                        return Unit();
                    }
                    break;
                case FCmpInst::FCMP_OEQ:
                case FCmpInst::FCMP_ONE:
                case FCmpInst::FCMP_OGE:
                case FCmpInst::FCMP_OLE:
                case FCmpInst::FCMP_OLT:
                case FCmpInst::FCMP_OGT:
                    if ( isnan( a ) || isnan( b ) ) {
                        r = false;
                        return Unit();
                    }
                    break;
                default: ASSERT_UNREACHABLE_F( "unexpected fcmp op %d", p );
            }

            switch ( p ) {
                case FCmpInst::FCMP_OEQ:
                case FCmpInst::FCMP_UEQ: r = a == b; return Unit();
                case FCmpInst::FCMP_ONE:
                case FCmpInst::FCMP_UNE: r = a != b; return Unit();

                case FCmpInst::FCMP_OLT:
                case FCmpInst::FCMP_ULT: r = a < b; return Unit();

                case FCmpInst::FCMP_OGT:
                case FCmpInst::FCMP_UGT: r = a > b; return Unit();

                case FCmpInst::FCMP_OLE:
                case FCmpInst::FCMP_ULE: r = a <= b; return Unit();

                case FCmpInst::FCMP_OGE:
                case FCmpInst::FCMP_UGE: r = a >= b; return Unit();
                default: ASSERT_UNREACHABLE_F( "unexpected fcmp op %d", p );
            }
        }
    };

    /******** Register access & conversion *******/

    struct Convert : Implementation {
        static const int arity = 2;
        template< typename L = int, typename R = L >
        auto operator()( L &l = Dummy< L >::v(),
                         R &r = Dummy< R >::v() )
            -> decltype( declcheck( l = L( r ) ) )
        {
            l = L( r );
            return Unit();
        }

        MemoryFlag resultFlag( MemoryFlags x ) { return x[1]; }
    };

    void implement_bitcast() {
        auto r = memcopy( instruction().operand( 0 ), instruction().result(), instruction().result().width );
        ASSERT_EQ( r, Problem::NoProblem );
        static_cast< void >( r );
    }

    template< typename _T >
    struct Get : Implementation {
        static const int arity = 1;
        typedef _T T;

        template< typename X = T >
        auto operator()( X &l = Dummy< X >::v() ) -> decltype( static_cast< T >( l ) )
        {
            return static_cast< T >( l );
        }

        MemoryFlag resultFlag( MemoryFlags x ) { return x[0]; }
    };

    template< typename _T >
    struct Set : Implementation {
        typedef _T Arg;
        Arg v;
        MemoryFlag _flag;
        static const int arity = 1;

        template< typename X = Arg >
        auto operator()( X &r = Dummy< X >::v() )
            -> decltype( declcheck( static_cast< X >( v ) ) )
        {
            r = static_cast< X >( v );
            return Unit();
        }

        MemoryFlag resultFlag( MemoryFlags ) { return _flag; }

        Set( Arg v, MemoryFlag f = MemoryFlag::Data ) : v( v ), _flag( f ) {}
    };

    typedef Get< bool > IsTrue;
    typedef Get< int > GetInt;
    typedef Set< int > SetInt;

    /******** Memory access & conversion ********/

    using AtOffset = std::pair< int, Type * >;

    AtOffset compositeOffset( Type *t ) {
        return std::make_pair( 0, t );
    }

    template< typename... Args >
    AtOffset compositeOffset( Type *t, int index, Args... indices )
    {
        int offset;

        if (::llvm::StructType *STy = dyn_cast< ::llvm::StructType >( t )) {
            const ::llvm::StructLayout *SLO = econtext.TD.getStructLayout(STy);
            offset = SLO->getElementOffset( index );
        } else {
            const ::llvm::SequentialType *ST = cast< ::llvm::SequentialType >( t );
            offset = index * econtext.TD.getTypeAllocSize( ST->getElementType() );
        }

        auto r = compositeOffset(
            cast< ::llvm::CompositeType >( t )->getTypeAtIndex( index ), indices... );
        return std::make_pair( r.first + offset, r.second );
    }

    int compositeOffsetFromInsn( Type *t, int current, int end )
    {
        if ( current == end )
            return 0;

        int index = withValues( GetInt(), instruction().operand( current ) );
        auto r = compositeOffset( t, index );

        return r.first + compositeOffsetFromInsn( r.second, current + 1, end );
    }

    struct GetElement : Implementation {
        static const int arity = 2;
        Unit operator()( Pointer &r = Dummy< Pointer >::v(),
                         Pointer &p = Dummy< Pointer >::v() )
        {
            r = p + this->evaluator().compositeOffsetFromInsn(
                this->i().op->getOperand(0)->getType(), 1, this->i().values.size() - 1 );
            return Unit();
        }
        MemoryFlag resultFlag( MemoryFlags x ) {
            ASSERT_LEQ( 2U, x.size() );
            return x[1];
        }
    };

    void implement_store() {
        Pointer to = withValues( Get< Pointer >(), instruction().operand( 1 ) );
        if ( auto r = memcopy( instruction().operand( 0 ), to, instruction().operand( 0 ).width ) )
            ccontext.problem( r );
    }

    void implement_load() {
        Pointer from = withValues( Get< Pointer >(), instruction().operand( 0 ) );
        if ( auto r = memcopy( from, instruction().result(), instruction().result().width ) )
            ccontext.problem( r );
    }

    template < typename From, typename To, typename FromC, typename ToC >
    Problem::What memcopy( From f, To t, int bytes, FromC &fromc, ToC &toc )
    {
        return llvm::memcopy( f, t, bytes, fromc, toc );
    }

    template < typename From, typename To >
    Problem::What memcopy( From f, To t, int bytes ) {
        return memcopy( f, t, bytes, econtext, econtext );
    }

    struct Memcpy : Implementation {
        static const int arity = 4;
        template< typename I = int >
        auto operator()( Pointer &ret = Dummy< Pointer >::v(),
                         Pointer &dest = Dummy< Pointer >::v(),
                         Pointer &src = Dummy< Pointer >::v(),
                         I &nmemb = Dummy< I >::v() )
            -> decltype( declcheck( memcpy( Dummy< void * >::v(), Dummy< void * >::v(), nmemb ) ) )
        {
            if ( auto problem = this->evaluator().memcopy( src, dest, nmemb ) )
                this->ccontext().problem( problem );

            ret = dest;
            return Unit();
        }

        /* copy status from dest */
        MemoryFlag resultFlag( MemoryFlags x ) { return x[1]; }
    };

    struct Switch : Implementation {
        static const int arity = 1;
        template< typename X = int >
        Unit operator()( X &condition = Dummy< X >::v() )
        {
            for ( int o = 2; o < int( this->i().values.size() ) - 1; o += 2 )
            {
                X &v = *reinterpret_cast< X * >(
                    this->evaluator().dereference( this->i().operand( o ) ) );
                if ( v == condition ) {
                    this->evaluator().jumpTo( this->i().operand( o + 1 ) );
                    return Unit();
                }
            }
            this->evaluator().jumpTo( this->i().operand( 1 ) );
            return Unit();
        }

        MemoryFlag resultFlag( MemoryFlags x ) { return x[0]; }
    };

    struct CmpXchg : Implementation {
        static const int arity = 4;
        template< typename X = int >
        auto operator()( X &result = Dummy< X >::v(),
                         Pointer &p = Dummy< Pointer >::v(),
                         X &expected = Dummy< X >::v(),
                         X &changed = Dummy< X >::v() )
            -> decltype( declcheck( expected == changed ) )
        {
            X &current = *reinterpret_cast< X * >(
                this->evaluator().dereference( p ) );

            result = current;

            if ( current == expected )
                current = changed;

            return Unit();
        }

        MemoryFlag resultFlag( MemoryFlags ) {
            return MemoryFlag::Data; /* nothing */
        }
    };

    struct AtomicRMW : Implementation {
        static const int arity = 3;
        template< typename X = int >
        auto operator()( X &result = Dummy< X >::v(),
                         Pointer &p = Dummy< Pointer >::v(),
                         X &x = Dummy< X >::v() )
            -> decltype( declcheck( x + x, x ^ x ) )
        {
            X &v = *reinterpret_cast< X * >(
                this->evaluator().dereference( p ) );

            result = v;

            switch (dyn_cast< AtomicRMWInst >( this->i().op )->getOperation()) {
                case AtomicRMWInst::Xchg: v = x;     return Unit();
                case AtomicRMWInst::Add:  v =  v + x; return Unit();
                case AtomicRMWInst::Sub:  v =  v - x; return Unit();
                case AtomicRMWInst::And:  v =  v & x; return Unit();
                case AtomicRMWInst::Nand: v = ~v & x; return Unit();
                case AtomicRMWInst::Or:   v =  v | x; return Unit();
                case AtomicRMWInst::Xor:  v =  v ^ x; return Unit();
                case AtomicRMWInst::Max:
                case AtomicRMWInst::UMax: v = std::max( v, x ); return Unit();
                case AtomicRMWInst::UMin:
                case AtomicRMWInst::Min:  v = std::min( v, x ); return Unit();
                case AtomicRMWInst::BAD_BINOP: ASSERT_UNREACHABLE_F( "bad binop in atomicrmw" );
            }

            return Unit();
        }

        MemoryFlag resultFlag( MemoryFlags ) {
            return MemoryFlag::Data; /* nothing */
        }
    };

    void implement_alloca() {
        ::llvm::AllocaInst *I = cast< ::llvm::AllocaInst >( instruction().op );
        Type *ty = I->getAllocatedType();

        int count = withValues( GetInt(), instruction().operand( 0 ) );
        int size = econtext.TD.getTypeAllocSize(ty); /* possibly aggregate */

        unsigned alloc = std::max( 1, count * size );
        Pointer &p = *reinterpret_cast< Pointer * >(
            dereference( instruction().result() ) );
        p = econtext.malloc( alloc, pointerId( true )[0] );
        econtext.memoryflag( instruction().result() ).set( MemoryFlag::HeapPointer );
    }

    void implement_extractvalue() {
        auto r = memcopy( ValueRef( instruction().operand( 0 ), 0, -1,
                                    compositeOffsetFromInsn( instruction().op->getOperand(0)->getType(), 1,
                                                             instruction().values.size() - 1 ) ),
                          instruction().result(), instruction().result().width );
        ASSERT_EQ( r, Problem::NoProblem );
        static_cast< void >( r );
    }

    void implement_insertvalue() { /* NB. Implemented against spec, UNTESTED! */
        /* first copy the original */
        auto r = memcopy( instruction().operand( 0 ), instruction().result(), instruction().result().width );
        ASSERT_EQ( r, Problem::NoProblem );
        /* write the new value over the selected field */
        r = memcopy( instruction().operand( 1 ),
                     ValueRef( instruction().result(), 0, -1,
                               compositeOffsetFromInsn( instruction().op->getOperand(0)->getType(), 2,
                                                        instruction().values.size() - 1 ) ),
                     instruction().operand( 1 ).width );
        ASSERT_EQ( r, Problem::NoProblem );
        static_cast< void >( r );
    }

    /******** Control flow ********/

    void jumpTo( ProgramInfo::Value v )
    {
        PC to = *reinterpret_cast< PC * >( dereference( v ) );
        jumpTo( to );
    }

    void jumpTo( PC to )
    {
        if ( ccontext.pc().function != to.function )
            ASSERT_UNREACHABLE_F( "Can't deal with cross-function jumps." );
        switchBB( to );
    }

    void maybe_die() {
        /* kill off everything if the main thread died */
        if ( ccontext.threadId() == 0 && ccontext.stackDepth() == 0 ) {
            for ( int i = 1; i < ccontext.threadCount(); ++i ) {
                ccontext.switch_thread( i );
                while ( ccontext.stackDepth() )
                    ccontext.leave();
            }
        }
    }

    void implement_ret() {
        if ( ccontext.stackDepth() == 1 ) {
            ccontext.leave();
            return maybe_die();
        }

        auto caller = info.instruction( ccontext.frame( 1 ).pc );
        if ( instruction().values.size() > 1 ) /* return value */
            memcopy( instruction().operand( 0 ), ValueRef( caller.result(), 1 ), caller.result().width );

        ccontext.leave();

        if ( isa< ::llvm::InvokeInst >( caller.op ) )
            jumpTo( caller.operand( -2 ) );
    }

    void implement_br()
    {
        if ( instruction().values.size() == 2 )
            jumpTo( instruction().operand( 0 ) );
        else {
            auto mflag = econtext.memoryflag( instruction().operand( 0 ) );
            if ( mflag.valid() && mflag.get() == MemoryFlag::Uninitialised )
                ccontext.problem( Problem::Uninitialised );
            if ( withValues( IsTrue(), instruction().operand( 0 ) ) )
                jumpTo( instruction().operand( 2 ) );
            else
                jumpTo( instruction().operand( 1 ) );
        }
    }

    void implement_indirectBr() {
        Pointer target = withValues( Get< Pointer >(), instruction().operand( 0 ) );
        jumpTo( *reinterpret_cast< PC * >( dereference( target ) ) );
    }

    /*
     * Two-phase PHI handler. We do this because all of the PHI nodes must be
     * executed atomically, reading their inputs before any of the results are
     * updated.  Not doing this can cause problems if the PHI nodes depend on
     * other PHI nodes for their inputs.  If the input PHI node is updated
     * before it is read, incorrect results can happen.
     */

    void switchBB( PC target )
    {
        PC origin = ccontext.pc();
        target.masked = origin.masked;
        ccontext.pc() = target;
        ccontext.jumped = true;

        _instruction = &info.instruction( ccontext.pc() );
        ASSERT( instruction().op );

        if ( !isa< ::llvm::PHINode >( instruction().op ) )
            return;  // Nothing fancy to do

        machine::Frame &original = ccontext.frame();
        int framesize = original.framesize( info );
        std::vector<char> tmp;
        tmp.resize( sizeof( machine::Frame ) + framesize );
        machine::Frame &copy = *reinterpret_cast< machine::Frame* >( &tmp[0] );
        copy = ccontext.frame();

        std::copy( original.memory(), original.memory() + framesize, copy.memory() );
        while ( ::llvm::PHINode *PN = dyn_cast< ::llvm::PHINode >( instruction().op ) ) {
            /* TODO use operands directly, avoiding valuemap lookup */
            auto v = info.valuemap[ PN->getIncomingValueForBlock(
                    cast< ::llvm::Instruction >( info.instruction( origin ).op )->getParent() ) ];
            FrameContext copyctx( info, copy );
            memcopy( v, instruction().result(), v.width, econtext, copyctx );
            ccontext.pc().instruction ++;
            _instruction = &info.instruction( ccontext.pc() );
        }
        std::copy( copy.memory(), copy.memory() + framesize, original.memory() );
    }

    /*
     * NB. Internally, we only deal with positive frameid's and they are always
     * relative to the *topmost* frame. The userspace doesn't know the stack
     * depth though, so we need to transform the frameid.
     */
    bool transform_frameid( int &frameid )
    {
        if ( frameid > 0 )
            // L - (L - id) - 1 = L - L + id - 1 = id - 1
            frameid = ccontext.stackDepth() - frameid;
        else
            frameid = std::min( frameid == INT_MIN ? INT_MAX : -frameid, ccontext.stackDepth() );

        if ( frameid > ccontext.stackDepth() ) {
            ccontext.problem( Problem::InvalidArgument );
            return false;
        }

        return true;
    }

    typedef std::vector< ProgramInfo::Value > Values;

    int find_invoke() {
        for ( int fid = 1; fid < ccontext.stackDepth(); ++fid ) {
            auto target = info.instruction( ccontext.frame( fid ).pc );
            if ( isa< ::llvm::InvokeInst >( target.op ) )
                return fid;
        }
        return ccontext.stackDepth(); /* past the end */
    }

    void unwind( int frameid, const Values &args )
    {
        if ( !transform_frameid( frameid ) )
            return;

        PC target_pc;

        /* we have a place to return to */
        if ( frameid < ccontext.stackDepth() ) {
            auto target = &info.instruction( ccontext.frame( frameid ).pc );

            if ( isa< ::llvm::InvokeInst >( target->op ) ) {
                target_pc = withValues( Get< PC >(), ValueRef( target->operand( -1 ), frameid ) );
                target = &info.instruction( target_pc );
            }

            int copied = 0;
            for ( auto arg : args ) {
                memcopy( arg, ValueRef( target->result(), frameid, -1, copied ), arg.width );
                copied += arg.width;
            }
        }

        while ( frameid ) { /* jump out */
            ccontext.leave();
            -- frameid;
        }

        if ( target_pc.function )
            jumpTo( target_pc );

        maybe_die();
    }

    struct LPInfo {
        bool cleanup;
        Pointer person;
        std::vector< std::pair< int, Pointer > > items;
        LPInfo() : cleanup( false ) {}
    };

    LPInfo getLPInfo( PC pc, int frameid = 0 )
    {
        LPInfo r;
        auto lpi = info.instruction( pc );
        if ( auto LPI = dyn_cast< ::llvm::LandingPadInst >( lpi.op ) ) {
            r.cleanup = LPI->isCleanup();
            r.person = withValues( Get< Pointer >(), ValueRef( lpi.operand( 0 ) ) );
            for ( int i = 0; i < int( LPI->getNumClauses() ); ++i ) {
                if ( LPI->isFilter( i ) ) {
                    r.items.push_back( std::make_pair( -1, Pointer() ) );
                } else {
                    auto tag = withValues( Get< Pointer >(), ValueRef( lpi.operand( i + 1 ), frameid ) );
                    auto type_id = info.function( pc ).typeID( tag );
                    r.items.push_back( std::make_pair( type_id, tag ) );
                }
            }
        }
        return r;
    }

    Pointer make_lpinfo( int frameid )
    {
        if ( !transform_frameid( frameid ) )
            return Pointer();
        if ( frameid == ccontext.stackDepth() )
            return Pointer();

        LPInfo lp;

        PC pc = ccontext.frame( frameid ).pc;
        auto insn = info.instruction( pc );
        if ( isa< ::llvm::InvokeInst >( insn.op ) ) {
            pc = withValues( Get< PC >(), ValueRef( insn.operand( -1 ), frameid ) );
            lp = getLPInfo( pc, frameid );
        }

        /* Well, this is not very pretty. */
        int psize = info.TD.getPointerSize();
        Pointer r = econtext.malloc( 8 + psize + (4 + psize) * lp.items.size(), 0 );

        auto c = econtext.dereference( r );
        *reinterpret_cast< int32_t * >( c ) = lp.cleanup; c += 4;
        *reinterpret_cast< int32_t * >( c ) = lp.items.size(); c += 4;
        *reinterpret_cast< Pointer * >( c ) = lp.person; c+= psize;

        for ( auto p : lp.items ) {
            *reinterpret_cast< int32_t * >( c ) = p.first; c += 4;
            *reinterpret_cast< Pointer * >( c ) = p.second; c+= psize;
        }

        return r;
    }

    struct PointerBlock {
        int count;
        Pointer ptr[0];
    };

    void implement_intrinsic( int id ) {
        switch ( id ) {
            case Intrinsic::vastart:
            case Intrinsic::trap:
            case Intrinsic::vaend:
            case Intrinsic::vacopy:
                ASSERT_UNIMPLEMENTED(); /* TODO */
            case Intrinsic::eh_typeid_for: {
                auto tag = withValues( Get< Pointer >(), instruction().operand( 0 ) );
                int type_id = info.function( ccontext.pc() ).typeID( tag );
                withValues( Set< int >( type_id ), instruction().result() );
                return;
            }
            case Intrinsic::smul_with_overflow:
            case Intrinsic::sadd_with_overflow:
            case Intrinsic::ssub_with_overflow:
                is_signed = true;
            case Intrinsic::umul_with_overflow:
            case Intrinsic::uadd_with_overflow:
            case Intrinsic::usub_with_overflow: {
                auto res = instruction().result();
                res.width = instruction().operand( 0 ).width;
                res.type = ProgramInfo::Value::Integer;
                auto over = instruction().result();
                over.width = 1; // hmmm
                over.type = ProgramInfo::Value::Integer;
                over.offset += compositeOffset( instruction().op->getType(), 1 ).first;
                withValues( ArithmeticWithOverflow( id ), res, over,
                            instruction().operand( 0 ), instruction().operand( 1 ) );
                return;
            }
            case Intrinsic::stacksave:
            case Intrinsic::stackrestore: {
                std::vector< ProgramInfo::Instruction > allocas;
                std::vector< Pointer > ptrs;
                auto &f = info.functions[ ccontext.pc().function ];
                for ( auto &i : f.instructions )
                    if ( i.opcode == LLVMInst::Alloca ) {
                        allocas.push_back( i );
                        auto ptr = withValues( Get< Pointer >(), i.result() );
                        if ( !ptr.null() )
                            ptrs.push_back( ptr );
                    }
                if ( id == Intrinsic::stacksave ) {
                    Pointer r = econtext.malloc( sizeof( PointerBlock ) + ptrs.size() * sizeof( Pointer ), 0 );
                    auto &c = *reinterpret_cast< PointerBlock * >( econtext.dereference( r ) );
                    c.count = ptrs.size();
                    int i = 0;
                    for ( auto ptr : ptrs ) {
                        econtext.memoryflag( r + sizeof( int ) + sizeof( Pointer ) * i ).set( MemoryFlag::HeapPointer );
                        c.ptr[ i++ ] = ptr;
                    }
                    withValues( Set< Pointer >( r, MemoryFlag::HeapPointer ), instruction().result() );
                } else { // stackrestore
                    Pointer r = withValues( Get< Pointer >(), instruction().operand( 0 ) );
                    auto &c = *reinterpret_cast< PointerBlock * >( econtext.dereference( r ) );
                    for ( auto ptr : ptrs ) {
                        bool retain = false;
                        for ( int i = 0; i < c.count; ++i )
                            if ( ptr == c.ptr[i] ) {
                                retain = true;
                                break;
                            }
                        if ( !retain )
                            econtext.free( ptr );
                    }
                }
                return;
            }
            default:
                /* We lowered everything else in buildInfo. */
                instruction().op->dump();
                ASSERT_UNREACHABLE_F( "unexpected intrinsic %d", id );
        }
    }

    void implement_builtin() {
        switch( instruction().builtin ) {
            case BuiltinChoice: {
                auto &c = ccontext.choice;
                c.options = withValues( GetInt(), instruction().operand( 0 ) );
                for ( int i = 1; i < int( instruction().values.size() ) - 2; ++i )
                    c.p.push_back( withValues( GetInt(), instruction().operand( i ) ) );
                if ( !c.p.empty() && int( c.p.size() ) != c.options ) {
                    ccontext.problem( Problem::InvalidArgument );
                    c.p.clear();
                }
                return;
            }
            case BuiltinAssert:
                if ( !withValues( GetInt(), instruction().operand( 0 ) ) )
                    ccontext.problem( Problem::Assert );
                return;
            case BuiltinProblem:
                ccontext.problem(
                    Problem::What( withValues( GetInt(), instruction().operand( 0 ) ) ),
                    withValues( Get< Pointer >(), instruction().operand( 1 ) ) );
                return;
            case BuiltinAp:
                ccontext.flags().ap |= (1 << withValues( GetInt(), instruction().operand( 0 ) ));
                return;
            case BuiltinMask: ccontext.pc().masked = true; return;
            case BuiltinUnmask: ccontext.pc().masked = false; return;
            case BuiltinInterrupt: return; /* an observable noop, see interpreter.h */
            case BuiltinGetTID:
                withValues( SetInt( ccontext.threadId() ), instruction().result() );
                return;
            case BuiltinNewThread: {
                PC entry = withValues( Get< PC >(), instruction().operand( 0 ) );
                Pointer arg = withValues( Get< Pointer >(), instruction().operand( 1 ) );
                int tid = ccontext.new_thread(
                    entry, Maybe< Pointer >::Just( arg ),
                    econtext.memoryflag( instruction().operand( 1 ) ).get() );
                withValues( SetInt( tid ), instruction().result() );
                return;
            }
            case BuiltinMalloc: {
                int size = withValues( Get< int >(), instruction().operand( 0 ) );
                if ( size >= ( 2 << Pointer::offsetSize ) ) {
                    ccontext.problem( Problem::InvalidArgument, Pointer() );
                    size = 0;
                }
                Pointer result = size ? econtext.malloc( size, pointerId( true )[0] ) : Pointer();
                withValues( Set< Pointer >( result, MemoryFlag::HeapPointer ), instruction().result() );
                return;
            }
            case BuiltinFree: {
                Pointer v = withValues( Get< Pointer >(), instruction().operand( 0 ) );
                if ( !econtext.free( v ) )
                    ccontext.problem( Problem::InvalidArgument, v );
                return;
            }
            case BuiltinHeapObjectSize: {
                Pointer v = withValues( Get< Pointer >(), instruction().operand( 0 ) );
                if ( !econtext.dereference( v ) )
                    ccontext.problem( Problem::InvalidArgument, v );
                else
                    withValues( Set< int >( econtext.pointerSize( v ) ), instruction().result() );
                return;
            }
            case BuiltinMemcpy: implement( Memcpy(), 4 ); return;
            case BuiltinVaStart: {
                auto f = info.functions[ ccontext.pc().function ];
                memcopy( f.values[ f.argcount ], instruction().result(), instruction().result().width );
                return;
            }
            case BuiltinUnwind:
                return unwind( withValues( GetInt(), instruction().operand( 0 ) ),
                               Values( instruction().values.begin() + 2,
                                       instruction().values.end() - 1 ) );
            case BuiltinLandingPad:
                withValues( Set< Pointer >( make_lpinfo( withValues( GetInt(), instruction().operand( 0 ) ) ),
                                            MemoryFlag::HeapPointer ),
                            instruction().result() );
                return;
            default:
                ASSERT_UNREACHABLE_F( "unknown builtin %d", instruction().builtin );
        }
    }

    void implement_call() {
        CallSite CS( cast< ::llvm::Instruction >( instruction().op ) );
        ::llvm::Function *F = CS.getCalledFunction();

        if ( F && F->isDeclaration() ) {
            auto id = F->getIntrinsicID();
            if ( id != Intrinsic::not_intrinsic )
                return implement_intrinsic( id );

            if ( instruction().builtin )
                return implement_builtin();
        }

        bool invoke = isa< ::llvm::InvokeInst >( instruction().op );
        auto pc = withValues( Get< PC >(), instruction().operand( invoke ? -3 : -1 ) );

        if ( !pc.function ) {
            ccontext.problem( Problem::InvalidArgument ); /* function 0 does not exist */
            return;
        }

        const auto &function = info.function( pc );

        /* report problems with the call before pushing the new stackframe */
        if ( !function.vararg && int( CS.arg_size() ) > function.argcount )
            ccontext.problem( Problem::InvalidArgument ); /* too many actual arguments */

        ccontext.enter( pc.function ); /* push a new frame */
        ccontext.jumped = true;

        /* Copy arguments to the new frame. */
        for ( int i = 0; i < int( CS.arg_size() ) && i < int( function.argcount ); ++i )
            memcopy( ValueRef( instruction().operand( i ), 1 ), function.values[ i ],
                     function.values[ i ].width );

        if ( function.vararg ) {
            int size = 0;
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i )
                size += instruction().operand( i ).width;
            Pointer vaptr = size ? econtext.malloc( size, 0 ) : Pointer();
            withValues( Set< Pointer >( vaptr, MemoryFlag::HeapPointer ),
                        function.values[ function.argcount ] );
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i ) {
                auto op = instruction().operand( i );
                memcopy( ValueRef( op, 1 ), vaptr, op.width );
                vaptr = vaptr + int( op.width );
            }
        }

        ASSERT( !isa< ::llvm::PHINode >( instruction().op ) );
    }

    /******** Dispatch ********/

    void run() {
        is_signed = false;
        switch ( instruction().opcode ) {
            case LLVMInst::GetElementPtr:
                implement( GetElement(), 2 ); break;
            case LLVMInst::Select:
                implement< Select >(); break;

            case LLVMInst::ICmp:
                switch ( dyn_cast< ICmpInst >( instruction().op )->getPredicate() ) {
                    case ICmpInst::ICMP_SLT:
                    case ICmpInst::ICMP_SGT:
                    case ICmpInst::ICMP_SLE:
                    case ICmpInst::ICMP_SGE: is_signed = true;
                    default: ;
                }
                implement< ICmp >(); break;

            case LLVMInst::FCmp:
                implement< FCmp >(); break;

            case LLVMInst::ZExt:
            case LLVMInst::FPExt:
            case LLVMInst::UIToFP:
            case LLVMInst::FPToUI:
            case LLVMInst::PtrToInt:
            case LLVMInst::IntToPtr:
            case LLVMInst::FPTrunc:
            case LLVMInst::Trunc:
                implement< Convert >(); break;

            case LLVMInst::Br:
                implement_br(); break;
            case LLVMInst::IndirectBr:
                implement_indirectBr(); break;
            case LLVMInst::Switch:
                implement( Switch(), 2 ); break;
            case LLVMInst::Call:
            case LLVMInst::Invoke:
                implement_call(); break;
            case LLVMInst::Ret:
                implement_ret(); break;

            case LLVMInst::SExt:
            case LLVMInst::SIToFP:
            case LLVMInst::FPToSI:
                is_signed = true;
                implement< Convert >(); break;

            case LLVMInst::BitCast:
                implement_bitcast(); break;

            case LLVMInst::Load:
                implement_load(); break;
            case LLVMInst::Store:
                implement_store(); break;
            case LLVMInst::Alloca:
                implement_alloca(); break;
            case LLVMInst::AtomicCmpXchg:
                implement< CmpXchg >(); break;
            case LLVMInst::AtomicRMW:
                implement< AtomicRMW >(); break;

            case LLVMInst::ExtractValue:
                implement_extractvalue(); break;
            case LLVMInst::InsertValue:
                implement_insertvalue(); break;

            case LLVMInst::SDiv:
            case LLVMInst::SRem:
                is_signed = true;
            case LLVMInst::FAdd:
            case LLVMInst::Add:
            case LLVMInst::FSub:
            case LLVMInst::Sub:
            case LLVMInst::FMul:
            case LLVMInst::Mul:
            case LLVMInst::FDiv:
            case LLVMInst::UDiv:
            case LLVMInst::FRem:
            case LLVMInst::URem:
                implement< Arithmetic >(); break;

            case LLVMInst::AShr:
                is_signed = true;
            case LLVMInst::And:
            case LLVMInst::Or:
            case LLVMInst::Xor:
            case LLVMInst::Shl:
            case LLVMInst::LShr:
                implement< Bitwise >(); break;

            case LLVMInst::Unreachable:
                ccontext.problem( Problem::UnreachableExecuted );
                break;

            case LLVMInst::Resume:
                /* unwind down to the next invoke instruction in the stack */
                unwind( -find_invoke(), Values( { instruction().operand( 0 ) } ) );
                break;

            case LLVMInst::LandingPad:
                break; /* nothing to do, handled by the unwinder */

            case LLVMInst::Fence: /* noop until we have reordering simulation */
                break;

            default:
                instruction().op->dump();
                ASSERT_UNREACHABLE_F( "unknown opcode %d", instruction().opcode );
        }

        /* invoke and call are checked when ret executes */
        if ( instruction().opcode != LLVMInst::Call &&
             instruction().opcode != LLVMInst::Invoke )
            checkPointsTo();
    }

    template< typename Fun >
    typename Fun::T implement( brick::types::NotPreferred, Fun = Fun(), ... )
    {
        instruction().op->dump();
        ASSERT_UNREACHABLE_F( "bad parameters for opcode %d", instruction().opcode );
    }

    template< typename Fun, typename I, typename Cons,
            typename = decltype( match( std::declval< Fun & >(), std::declval< Cons & >() ) ) >
    auto implement( brick::types::Preferred, Fun fun, I i, I e, Cons list )
        -> typename std::enable_if< Fun::arity == Cons::length, typename Fun::T >::type
    {
        ASSERT( i == e );
        brick::_assert::unused( i, e );
        fun._evaluator = this;
        auto retval = match( fun, list );
        auto mflag = econtext.memoryflag( result.back() );
        if ( mflag.valid() )
            mflag.set( fun.resultFlag( flags.back() ) );
        flags.pop_back();
        result.pop_back();
        return retval;
    }

    template< typename Fun, typename I, typename Cons,
            typename = decltype( match( std::declval< Fun & >(), std::declval< Cons & >() ) ) >
    auto implement( brick::types::Preferred, Fun fun, I i, I e, Cons list )
        -> typename std::enable_if< (Fun::arity > Cons::length), typename Fun::T >::type
    {
        typedef ProgramInfo::Value Value;
        brick::types::Preferred p;

        ASSERT( i != e );

        ValueRef v = *i++;
        char *mem = dereference( v );
        auto mflag = econtext.memoryflag( v );
        flags.back().push_back( mflag.valid() ? mflag.get() : MemoryFlag::Data );

        switch ( v.v.type ) {
            case Value::Integer: if ( is_signed ) switch ( v.v.width ) {
                    case 1: return implement( p, fun, i, e, consPtr<  int8_t >( mem, list ) );
                    case 4: return implement( p, fun, i, e, consPtr< int32_t >( mem, list ) );
                    case 2: return implement( p, fun, i, e, consPtr< int16_t >( mem, list ) );
                    case 8: return implement( p, fun, i, e, consPtr< int64_t >( mem, list ) );
                } else switch ( v.v.width ) {
                    case 1: return implement( p, fun, i, e, consPtr<  uint8_t >( mem, list ) );
                    case 4: return implement( p, fun, i, e, consPtr< uint32_t >( mem, list ) );
                    case 2: return implement( p, fun, i, e, consPtr< uint16_t >( mem, list ) );
                    case 8: return implement( p, fun, i, e, consPtr< uint64_t >( mem, list ) );
                }
                ASSERT_UNREACHABLE_F( "Wrong integer width %d", v.v.width );
            case Value::Pointer: case Value::Alloca:
                return implement( p, fun, i, e, consPtr< Pointer >( mem, list ) );
            case Value::CodePointer:
                return implement( p, fun, i, e, consPtr< PC >( mem, list ) );
            case Value::Float: switch ( v.v.width ) {
                case sizeof(float):
                    return implement( p, fun, i, e, consPtr< float >( mem, list ) );
                case sizeof(double):
                    return implement( p, fun, i, e, consPtr< double >( mem, list ) );
            }
            case Value::Aggregate:
                return implement( p, fun, i, e, consPtr< void >( mem, list ) );
            case Value::Void:
                return implement( p, fun, i, e, list ); /* ignore void items */
        }

        ASSERT_UNREACHABLE_F( "unexpected value type %d", v.v.type );
    }

    template< typename Fun, int Limit = 3 >
    typename Fun::T implement( Fun fun = Fun(), int limit = 0 )
    {
        flags.push_back( MemoryFlags() );
        auto i = instruction().values.begin(), e = limit ? i + limit : instruction().values.end();
        result.push_back( instruction().result() );
        return implement( brick::types::Preferred(), fun, i, e, list::Nil() );
    }

    template< typename Fun >
    typename Fun::T _withValues( Fun fun ) {
        flags.push_back( MemoryFlags() );
        return implement< Fun >( brick::types::Preferred(), fun, values.begin(), values.end(), list::Nil() );
    }

    template< typename Fun, typename... Values >
    typename Fun::T _withValues( Fun fun, ValueRef v, Values... vs ) {
        if ( values.empty() )
            result.push_back( v );
        values.push_back( v );
        return _withValues< Fun >( fun, vs... );
    }

    template< typename Fun, typename... Values >
    typename Fun::T withValues( Fun fun, Values... vs ) {
        values.clear();
        return _withValues< Fun >( fun, vs... );
    }
};


}
}

#endif
