// -*- C++ -*- (c) 2012 Petr Rockai

#define NO_RTTI

#include <wibble/exception.h>
#include <wibble/test.h>
#include <wibble/maybe.h>

#include <divine/llvm/machine.h>
#include <divine/llvm/program.h>

#include <divine/llvm/wrap/Instructions.h>
#include <divine/llvm/wrap/Constants.h>

#include <llvm/Support/GetElementPtrTypeIterator.h>
#include <llvm/Support/CallSite.h>
#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Target/TargetData.h>
#else
  #include <divine/llvm/wrap/DataLayout.h>
  #define TargetData DataLayout
#endif

#include <algorithm>
#include <cmath>

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
using wibble::Unit;
using wibble::Maybe;

template< int N, typename _T > struct Z;
template< int N, typename _T > struct NZ { typedef _T T; };

template< typename _T > struct Z< 0, _T > { typedef _T T; };
template< typename _T > struct NZ< 0, _T > {};

struct Nil {
    static const int length = 0;
};

template< int N > struct ConsAt;

template<>
struct ConsAt< 0 > {
    template< typename Cons >
    static auto get( Cons &c ) -> decltype( c.car ) {
        return c.car;
    }
};

template< int N >
struct ConsAt {
    template< typename Cons >
    static auto get( Cons &c ) -> decltype( ConsAt< N - 1 >::get( c.cdr ) )
    {
        return ConsAt< N - 1 >::get( c.cdr );
    }
};

template< int, int > struct Eq;
template< int i > struct Eq< i, i > { typedef int Yes; };

template< typename A, typename B >
struct Cons {
    typedef A Car;
    typedef B Cdr;
    A car;
    B cdr;
    static const int length = 1 + B::length;

    template< int N >
    auto get() -> decltype( ConsAt< N >::get( *this ) )
    {
        return ConsAt< N >::get( *this );
    }
};

template< typename A, typename B >
Cons< A, B > cons( A a, B b ) {
    Cons< A, B > r;
    r.car = a; r.cdr = b;
    return r;
}

template< typename As, typename A, typename B >
Cons< As *, B > consPtr( A *a, B b ) {
    Cons< As *, B > r;
    r.car = reinterpret_cast< As * >( a );
    r.cdr = b;
    return r;
}

template< typename X >
struct UnPtr {};
template< typename X >
struct UnPtr< X * > { typedef X T; };

template< int I, typename Cons >
auto decons( Cons c ) -> typename UnPtr< decltype( ConsAt< I >::get( c ) ) >::T &
{
    return *c.template get< I >();
}

#define MATCH(l, ...) template< typename F, typename X > \
    auto match( F &f, X x ) -> \
        typename wibble::TPair< typename Eq< l, X::length >::Yes, decltype( f( __VA_ARGS__ ) ) >::Second \
    { return f( __VA_ARGS__ ); }

MATCH( 0, /**/ )
MATCH( 1, decons< 0 >( x ) )
MATCH( 2, decons< 1 >( x ), decons< 0 >( x ) )
MATCH( 3, decons< 2 >( x ), decons< 1 >( x ), decons< 0 >( x ) )
MATCH( 4, decons< 3 >( x ), decons< 2 >( x ), decons< 1 >( x ), decons< 0 >( x ) )

#undef MATCH

/* Dummy implementation of a ControlContext, useful for Evaluator for
 * control-flow-free snippets (like ConstantExpr). */
struct ControlContext {
    bool jumped;
    Choice choice;
    void enter( int ) { assert_die(); }
    void leave() { assert_die(); }
    MachineState::Frame &frame( int /*depth*/ = 0 ) { assert_die(); }
    MachineState::Flags &flags() { assert_die(); }
    void problem( Problem::What, Pointer = Pointer() ) { assert_die(); }
    PC &pc() { assert_die(); }
    int new_thread( PC, Maybe< Pointer >, MemoryFlag ) { assert_die(); }
    int stackDepth() { assert_die(); }
    int threadId() { assert_die(); }
    int threadCount() { assert_die(); }
    void switch_thread( int ) { assert_die(); }
    void dump() {}
};

template< typename X >
struct Dummy {
    static X &v() { assert_die(); }
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
    ProgramInfo::Instruction instruction;
    std::vector< ValueRef > values; /* a withValues stash */
    typedef std::vector< MemoryFlag > MemoryFlags;
    std::vector< MemoryFlags > flags;
    std::vector< ValueRef > result;

