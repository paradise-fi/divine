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

#include <divine/llvm-new/interpreter.h>

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
#include <algorithm>
#include <cmath>
#include <wibble/test.h>
#include <wibble/sys/mutex.h>

using namespace llvm;
using namespace divine::llvm2;

template< typename Cons, int N > struct ConsAt;

template< typename Cons >
struct ConsAt< Cons, 0 > {
    typedef typename Cons::Car T;
    static T get( Cons &c ) {
        return c.car;
    }
};

template< typename Cons, int N >
struct ConsAt {
    typedef typename Cons::Cdr Cdr;
    typedef ConsAt< Cdr, N - 1 > CdrAt;

    typedef typename CdrAt::T T;

    static T get( Cons &c ) {
        return CdrAt::get( c.cdr );
    }
};


template< typename A, typename B >
struct Cons {
    typedef A Car;
    typedef B Cdr;
    A car;
    B cdr;

    template< int N >
    auto get() -> typename ConsAt< Cons< A, B >, N >::T
    {
        return ConsAt< Cons< A, B >, N >::get( *this );
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
typename UnPtr< typename ConsAt< Cons, I >::T >::T &decons( Cons &c ) {
    return *c.template get< I >();
}

bool isSignedOp( ProgramInfo::Instruction i ) {
    return true; // XXX
}

template< template< typename > class Fun, typename Cons >
typename Fun< Nil >::T Interpreter::implement( ProgramInfo::Instruction i, Cons list )
{
    Fun< Cons > instance;
    instance._interpreter = this;
    instance.instruction = i;
    return instance( list );
}

template< template< typename > class Fun, typename Cons, typename... Args >
typename Fun< Nil >::T Interpreter::implement( ProgramInfo::Instruction i, Cons list,
                                               std::pair< Type *, char * > arg, Args... args )
{
    Type *ty = arg.first;
    int width = TD.getTypeAllocSize( ty );

    if ( ty->isIntegerTy() ) {
        if ( isSignedOp( i ) ) {
            switch ( width ) {
                case  8: return implement< Fun >( i, consPtr<  int8_t >( arg.second, list ), args... );
                case 16: return implement< Fun >( i, consPtr< int16_t >( arg.second, list ), args... );
            }
        }
    }

    if ( ty->isFloatTy() )
        return implement< Fun >( i, consPtr< float /* TODO float32_t */ >( arg.second, list ), args... );
    if ( ty->isDoubleTy() )
        return implement< Fun >( i, consPtr< double /* TODO float64_t */ >( arg.second, list ), args... );

    assert_die();
}

template< template< typename > class Fun, typename... Args >
typename Fun< Nil >::T Interpreter::implementN( Args... args )
{
    return implement< Fun >( ProgramInfo::Instruction(), Nil(), args... );
}

template< template< typename > class I >
void Interpreter::implement1( ProgramInfo::Instruction i )
{
    Type *ty = i.op->getOperand(0)->getType();
    implement< I >( i, Nil(),
                    dereference( ty, i.result ),
                    dereference( ty, i.operands[ 0 ] ) );
}

template< template< typename > class I >
void Interpreter::implement2( ProgramInfo::Instruction i )
{
    Type *ty = i.op->getOperand(0)->getType();
    implement< I >( i, Nil(),
                    dereference( ty, i.result ),
                    dereference( ty, i.operands[ 0 ] ),
                    dereference( ty, i.operands[ 1 ] ) );
}

template< template< typename > class I >
void Interpreter::implement3( ProgramInfo::Instruction i )
{
    Type *ty = i.op->getOperand(0)->getType();
    implement< I >( i, Nil(),
                    dereference( ty, i.result ),
                    dereference( ty, i.operands[ 0 ] ),
                    dereference( ty, i.operands[ 1 ] ),
                    dereference( ty, i.operands[ 2 ] ) );
}

struct Implementation {
    typedef void T;
    ProgramInfo::Instruction instruction;
    Interpreter *_interpreter;
    ProgramInfo::Instruction i() { return instruction; }
    Interpreter &interpreter() { return *_interpreter; }
    TargetData &TD() { return interpreter().TD; }
};

template< typename Cons >
struct Arithmetic : Implementation {

    void doit( wibble::NotPreferred, Cons &list ) {
        assert_die();
    }

    template< typename X >
    static void check( X ) {}

    template< typename X >
    auto doit( wibble::Preferred, X &list ) -> decltype( check( decons< 1 >( list ) % decons< 2 >( list ) ) ) {
        auto &r = decons< 0 >( list );
        auto &a = decons< 1 >( list );
        auto &b = decons< 2 >( list );

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

    template< typename X >
    void operator()( X &list ) {
        return doit( wibble::Preferred(), list );
    }
};

template< typename Cons >
struct Select : Implementation {
    void operator()( Cons &l ) {
        decons< 0 >( l ) = ( decons< 1 >( l ) ? decons< 3 >( l ) : decons< 2 >( l ) );
    }
};

template< typename Cons >
struct ICmp : Implementation {
    void operator()( Cons &list ) {
        auto &r = decons< 0 >( list );
        auto &a = decons< 1 >( list );
        auto &b = decons< 2 >( list );

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
        }
        assert_die();
    }
};

template< typename Cons >
struct FCmp : Implementation {
    void operator()( Cons &list ) {

        auto &r = decons< 0 >( list );
        auto &a = decons< 1 >( list );
        auto &b = decons< 2 >( list );

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
        }
        assert_die();
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

template< typename Cons >
struct Copy : Implementation {
    void operator()( Cons &l ) {
        decons< 0 >( l ) = decons< 1 >( l );
    }
};

template< typename Cons >
struct BitCast : Implementation {
    void operator()( Cons &l ) {
        char *from = reinterpret_cast< char * >( &decons< 1 >( l ) );
        char *to = reinterpret_cast< char * >( &decons< 0 >( l ) );
        assert_eq( sizeof( decons< 0 >( l ) ), sizeof( decons< 1 >( l ) ) );
        std::copy( from, from + sizeof( decons< 0 >( l ) ), to );
    }
};

template< typename Cons >
struct IsTrue : Implementation {
    typedef bool T;
    bool operator()( Cons &l ) {
        return decons< 0 >( l );
    }
};

template< typename _T >
struct Get {
    template< typename Cons >
    struct I : Implementation {
        typedef _T T;
        T operator()( Cons &l ) { /* kinda evil */
            return static_cast< T >( decons< 0 >( l ) );
        };
    };
};

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
            switchBB( II->getNormalDest() );
    }

    leaveFrame(); /* Actually pop the stack. */
}

void Interpreter::visitReturnInst(ReturnInst &I) {
    if ( I.getNumOperands() )
        leaveFrame( I.getReturnValue()->getType(), instruction().operands[ 0 ] );
    else
        leaveFrame();
}

void Interpreter::visitBranchInst(BranchInst &I)
{
    if ( I.isUnconditional() )
        switchBB( I.getSuccessor( 0 ) );
    else {
        if ( implementN< IsTrue >( dereferenceOperand( instruction(), 0 ) ) )
            switchBB( I.getSuccessor( 0 ) );
        else
            switchBB( I.getSuccessor( 1 ) );
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
    Pointer target = implementN< Get< Pointer >::I >( dereferenceOperand( instruction(), 0 ) );
    assert_die(); // switchBB( dereferenceBB( target ) );
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
void Interpreter::switchBB( BasicBlock *Dest )
{
    pc() = info.pcmap[ &*(Dest->begin()) ]; /* jump! */

    if ( !isa<PHINode>( Dest->begin() ) ) return;  // Nothing fancy to do

    assert_die();
#if 0
    // Loop over all of the PHI nodes in the current block, reading their inputs.
    std::vector<GenericValue> ResultValues;

    for (; PHINode *PN = dyn_cast<PHINode>(next.insn); ++next.insn) {
        // Search for the value corresponding to this previous bb...
        int i = PN->getBasicBlockIndex(previous.block);
        assert(i != -1 && "PHINode doesn't contain entry for predecessor??");
        Value *IncomingValue = PN->getIncomingValue(i);

        // Save the incoming value for this PHI node...
        ResultValues.push_back(getOperandValue(IncomingValue, SF));
    }

    // Now loop over all of the PHI nodes setting their values...
    next.insn = next.block->begin();
    for (unsigned i = 0; isa<PHINode>(next.insn); ++next.insn, ++i) {
        PHINode *PN = cast<PHINode>(next.insn);
        SetValue(PN, ResultValues[i], SF);
    }
    setLocation( SF, next );
#endif
}

//===----------------------------------------------------------------------===//
//                     Memory Instruction Implementations
//===----------------------------------------------------------------------===//

void Interpreter::visitAllocaInst(AllocaInst &I) {
    Type *ty = I.getType()->getElementType();  // Type to be allocated

    int count = implementN< Get< int >::I >( dereferenceOperand( instruction(), 0 ) );
    int size = TD.getTypeAllocSize(ty);

    // Avoid malloc-ing zero bytes, use max()...
    unsigned alloc = std::max( 1, count * size );

    assert_eq( alloc, 0 ); // bang. To be implemented.

#if 0
    // Allocate enough memory to hold the type...
    Arena::Index Memory = arena.allocate(alloc);

    GenericValue Result;
    Result.PointerVal = reinterpret_cast< void * >( intptr_t( Memory ) );
    Result.IntVal = APInt(2, 0); // XXX, not very clean; marks an alloca for cloning by detach()
    assert(Result.PointerVal != 0 && "Null pointer returned by malloc!");
    SetValue(&I, Result, SF());

    if (I.getOpcode() == Instruction::Alloca)
        SF().allocas.push_back(Memory);
#endif
}

template< typename Cons >
struct GetElement : Implementation {
    void operator()( Cons &c ) {
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
                int index = interpreter().template implementN< Get< int >::I >(
                    interpreter().dereferenceOperand( i(), meh ) );
                total += index * TD().getTypeAllocSize( ST->getElementType() );
            }
        }

        /* decons< 0 > has to be a Pointer */
        decons< 0 >( c ) = decons< 1 >( c ) + total;
    }
};

void Interpreter::visitGetElementPtrInst(GetElementPtrInst &I) {
    assert(I.getPointerOperand()->getType()->isPointerTy() &&
           "Cannot getElementOffset of a nonpointer type!");
    implement2< GetElement >( instruction() );
}

template< typename Cons >
struct Load : Implementation {
    void operator()( Cons &c ) {
        assert_die(); /*
        decons< 0 >( c ) = *reinterpret_cast< decltype( &decons< 0 >( c ) ) >(
            interpreter().dereferencePointer( decons< 1 >( c ) ) );
                      */
    }
};

template< typename Cons >
struct Store : Implementation {
    void operator()( Cons &c ) {
        assert_die(); /*
        *reinterpret_cast< decltype( &decons< 1 >( c ) ) >(
            interpreter().dereferencePointer( decons< 0 >( c ) ) ) = decons< 1 >( c );
                      */
    }
};

void Interpreter::visitLoadInst(LoadInst &I) {
    implement1< Load >( instruction() );
}

void Interpreter::visitStoreInst(StoreInst &I) {
    implement1< Store >( instruction() );
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

    if ( F && F->isDeclaration() )
        switch (F->getIntrinsicID()) {
            case Intrinsic::not_intrinsic:
                break;
            case Intrinsic::vastart: { // va_start
                assert_die(); /*
                GenericValue ArgIndex;
                ArgIndex.UIntPairVal.first = stack().size() - 1;
                ArgIndex.UIntPairVal.second = 0;
                SetValue(CS.getInstruction(), ArgIndex, SF()); */
                return;
            }
            // noops
            case Intrinsic::dbg_declare:
            case Intrinsic::dbg_value:
                return;
            case Intrinsic::trap:
                assert_die();
                /* while (!stack().empty()) // get us out
                    leave(); */
                return;

            case Intrinsic::vaend:    // va_end is a noop for the interpreter
                return;
            case Intrinsic::vacopy:   // va_copy: dest = src
                assert_die();
                // SetValue(CS.getInstruction(), getOperandValue(*CS.arg_begin(), SF()), SF());
                return;
            default:
                // If it is an unknown intrinsic function, use the intrinsic lowering
                // class to transform it into hopefully tasty LLVM code.
                //
                // dbgs() << "FATAL: Can't lower:" << *CS.getInstruction() << "\n";
                assert_die(); /* TODO: the new locations need to be indexed */
                /* BasicBlock::iterator me(CS.getInstruction());
                BasicBlock *Parent = CS.getInstruction()->getParent();
                bool atBegin(Parent->begin() == me);
                if (!atBegin)
                    --me;
                IL->LowerIntrinsicCall(cast<CallInst>(CS.getInstruction()));

                // Restore the CurInst pointer to the first instruction newly inserted, if
                // any.
                if (atBegin) {
                    setInstruction( SF(), Parent->begin() );
                } else {
                    BasicBlock::iterator me_next = me;
                    ++ me_next;
                    setInstruction( SF(), me_next );
                }
                return; */
        }

    // Special handling for external functions.
    if (F->isDeclaration()) {
        /* This traps into the "externals": functions that may be provided by
         * our own runtime (these may be nondeterministic), or, possibly (TODO)
         * into external, native library code  */
        assert_die();
    }

    int functionid = info.functionmap[ F ];
    state.enter( functionid );

    ProgramInfo::Function function = info.function( pc() );

    for ( int i = 0; i < CS.arg_size(); ++i )
    {
        Type *ty = CS.getArgument( i )->getType();
        implementN< Copy >( dereference( ty, function.values[ i ] ),
                            dereferenceOperand( insn, i + 1 ) );
    }

    assert_die();
#if 0
    Location loc = location( SF() );
    loc.insn = CS.getInstruction();
    SF().caller = locationIndex.left( loc );
    std::vector<GenericValue> ArgVals;
    const unsigned NumArgs = CS.arg_size();
    ArgVals.reserve(NumArgs);
    uint16_t pNum = 1;
    for (CallSite::arg_iterator i = CS.arg_begin(),
                                e = CS.arg_end(); i != e; ++i, ++pNum) {
        Value *V = *i;
        ArgVals.push_back(getOperandValue(V, SF()));
    }

    // To handle indirect calls, we must get the pointer value from the argument
    // and treat it as a function pointer.
    GenericValue SRC = getOperandValue(CS.getCalledValue(), SF());
    Function *fun = functionIndex.right(int(intptr_t(GVTOP(SRC))));
    callFunction(fun, ArgVals);
#endif
}

// FPTo*I should round

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
    implement2< Copy >( instruction() ); // XXX rounding?
}

void Interpreter::visitFPToSIInst(FPToSIInst &I) {
    implement2< Copy >( instruction() ); // XXX rounding?
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

