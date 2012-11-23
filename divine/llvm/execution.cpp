//===-- Execution.cpp - Implement code to simulate the program ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file contains the actual instruction interpreter.
//
//===----------------------------------------------------------------------===//

#define NO_RTTI
#include <wibble/exception.h>
#include <divine/llvm/interpreter.h>

#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/LLVMContext.h"
#include <algorithm>
#include <cmath>
#include <wibble/test.h>
#include <wibble/sys/mutex.h>

using namespace llvm;
using namespace divine::llvm;

struct Nil {};

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

template< typename A, typename B >
struct Cons {
    typedef A Car;
    typedef B Cdr;
    A car;
    B cdr;

    template< int N >
    auto get() -> decltype( ConsAt< N >::get( *this ) )
    {
        return ConsAt< N >::get( *this );
    }
};

template< typename A, typename B >
Cons< A, B > cons( A a, B b ) {
    Cons< A, B > r = { .car = a, .cdr = b };
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
auto decons( Cons &c ) -> typename UnPtr< decltype( ConsAt< I >::get( c ) ) >::T &
{
    return *c.template get< I >();
}

#define MATCH(expr...) template< typename F, typename X > \
    auto match( wibble::Preferred, F f, X &&x ) -> decltype( f( expr ) ) { return f( expr ); }

MATCH( decons< 0 >( x ) )
MATCH( decons< 1 >( x ), decons< 0 >( x ) )
MATCH( decons< 2 >( x ), decons< 1 >( x ), decons< 0 >( x ) )
MATCH( decons< 3 >( x ), decons< 2 >( x ), decons< 1 >( x ), decons< 0 >( x ) )

#undef MATCH

template< typename F, typename X >
typename F::T match( wibble::NotPreferred, F f, X &&x )
{
    assert_die();
}

bool isSignedOp( ProgramInfo::Instruction i ) {
    return true; // XXX
}

template< typename Fun, typename Cons >
typename Fun::T Interpreter::implement( ProgramInfo::Instruction i, Cons list )
{
    Fun instance;
    instance._interpreter = this;
    instance.instruction = i;
    return match( wibble::Preferred(), instance, list );
}

template< typename Fun, typename Cons, typename... Args >
typename Fun::T Interpreter::implement( ProgramInfo::Instruction i, Cons list,
                                        std::pair< Type *, char * > arg, Args... args )
{
    Type *ty = arg.first;
    int width = TD.getTypeAllocSize( ty ); /* bytes */

    if ( ty->isVoidTy() )
        return implement< Fun >( i, list , args... ); /* we entirely ignore void items */

    if ( ty->isIntegerTy() ) {
        if ( isSignedOp( i ) ) {
            switch ( width ) {
                case 1: return implement< Fun >( i, consPtr<  int8_t >( arg.second, list ), args... );
                case 2: return implement< Fun >( i, consPtr< int16_t >( arg.second, list ), args... );
                case 4: return implement< Fun >( i, consPtr< int32_t >( arg.second, list ), args... );
                case 8: return implement< Fun >( i, consPtr< int64_t >( arg.second, list ), args... );
            }
        }
    }

    if ( ty->isPointerTy() )
        return implement< Fun >( i, consPtr< Pointer >( arg.second, list ), args... );

    if ( ty->isFloatTy() )
        return implement< Fun >( i, consPtr< float /* TODO float32_t */ >( arg.second, list ), args... );
    if ( ty->isDoubleTy() )
        return implement< Fun >( i, consPtr< double /* TODO float64_t */ >( arg.second, list ), args... );
    assert_die();
}

template< typename Fun, typename... Args >
typename Fun::T Interpreter::implementN( Args... args )
{
    return implement< Fun >( ProgramInfo::Instruction(), Nil(), args... );
}

template< typename I >
void Interpreter::implement1( ProgramInfo::Instruction i )
{
    implement< I >( i, Nil(),
                    dereferenceResult( i ),
                    dereferenceOperand( i, 0 ) );
}

template< typename I >
void Interpreter::implement2( ProgramInfo::Instruction i )
{
    implement< I >( i, Nil(),
                    dereferenceResult( i ),
                    dereferenceOperand( i, 0 ),
                    dereferenceOperand( i, 1 ) );
}

template< typename I >
void Interpreter::implement3( ProgramInfo::Instruction i )
{
    implement< I >( i, Nil(),
                    dereferenceResult( i ),
                    dereferenceOperand( i, 0 ),
                    dereferenceOperand( i, 1 ),
                    dereferenceOperand( i, 2 ) );
}

struct Implementation {
    typedef void T;
    ProgramInfo::Instruction instruction;
    Interpreter *_interpreter;
    ProgramInfo::Instruction i() { return instruction; }
    Interpreter &interpreter() { return *_interpreter; }
    TargetData &TD() { return interpreter().TD; }
};

template< typename... X >
static void declcheck( X... ) {}

struct Arithmetic : Implementation {
    template< typename X >
    auto operator()( X &r, X &a, X &b ) -> decltype( declcheck( a % b ) )
    {
        switch( i().op->getOpcode() ) {
            case Instruction::FAdd:
            case Instruction::Add: r = a + b; return;
            case Instruction::FSub:
            case Instruction::Sub: r = a - b; return;
            case Instruction::FMul:
            case Instruction::Mul: r = a * b; return;
            case Instruction::FDiv:
            case Instruction::SDiv:
            case Instruction::UDiv: r = a / b; return;
            case Instruction::FRem: r = std::fmod( a, b ); return;
            case Instruction::URem:
            case Instruction::SRem: r = a % b; return;
            case Instruction::And:  r = a & b; return;
            case Instruction::Or:   r = a | b; return;
            case Instruction::Xor:  r = a ^ b; return;
            case Instruction::Shl:  r = a << b; return;
            case Instruction::AShr:  // XXX?
            case Instruction::LShr:  r = a >> b; return;
        }
        assert_die();
    }
};

struct Select : Implementation {
    template< typename R, typename C >
    auto operator()( R &r, C &a, R &b, R &c ) -> decltype( declcheck( r = a ? b : c ) )
    {
        r = a ? b : c;
    }
};

struct ICmp : Implementation {
    template< typename R, typename X >
    auto operator()( R &r, X &a, X &b ) -> decltype( declcheck( r = a < b ) ) {
        switch (dyn_cast< ICmpInst >( i().op )->getPredicate()) {
            case ICmpInst::ICMP_EQ:  r = a == b; return;
            case ICmpInst::ICMP_NE:  r = a != b; return;
            case ICmpInst::ICMP_ULT:
            case ICmpInst::ICMP_SLT: r = a < b; return;
            case ICmpInst::ICMP_UGT:
            case ICmpInst::ICMP_SGT: r = a > b; return;
            case ICmpInst::ICMP_ULE:
            case ICmpInst::ICMP_SLE: r = a <= b; return;
            case ICmpInst::ICMP_UGE:
            case ICmpInst::ICMP_SGE: r = a >= b; return;
            default: assert_die();
        }
    }
};

struct FCmp : Implementation {
    template< typename R, typename X >
    auto operator()( R &r, X &a, X &b ) ->
        decltype( declcheck( r = isnan( a ) && isnan( b ) ) )
    {
        switch ( dyn_cast< FCmpInst >( i().op )->getPredicate() ) {
            case FCmpInst::FCMP_FALSE: r = false; return;
            case FCmpInst::FCMP_TRUE:  r = true;  return;
            case FCmpInst::FCMP_ORD:   r = !isnan( a ) && !isnan( b ); return;
            case FCmpInst::FCMP_UNO:   r = isnan( a ) || isnan( b );   return;

            case FCmpInst::FCMP_UEQ:
            case FCmpInst::FCMP_UNE:
            case FCmpInst::FCMP_UGE:
            case FCmpInst::FCMP_ULE:
            case FCmpInst::FCMP_ULT:
            case FCmpInst::FCMP_UGT:
                if ( isnan( a ) || isnan( b ) ) {
                    r = true;
                    return;
                }
                break;
            default: assert_die();
        }

        switch ( dyn_cast< FCmpInst >( i().op )->getPredicate() ) {
            case FCmpInst::FCMP_OEQ:
            case FCmpInst::FCMP_UEQ: r = a == b; return;
            case FCmpInst::FCMP_ONE:
            case FCmpInst::FCMP_UNE: r = a != b; return;

            case FCmpInst::FCMP_OLT:
            case FCmpInst::FCMP_ULT: r = a < b; return;

            case FCmpInst::FCMP_OGT:
            case FCmpInst::FCMP_UGT: r = a > b; return;

            case FCmpInst::FCMP_OLE:
            case FCmpInst::FCMP_ULE: r = a <= b; return;

            case FCmpInst::FCMP_OGE:
            case FCmpInst::FCMP_UGE: r = a >= b; return;
            default: assert_die();
        }
    }
};

/* Bit lifting. */

void Interpreter::visitFCmpInst(FCmpInst &) {
    implement2< FCmp >( instruction() );
}

void Interpreter::visitICmpInst(ICmpInst &) {
    implement2< ICmp >( instruction() );
}

void Interpreter::visitBinaryOperator(BinaryOperator &) {
    implement2< Arithmetic >( instruction() );
}

void Interpreter::visitSelectInst(SelectInst &) {
    implement3< Select >( instruction() );
}

/* Control flow. */

struct Copy : Implementation {
    template< typename X >
    void operator()( X &r, X &l )
    {
        r = l;
    }
};

struct BitCast : Implementation {
    template< typename R, typename L >
    void operator()( R &r, L &l ) {
        char *from = reinterpret_cast< char * >( &l );
        char *to = reinterpret_cast< char * >( &r );
        assert_eq( sizeof( R ), sizeof( L ) );
        std::copy( from, from + sizeof( R ), to );
    }
};

template< typename _T >
struct Get : Implementation {
    typedef _T T;

    template< typename X >
    auto operator()( X &l ) -> decltype( static_cast< T >( l ) )
    {
        return static_cast< T >( l );
    }
};

typedef Get< bool > IsTrue;

void Interpreter::leaveFrame()
{
    state.leave();
}

void Interpreter::leaveFrame( Type *ty, ProgramInfo::Value result ) {

    /* Handle the return value first. */
    if ( state.stack().get().length() == 1 ) {
        /* TODO handle exit codes (?) */
    } else {
        /* Find the call instruction we are going back to. */
        auto i = info.instruction( state.frame( -1, 1 ).pc );
        /* Copy the return value. */
        implementN< Copy >( dereference( ty, i.result, -1 , 1 ),
                            dereference( ty, result ) );

        /* If this was an invoke, run the non-error target. */
        if ( InvokeInst *II = dyn_cast< InvokeInst >( i.op ) )
            jumpTo( instruction().operands[ instruction().operands.size() - 2 ] );
    }

    leaveFrame(); /* Actually pop the stack. */
}

void Interpreter::visitReturnInst(ReturnInst &I) {
    if ( I.getNumOperands() )
        leaveFrame( I.getReturnValue()->getType(), instruction().operands[ 0 ] );
    else
        leaveFrame();
}

void Interpreter::jumpTo( ProgramInfo::Value v )
{
    PC to = *reinterpret_cast< PC * >( state.dereference( v ) );
    jumpTo( to );
}

void Interpreter::jumpTo( PC to )
{
    if ( pc().function != to.function )
        throw wibble::exception::Consistency(
            "Interpreter::checkJump",
            "Can't deal with cross-function jumps." );
    switchBB( to );
}

void Interpreter::visitBranchInst(BranchInst &I)
{
    if ( I.isUnconditional() )
        jumpTo( instruction().operands[ 0 ] );
    else {
        if ( implementN< IsTrue >( dereferenceOperand( instruction(), 0 ) ) )
            jumpTo( instruction().operands[ 2 ] );
        else
            jumpTo( instruction().operands[ 1 ] );
    }
}

void Interpreter::visitSwitchInst(SwitchInst &I) {
    assert_die();
#if 0
    GenericValue CondVal = getOperandValue(I.getOperand(0), SF());
    Type *ElTy = I.getOperand(0)->getType();

    // Check to see if any of the cases match...
    BasicBlock *Dest = 0;
    for (unsigned i = 2, e = I.getNumOperands(); i != e; i += 2)
        if (executeICMP_EQ(CondVal, getOperandValue(I.getOperand(i), SF()), ElTy)
            .IntVal != 0) {
            Dest = cast<BasicBlock>(I.getOperand(i+1));
            break;
        }

    if (!Dest) Dest = I.getDefaultDest();   // No cases matched: use default
    SwitchToNewBasicBlock(Dest, SF());
#endif
}

void Interpreter::visitIndirectBrInst(IndirectBrInst &I) {
    Pointer target = implementN< Get< Pointer > >( dereferenceOperand( instruction(), 0 ) );
    jumpTo( *reinterpret_cast< PC * >( dereferencePointer( target ) ) );
}

// SwitchToNewBasicBlock - This method is used to jump to a new basic block.
// This function handles the actual updating of block and instruction iterators
// as well as execution of all of the PHI nodes in the destination block.
//
// This method does this because all of the PHI nodes must be executed
// atomically, reading their inputs before any of the results are updated.  Not
// doing this can cause problems if the PHI nodes depend on other PHI nodes for
// their inputs.  If the input PHI node is updated before it is read, incorrect
// results can happen.  Thus we use a two phase approach.
//
void Interpreter::switchBB( PC target )
{
    jumped = true;
    PC origin = pc();
    pc() = target;

    assert( instruction().op );

    if ( !isa<PHINode>( instruction().op ) )
        return;  // Nothing fancy to do

    MachineState::Frame &original = state.frame();
    int framesize = original.framesize( info );
    Blob tmp( sizeof( MachineState::Frame ) + framesize );
    MachineState::Frame &copy = tmp.get< MachineState::Frame >();
    copy = state.frame();

    std::copy( original.memory, original.memory + framesize, copy.memory );
    while ( PHINode *PN = dyn_cast<PHINode>(instruction().op) ) {
        /* TODO use operands directly, avoiding valuemap lookup */
        auto v = info.valuemap[ PN->getIncomingValueForBlock( info.block( origin ).bb ) ];
        char *value = ( v.global || v.constant ) ?
                          state.dereference( v ) : original.dereference( info, v );
        char *result = copy.dereference( info, instruction().result );
        std::copy( value, value + v.width, result );
        advance();
    }
    std::copy( copy.memory, copy.memory + framesize, original.memory );
}

//===----------------------------------------------------------------------===//
//                     Memory Instruction Implementations
//===----------------------------------------------------------------------===//

void Interpreter::visitAllocaInst(AllocaInst &I) {
    Type *ty = I.getType()->getElementType();  // Type to be allocated

    int count = implementN< Get< int > >( dereferenceOperand( instruction(), 0 ) );
    int size = TD.getTypeAllocSize(ty);

    // Avoid malloc-ing zero bytes, use max()...
    unsigned alloc = std::max( 1, count * size );
    Pointer &p = *reinterpret_cast< Pointer * >( state.dereference( instruction().result ) );
    p = state.nursery.malloc( alloc );
}

struct GetElement : Implementation {
    void operator()( Pointer &r, Pointer &p ) {
        int total = 0;

        gep_type_iterator I = gep_type_begin( i().op ),
                          E = gep_type_end( i().op );

        int meh = 1;
        for (; I != E; ++I, ++meh) {
            if (StructType *STy = dyn_cast<StructType>(*I)) {
                const StructLayout *SLO = TD().getStructLayout(STy);
                const ConstantInt *CPU = cast<ConstantInt>(I.getOperand()); /* meh */
                int index = CPU->getZExtValue();
                total += SLO->getElementOffset( index );
            } else {
                const SequentialType *ST = cast<SequentialType>(*I);
                int index = interpreter().implementN< Get< int > >(
                    interpreter().dereferenceOperand( i(), meh ) );
                total += index * TD().getTypeAllocSize( ST->getElementType() );
            }
        }

        r = p + total;
    }
};

void Interpreter::visitGetElementPtrInst(GetElementPtrInst &I) {
    implement2< GetElement >( instruction() );
}

struct Load : Implementation {
    template< typename R >
    void operator()( R &r, Pointer p ) {
        r = *reinterpret_cast< R * >( interpreter().dereferencePointer( p ) );
    }
};

struct Store : Implementation {
    template< typename L >
    void operator()( L &l, Pointer p ) {
        *reinterpret_cast< L * >( interpreter().dereferencePointer( p ) ) = l;
    }
};

void Interpreter::visitLoadInst(LoadInst &I) {
    implement1< Load >( instruction() );
}

void Interpreter::visitStoreInst(StoreInst &I) {
    implement2< Store >( instruction() );
}

//===----------------------------------------------------------------------===//
//                 Miscellaneous Instruction Implementations
//===----------------------------------------------------------------------===//

void Interpreter::visitFenceInst(FenceInst &I) {
    // do nothing for now
}

void Interpreter::visitCallSite(CallSite CS) {
    ProgramInfo::Instruction insn = instruction();
    // Check to see if this is an intrinsic function call...
    Function *F = CS.getCalledFunction();

    if ( F && F->isDeclaration() ) {

        switch (F->getIntrinsicID()) {
            case Intrinsic::not_intrinsic: break;
            case Intrinsic::vastart:
            case Intrinsic::trap:
            case Intrinsic::vaend:
            case Intrinsic::vacopy:
                assert_die(); /* TODO */
            default:
                assert_die(); /* We lowered everything in buildInfo. */
        }

        switch( insn.builtin ) {
            case NotBuiltin: break;
            case BuiltinChoice:
                choice = implementN< Get< int > >(
                    dereferenceOperand( instruction(), 0 ) );
                return;
            case BuiltinMask: state.frame().pc.masked = true; return;
            case BuiltinUnmask: state.frame().pc.masked = false; return;
            case BuiltinGetTID: assert_die(); /* TODO */
            case BuiltinNewThread:
                Pointer entry = implementN< Get< Pointer > >( dereferenceOperand( instruction(), 0 ) );
                /* As far as LLVM is concerned, entry is a Pointer, but in fact it's a PC. */
                new_thread( *reinterpret_cast< PC * >( &entry ) );
                return; /* TODO result! */
        }

    }

    assert ( !F->isDeclaration() );

    /* TODO (performance) Use an operand Value here instead. */
    int functionid = info.functionmap[ F ];
    state.enter( functionid ); /* push a new frame */
    jumped = true;

    /* Copy arguments to the new frame. */
    ProgramInfo::Function function = info.function( pc() );
    for ( int i = 0; i < CS.arg_size(); ++i )
    {
        Type *ty = CS.getArgument( i )->getType();
        implementN< Copy >( dereference( ty, function.values[ i ] ),
                            dereferenceOperand( insn, i, 1 ) );
    }

    /* TODO varargs */

    assert( !isa<PHINode>( instruction().op ) );
}

void Interpreter::visitTruncInst(TruncInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitSExtInst(SExtInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitZExtInst(ZExtInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitFPTruncInst(FPTruncInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitFPExtInst(FPExtInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitUIToFPInst(UIToFPInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitSIToFPInst(SIToFPInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitFPToUIInst(FPToUIInst &I) {
    implement2< Copy >( instruction() ); // XXX rounding!
}

void Interpreter::visitFPToSIInst(FPToSIInst &I) {
    implement2< Copy >( instruction() ); // XXX rounding!
}

void Interpreter::visitPtrToIntInst(PtrToIntInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitIntToPtrInst(IntToPtrInst &I) {
    implement2< Copy >( instruction() );
}

void Interpreter::visitBitCastInst(BitCastInst &I) {
    implement2< BitCast >( instruction() ); // XXX?
}

void Interpreter::choose( int32_t result )
{
    assert_eq( instruction().builtin, BuiltinChoice );
    char *result_mem = reinterpret_cast< char * >( &result );
    Type *result_type = Type::getInt32Ty( ::llvm::getGlobalContext() );
    implementN< Copy >( dereferenceResult( instruction() ),
                        std::make_pair( result_type, result_mem ) );
}