    struct Implementation {
        typedef Unit T;
        Evaluator< EvalContext, ControlContext > *_evaluator;
        Evaluator< EvalContext, ControlContext > &evaluator() { return *_evaluator; }
        ProgramInfo::Instruction i() { return _evaluator->instruction; }
        EvalContext &econtext() { return _evaluator->econtext; }
        ControlContext &ccontext() { return _evaluator->ccontext; }
        ::llvm::TargetData &TD() { return econtext().TD; }
        MemoryFlag resultFlag( std::vector< MemoryFlag > ) { return MemoryFlag::Data; }
    };

    template< typename X >
    char *dereference( X x ) { return econtext.dereference( x ); }

    template< typename... X >
    static Unit declcheck( X... ) { return Unit(); }

    /******** Arithmetic & comparisons *******/

    struct Arithmetic : Implementation {
        static const int arity = 3;
        template< typename X = int >
        auto operator()( X &r = Dummy< X >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> decltype( declcheck( a + b, a % b, std::fmod( a, b ) ) )
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
                case LLVMInst::FRem: r = std::fmod( a, b ); return Unit();
                case LLVMInst::URem:
                case LLVMInst::SRem:
                    if ( !b )
                        this->ccontext().problem( Problem::DivisionByZero );
                    r = b ? (a % b) : 0;
                    return Unit();
                case LLVMInst::And:  r = a & b; return Unit();
                case LLVMInst::Or:   r = a | b; return Unit();
                case LLVMInst::Xor:  r = a ^ b; return Unit();
                case LLVMInst::Shl:  r = a << b; return Unit();
                case LLVMInst::AShr:  // XXX?
                case LLVMInst::LShr:  r = a >> b; return Unit();
            }
            assert_die();
        }

        MemoryFlag resultFlag( std::vector< MemoryFlag > x ) {
            if ( x[1] == MemoryFlag::Uninitialised || x[2] == MemoryFlag::Uninitialised )
                return MemoryFlag::Uninitialised;
            if ( x[1] == MemoryFlag::HeapPointer || x[2] == MemoryFlag::HeapPointer )
                return MemoryFlag::HeapPointer;
            return MemoryFlag::Data;
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
        MemoryFlag resulFlag( std::vector< MemoryFlag > x ) { return x[ _selected ]; }
    };

    struct ICmp : Implementation {
        static const int arity = 3;
        template< typename R = int, typename X = int >
        auto operator()( R &r = Dummy< R >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> decltype( declcheck( r = a < b ) )
        {
            switch (dyn_cast< ICmpInst >( this->i().op )->getPredicate()) {
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
                default: assert_die();
            }
        }
    };

    struct FCmp : Implementation {
        static const int arity = 3;
        template< typename R = int, typename X = int >
        auto operator()( R &r = Dummy< R >::v(),
                         X &a = Dummy< X >::v(),
                         X &b = Dummy< X >::v() )
            -> decltype( declcheck( r = isnan( a ) && isnan( b ) ) )
        {
            switch ( dyn_cast< FCmpInst >( this->i().op )->getPredicate() ) {
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
                default: assert_die();
            }

            switch ( dyn_cast< FCmpInst >( this->i().op )->getPredicate() ) {
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
                default: assert_die();
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

        MemoryFlag resultFlag( std::vector< MemoryFlag > x ) { return x[1]; }
    };

    void implement_bitcast() {
        auto r = memcopy( instruction.operand( 0 ), instruction.result(), instruction.result().width );
        assert_eq( r, Problem::NoProblem );
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

        MemoryFlag resultFlag( std::vector< MemoryFlag > x ) { return x[0]; }
    };

    template< typename _T >
    struct Set : Implementation {
        typedef _T Arg;
        Arg v;
        MemoryFlag _flag;
        static const int arity = 1;

        template< typename X = int >
        auto operator()( X &r = Dummy< X >::v() )
            -> decltype( declcheck( static_cast< X >( v ) ) )
        {
            r = static_cast< X >( v );
            return Unit();
        }

        MemoryFlag resultFlag( std::vector< MemoryFlag > x ) { return _flag; }

        Set( Arg v, MemoryFlag f = MemoryFlag::Data ) : v( v ), _flag( f ) {}
    };

    typedef Get< bool > IsTrue;
    typedef Get< int > GetInt;
    typedef Set< int > SetInt;

    /******** Memory access & conversion ********/

    int compositeOffset( ::llvm::Type *t, int current, int end )
    {
        if ( current == end )
            return 0;

        int offset = 0;
        int index = withValues( GetInt(), instruction.operand( current ) );

        if (::llvm::StructType *STy = dyn_cast< ::llvm::StructType >( t )) {
            const ::llvm::StructLayout *SLO = econtext.TD.getStructLayout(STy);
            offset = SLO->getElementOffset( index );
        } else {
            const ::llvm::SequentialType *ST = cast< ::llvm::SequentialType >( t );
            offset = index * econtext.TD.getTypeAllocSize( ST->getElementType() );
        }
        return offset +
            compositeOffset( cast< ::llvm::CompositeType >( t )->getTypeAtIndex( index ),
                             current + 1, end );
    }

    struct GetElement : Implementation {
        static const int arity = 2;
        Unit operator()( Pointer &r = Dummy< Pointer >::v(),
                         Pointer &p = Dummy< Pointer >::v() )
        {
            r = p + this->evaluator().compositeOffset(
                this->i().op->getOperand(0)->getType(), 1, this->i().values.size() - 1 );
            return Unit();
        }
        MemoryFlag resultFlag( std::vector< MemoryFlag > x ) {
            assert_leq( 2U, x.size() );
            return x[1];
        }
    };

    void implement_store() {
        Pointer to = withValues( Get< Pointer >(), instruction.operand( 1 ) );
        if ( auto r = memcopy( instruction.operand( 0 ), to, instruction.operand( 0 ).width ) )
            ccontext.problem( r );
    }

    void implement_load() {
        Pointer from = withValues( Get< Pointer >(), instruction.operand( 0 ) );
        if ( auto r = memcopy( from, instruction.result(), instruction.result().width ) )
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
        MemoryFlag resultFlag( std::vector< MemoryFlag > x ) { return x[1]; }
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

        MemoryFlag resultFlag( std::vector< MemoryFlag > x ) { return x[0]; }
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

        MemoryFlag resultFlag( std::vector< MemoryFlag > ) {
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
                case AtomicRMWInst::BAD_BINOP: assert_unreachable( "bad binop in atomicrmw" );
            }

            return Unit();
        }

        MemoryFlag resultFlag( std::vector< MemoryFlag > ) {
            return MemoryFlag::Data; /* nothing */
        }
    };

    void implement_alloca() {
        ::llvm::AllocaInst *I = cast< ::llvm::AllocaInst >( instruction.op );
        Type *ty = I->getAllocatedType();

        int count = withValues( GetInt(), instruction.operand( 0 ) );
        int size = econtext.TD.getTypeAllocSize(ty); /* possibly aggregate */

        unsigned alloc = std::max( 1, count * size );
        Pointer &p = *reinterpret_cast< Pointer * >(
            dereference( instruction.result() ) );
        p = econtext.malloc( alloc );
        econtext.memoryflag( instruction.result() ).set( MemoryFlag::HeapPointer );
    }

    void implement_extractvalue() {
        auto r = memcopy( ValueRef( instruction.operand( 0 ), 0, -1,
                                    compositeOffset( instruction.op->getOperand(0)->getType(), 1,
                                                     instruction.values.size() - 1 ) ),
                          instruction.result(), instruction.result().width );
        assert_eq( r, Problem::NoProblem );
        static_cast< void >( r );
    }

    void implement_insertvalue() { /* NB. Implemented against spec, UNTESTED! */
        /* first copy the original */
        auto r = memcopy( instruction.operand( 0 ), instruction.result(), instruction.result().width );
        assert_eq( r, Problem::NoProblem );
        /* write the new value over the selected field */
        r = memcopy( instruction.operand( 1 ),
                     ValueRef( instruction.result(), 0, -1,
                               compositeOffset( instruction.op->getOperand(0)->getType(), 2,
                                                instruction.values.size() - 1 ) ),
                     instruction.operand( 1 ).width );
        assert_eq( r, Problem::NoProblem );
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
            throw wibble::exception::Consistency(
                "Evaluator::jumpTo",
                "Can't deal with cross-function jumps." );
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
        if ( instruction.values.size() > 1 ) /* return value */
            memcopy( instruction.operand( 0 ), ValueRef( caller.result(), 1 ), caller.result().width );

        ccontext.leave();

        if ( isa< ::llvm::InvokeInst >( caller.op ) )
            jumpTo( caller.operand( -2 ) );
    }

    void implement_br()
    {
        if ( instruction.values.size() == 2 )
            jumpTo( instruction.operand( 0 ) );
        else {
            if ( econtext.memoryflag( instruction.operand( 0 ) ).get() ==
                 MemoryFlag::Uninitialised )
                ccontext.problem( Problem::Uninitialised );
            if ( withValues( IsTrue(), instruction.operand( 0 ) ) )
                jumpTo( instruction.operand( 2 ) );
            else
                jumpTo( instruction.operand( 1 ) );
        }
    }

    void implement_indirectBr() {
        Pointer target = withValues( Get< Pointer >(), instruction.operand( 0 ) );
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

        instruction = info.instruction( ccontext.pc() );
        assert( instruction.op );

        if ( !isa< ::llvm::PHINode >( instruction.op ) )
            return;  // Nothing fancy to do

        MachineState::Frame &original = ccontext.frame();
        int framesize = original.framesize( info );
        std::vector<char> tmp;
        tmp.resize( sizeof( MachineState::Frame ) + framesize );
        MachineState::Frame &copy = *reinterpret_cast< MachineState::Frame* >( &tmp[0] );
        copy = ccontext.frame();

        std::copy( original.memory(), original.memory() + framesize, copy.memory() );
        while ( ::llvm::PHINode *PN = dyn_cast< ::llvm::PHINode >( instruction.op ) ) {
            /* TODO use operands directly, avoiding valuemap lookup */
            auto v = info.valuemap[ PN->getIncomingValueForBlock(
                    cast< ::llvm::Instruction >( info.instruction( origin ).op )->getParent() ) ];
            FrameContext copyctx( info, copy );
            memcopy( v, instruction.result(), v.width, econtext, copyctx );
            ccontext.pc().instruction ++;
            instruction = info.instruction( ccontext.pc() );
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
            auto target = info.instruction( ccontext.frame( frameid ).pc );

            if ( isa< ::llvm::InvokeInst >( target.op ) ) {
                target_pc = withValues( Get< PC >(), ValueRef( target.operand( -1 ), frameid ) );
                target = info.instruction( target_pc );
            }

            int copied = 0;
            for ( auto arg : args ) {
                memcopy( arg, ValueRef( target.result(), frameid, -1, copied ), arg.width );
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
        Pointer r = econtext.malloc( 8 + psize + (4 + psize) * lp.items.size() );

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

    void implement_call() {
        CallSite CS( cast< ::llvm::Instruction >( instruction.op ) );
        ::llvm::Function *F = CS.getCalledFunction();

        if ( F && F->isDeclaration() ) {

            switch (F->getIntrinsicID()) {
                case Intrinsic::not_intrinsic: break;
                case Intrinsic::vastart:
                case Intrinsic::trap:
                case Intrinsic::vaend:
                case Intrinsic::vacopy:
                    assert_unimplemented(); /* TODO */
                case Intrinsic::eh_typeid_for: {
                    auto tag = withValues( Get< Pointer >(), instruction.operand( 0 ) );
                    int type_id = info.function( ccontext.pc() ).typeID( tag );
                    withValues( Set< int >( type_id ), instruction.result() );
                    return;
                }
                default:
                    /* We lowered everything else in buildInfo. */
                    F->dump();
                    assert_unreachable( "unexpected intrinsic %d", F->getIntrinsicID() );
            }

            switch( instruction.builtin ) {
                case NotBuiltin: break;
                case BuiltinChoice: {
                    auto &c = ccontext.choice;
                    c.options = withValues( GetInt(), instruction.operand( 0 ) );
                    for ( int i = 1; i < int( instruction.values.size() ) - 2; ++i )
                        c.p.push_back( withValues( GetInt(), instruction.operand( i ) ) );
                    if ( !c.p.empty() && int( c.p.size() ) != c.options ) {
                        ccontext.problem( Problem::InvalidArgument );
                        c.p.clear();
                    }
                    return;
                }
                case BuiltinAssert:
                    if ( !withValues( GetInt(), instruction.operand( 0 ) ) )
                        ccontext.problem( Problem::Assert );
                    return;
                case BuiltinProblem:
                    ccontext.problem(
                        Problem::What( withValues( GetInt(), instruction.operand( 0 ) ) ),
                        withValues( Get< Pointer >(), instruction.operand( 1 ) ) );
                    return;
                case BuiltinAp:
                    ccontext.flags().ap |= (1 << withValues( GetInt(), instruction.operand( 0 ) ));
                    return;
                case BuiltinMask: ccontext.pc().masked = true; return;
                case BuiltinUnmask: ccontext.pc().masked = false; return;
                case BuiltinInterrupt: return; /* an observable noop, see interpreter.h */
                case BuiltinGetTID:
                    withValues( SetInt( ccontext.threadId() ), instruction.result() );
                    return;
                case BuiltinNewThread: {
                    PC entry = withValues( Get< PC >(), instruction.operand( 0 ) );
                    Pointer arg = withValues( Get< Pointer >(), instruction.operand( 1 ) );
                    int tid = ccontext.new_thread(
                        entry, Maybe< Pointer >::Just( arg ),
                        econtext.memoryflag( instruction.operand( 1 ) ).get() );
                    withValues( SetInt( tid ), instruction.result() );
                    return;
                }
                case BuiltinMalloc: {
                    int size = withValues( Get< int >(), instruction.operand( 0 ) );
                    Pointer result = size ? econtext.malloc( size ) : Pointer();
                    withValues( Set< Pointer >( result, MemoryFlag::HeapPointer ), instruction.result() );
                    return;
                }
                case BuiltinFree: {
                    Pointer v = withValues( Get< Pointer >(), instruction.operand( 0 ) );
                    if ( !econtext.free( v ) )
                        ccontext.problem( Problem::InvalidArgument, v );
                    return;
                }
                case BuiltinHeapObjectSize: {
                    Pointer v = withValues( Get< Pointer >(), instruction.operand( 0 ) );
                    if ( !econtext.dereference( v ) )
                        ccontext.problem( Problem::InvalidArgument, v );
                    else
                        withValues( Set< int >( econtext.pointerSize( v ) ), instruction.result() );
                    return;
                }
                case BuiltinMemcpy: implement( Memcpy(), 4 ); return;
                case BuiltinVaStart: {
                    auto f = info.functions[ ccontext.pc().function ];
                    memcopy( f.values[ f.argcount ], instruction.result(), instruction.result().width );
                    return;
                }
                case BuiltinUnwind:
                    return unwind( withValues( GetInt(), instruction.operand( 0 ) ),
                                   Values( instruction.values.begin() + 2,
                                           instruction.values.end() - 1 ) );
                case BuiltinLandingPad:
                    withValues( Set< Pointer >( make_lpinfo( withValues( GetInt(), instruction.operand( 0 ) ) ),
                                                MemoryFlag::HeapPointer ),
                                instruction.result() );
                    return;
            }
        }

        bool invoke = isa< ::llvm::InvokeInst >( instruction.op );
        auto pc = withValues( Get< PC >(), instruction.operand( invoke ? -3 : -1 ) );

        if ( !pc.function ) {
            ccontext.problem( Problem::InvalidArgument ); /* function 0 does not exist */
            return;
        }

        auto function = info.function( pc );

        /* report problems with the call before pushing the new stackframe */
        if ( !function.vararg && int( CS.arg_size() ) > function.argcount )
            ccontext.problem( Problem::InvalidArgument ); /* too many actual arguments */

        ccontext.enter( pc.function ); /* push a new frame */
        ccontext.jumped = true;

        /* Copy arguments to the new frame. */
        for ( int i = 0; i < int( CS.arg_size() ) && i < int( function.argcount ); ++i )
            memcopy( ValueRef( instruction.operand( i ), 1 ), function.values[ i ],
                     function.values[ i ].width );

        if ( function.vararg ) {
            int size = 0;
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i )
                size += instruction.operand( i ).width;
            Pointer vaptr = size ? econtext.malloc( size ) : Pointer();
            withValues( Set< Pointer >( vaptr, MemoryFlag::HeapPointer ),
                        function.values[ function.argcount ] );
            for ( int i = function.argcount; i < int( CS.arg_size() ); ++i ) {
                auto op = instruction.operand( i );
                memcopy( ValueRef( op, 1 ), vaptr, op.width );
                vaptr = vaptr + int( op.width );
            }
        }

        assert( !isa< ::llvm::PHINode >( instruction.op ) );
    }

    /******** Dispatch ********/

    void run() {
        is_signed = false;
        switch ( instruction.opcode ) {
            case LLVMInst::GetElementPtr:
                implement( GetElement(), 2 ); break;
            case LLVMInst::Select:
                implement< Select >(); break;

            case LLVMInst::ICmp:
                switch ( dyn_cast< ICmpInst >( instruction.op )->getPredicate() ) {
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
            case LLVMInst::AShr:
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
            case LLVMInst::And:
            case LLVMInst::Or:
            case LLVMInst::Xor:
            case LLVMInst::Shl:
            case LLVMInst::LShr:
                implement< Arithmetic >(); break;

            case LLVMInst::Unreachable:
                ccontext.problem( Problem::UnreachableExecuted );
                break;

            case LLVMInst::Resume:
                /* unwind down to the next invoke instruction in the stack */
                unwind( -find_invoke(), Values( { instruction.operand( 0 ) } ) );
                break;

            case LLVMInst::LandingPad:
                break; /* nothing to do, handled by the unwinder */

            case LLVMInst::Fence: /* noop until we have reordering simulation */
                break;

            default:
                instruction.op->dump();
                assert_unreachable( "unknown opcode %d", instruction.opcode );
        }
    }

    template< typename Fun, typename I, typename Cons >
    typename Fun::T implement( wibble::NotPreferred, I /*i*/, I /*e*/, Cons /*list*/, Fun = Fun() ) {
        instruction.op->dump();
        assert_unreachable( "bad parameters for opcode %d", instruction.opcode );
    }

    template< typename Fun, typename I, typename Cons >
    auto implement( wibble::Preferred, I i, I e, Cons list, Fun fun = Fun() )
        -> typename wibble::TPair< wibble::TPair< decltype( match( fun, list ) ),
                                                  typename Eq< true, (Fun::arity == Cons::length) >::Yes >,
                                   typename Fun::T >::Second
    {
        typedef ProgramInfo::Value Value;

        assert( i == e );
        wibble::param::discard( i, e );
        fun._evaluator = this;
        auto retval = match( fun, list );
        econtext.memoryflag( result.back() ).set( fun.resultFlag( flags.back() ) );
        flags.pop_back();
        result.pop_back();
        return retval;
    }

    template< typename Fun, typename I, typename Cons >
    auto implement( wibble::Preferred, I i, I e, Cons list, Fun fun = Fun() )
        -> typename wibble::TPair< wibble::TPair< decltype( match( fun, list ) ),
                                                  typename Eq< true, (Fun::arity > Cons::length) >::Yes >,
                                   typename Fun::T >::Second
    {
        typedef ProgramInfo::Value Value;
        wibble::Preferred p;

        assert( i != e );

        ValueRef v = *i++;
        char *mem = dereference( v );
        flags.back().push_back( econtext.memoryflag( v ).get() );

        switch ( v.v.type ) {
            case Value::Integer: if ( is_signed ) switch ( v.v.width ) {
                    case 1: return implement( p, i, e, consPtr<  int8_t >( mem, list ), fun );
                    case 4: return implement( p, i, e, consPtr< int32_t >( mem, list ), fun );
                    case 2: return implement( p, i, e, consPtr< int16_t >( mem, list ), fun );
                    case 8: return implement( p, i, e, consPtr< int64_t >( mem, list ), fun );
                } else switch ( v.v.width ) {
                    case 1: return implement( p, i, e, consPtr<  uint8_t >( mem, list ), fun );
                    case 4: return implement( p, i, e, consPtr< uint32_t >( mem, list ), fun );
                    case 2: return implement( p, i, e, consPtr< uint16_t >( mem, list ), fun );
                    case 8: return implement( p, i, e, consPtr< uint64_t >( mem, list ), fun );
                }
                assert_unreachable( "Wrong integer width %d", v.v.width );
            case Value::Pointer: case Value::Alloca:
                return implement( p, i, e, consPtr< Pointer >( mem, list ), fun );
            case Value::CodePointer:
                return implement( p, i, e, consPtr< PC >( mem, list ), fun );
            case Value::Float: switch ( v.v.width ) {
                case sizeof(float):
                    return implement( p, i, e, consPtr< float >( mem, list ), fun );
                case sizeof(double):
                    return implement( p, i, e, consPtr< double >( mem, list ), fun );
            }
            case Value::Aggregate:
                return implement( p, i, e, consPtr< void >( mem, list ), fun );
            case Value::Void:
                return implement( p, i, e, list, fun ); /* ignore void items */
        }

        assert_die();
    }

    template< typename Fun, int Limit = 3 >
    typename Fun::T implement( Fun fun = Fun(), int limit = 0 )
    {
        flags.push_back( MemoryFlags() );
        auto i = instruction.values.begin(), e = limit ? i + limit : instruction.values.end();
        result.push_back( instruction.result() );
        return implement( wibble::Preferred(), i, e, Nil(), fun );
    }

    template< typename Fun >
    typename Fun::T _withValues( Fun fun ) {
        flags.push_back( MemoryFlags() );
        return implement< Fun >( wibble::Preferred(), values.begin(), values.end(), Nil(), fun );
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
