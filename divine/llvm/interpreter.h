//===-- Interpreter.h ------------------------------------------*- C++ -*--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header file defines the interpreter structure
//
//===----------------------------------------------------------------------===//

#ifndef DIVINE_LLVM_INTERPRETER_H
#define DIVINE_LLVM_INTERPRETER_H

#define NO_RTTI

#include <divine/llvm/program.h>
#include <divine/llvm/machine.h>
#include <divine/graph/allocator.h> // hmm.

#include "llvm/Function.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"


namespace llvm {

class IntrinsicLowering;
struct FunctionInfo;
template<typename T> class generic_gep_type_iterator;
class ConstantExpr;
typedef generic_gep_type_iterator<User::const_op_iterator> gep_type_iterator;

}

namespace divine {
namespace llvm {

struct Interpreter;

using namespace ::llvm;

// Interpreter - This class represents the entirety of the interpreter.
//
class Interpreter : public ::llvm::ExecutionEngine, public ::llvm::InstVisitor<Interpreter>
{
    IntrinsicLowering *IL;
    friend ProgramInfo; // for getConstantValue

public:
    TargetData TD;

    std::map< std::string, std::string > properties;
    ::llvm::Module *module; /* The bitcode. */
    MachineState state; /* the state we are dealing with */
    ProgramInfo info;
    bool jumped, observable;
    int choice;

    Allocator &alloc;

    void parseProperties( Module *M );
    void buildInfo( Module *M );

    std::pair< Type *, char * > dereference(
        Type *t, ProgramInfo::Value v, int tid = -1, int frame = 0 )
    {
        char *mem = state.dereference( v, tid, frame );
        return std::make_pair( t, mem );
    }

    std::pair< Type *, char * > dereferenceOperand(
        const ProgramInfo::Instruction &i, int n, int frame = 0 )
    {
        Type *t = i.op->getOperand( n )->getType();
        return dereference( t, i.operands[ n ], -1, frame );
    }

    std::pair< Type *, char * > dereferenceResult( const ProgramInfo::Instruction &i )
    {
        if ( i.result.width )
            return dereference( i.op->getType(), i.result );
        else
            return std::make_pair( i.op->getType(), nullptr );
    }

    char * dereferencePointer( Pointer p ) { return state.dereference( p ); }
    BasicBlock *dereferenceBB( Pointer );

    template< typename Fun, typename Cons >
    typename Fun::T implement( ProgramInfo::Instruction i, Cons list );

    template< typename Fun, typename Cons, typename... Args >
    typename Fun::T implement( ProgramInfo::Instruction i, Cons list,
                                      std::pair< Type *, char * > arg, Args... args );

    template< typename Fun, typename... Args >
    typename Fun::T implementN( Args... args );

    template< typename I >
    void implement1( ProgramInfo::Instruction i );

    template< typename I >
    void implement2( ProgramInfo::Instruction i );

    template< typename I >
    void implement3( ProgramInfo::Instruction i );

    // the currently executing one, i.e. what pc of the top frame of the active thread points at
    ProgramInfo::Instruction instruction() { return info.instruction( pc() ); }
    PC &pc() { return state._frame->pc; }

    Blob detach() {
        // make a copy of state.
        assert_die();
    }

    MDNode *node( MDNode *root ) { return root; }

    template< typename N, typename... Args >
    MDNode *node( N *root, int n, Args... args ) {
        if (root)
            return node( cast< MDNode >( root->getOperand(n) ), args... );
        return NULL;
    }

    MDNode *findEnum( std::string lookup ) {
        assert( module );
        MDNode *enums = node( module->getNamedMetadata( "llvm.dbg.cu" ), 0, 10, 0 );
        if ( !enums )
            return NULL;
        for ( int i = 0; i < enums->getNumOperands(); ++i ) {
            MDNode *n = cast< MDNode >( enums->getOperand(i) );
            MDString *name = cast< MDString >( n->getOperand(2) );
            if ( name->getString() == lookup )
                return cast< MDNode >( n->getOperand(10) ); // the list of enum items
        }
        return NULL;
    }

    GenericValue constantGV( const Constant *x ) { return getConstantValue( x ); }

    explicit Interpreter(Allocator &a, Module *M);
    ~Interpreter();

    typedef std::pair< std::string, char * > Describe;
    typedef std::set< std::pair< Pointer, Type * > > DescribeSeen;

    Describe describeAggregate( Type *t, char *where, DescribeSeen& );
    Describe describeValue( Type *t, char *where, DescribeSeen& );
    std::string describePointer( Type *t, Pointer p, DescribeSeen& );
    std::string describeValue( const ::llvm::Value *, int thread,
                               DescribeSeen * = nullptr,
                               int *anonymous = nullptr,
                               std::vector< std::string > *container = nullptr );
    std::string describe( bool detailed = false );

    Blob initial( Function *f ); /* Make an initial state from Function. */
    void new_thread( Function *f );
    void rewind( Blob b ) { state.rewind( b, 0 ); }
    void choose( int32_t i );

    void advance() {
        pc().instruction ++;
        if ( !instruction().op ) {
            PC to = pc();
            to.block ++;
            to.instruction = 0;
            switchBB( to );
        }
    }

    template< typename Yield >
    void run( Blob b, Yield yield ) {
        state.rewind( b, -1 ); int tid = 0;
        while ( state._thread_count ) {
            run( tid, yield );
            if ( ++tid == state._thread_count )
                break;
            state.rewind( b, -1 );
        }
    }

    template< typename Yield >
    void run( int tid, Yield yield ) {
        std::set< PC > seen;

        if ( !state._thread_count )
            return; /* no more successors for you */

        assert_leq( tid, state._thread_count - 1 );
        if ( state._thread != tid )
            state.switch_thread( tid );
        assert( state.stack().get().length() );

        while ( true ) {
            seen.insert( pc() );
            state.flags().assert = false;
            observable = jumped = false;
            choice = 0;

            visit( instruction().op );

            if ( !state.stack().get().length() )
                break; /* this thread is done */

            if ( choice ) {
                assert( !jumped );
                Blob fork = state.snapshot();
                int limit = choice; /* make a copy, sublings must overwrite the original */
                for ( int i = 0; i < limit; ++i ) {
                    state.rewind( fork, tid );
                    choose( i );
                    advance();
                    run( tid, yield );
                }
                fork.free( alloc.pool() );
                return;
            }

            if ( !jumped )
                advance();

            if ( ( observable && !pc().masked ) || seen.count( pc() ) ) {
                yield( state.snapshot() );
                return;
            }
        }

        yield( state.snapshot() );
    }

    /* Flow control. */
    void visitReturnInst(ReturnInst &I);
    void visitBranchInst(BranchInst &I);
    void visitSwitchInst(SwitchInst &I);
    void visitIndirectBrInst(IndirectBrInst &I);
    void visitPHINode(PHINode &PN) { assert_die(); }
    void visitCallSite(CallSite CS);
    void visitCallInst(CallInst &I) { visitCallSite (CallSite (&I)); }
    void visitInvokeInst(InvokeInst &I) { visitCallSite (CallSite (&I)); }
    void visitUnreachableInst(UnreachableInst &I) { assert_die(); }
    void visitSelectInst(SelectInst &I);

    /* Bit lifting. */
    void visitBinaryOperator(BinaryOperator &I);
    void visitICmpInst(ICmpInst &I);
    void visitFCmpInst(FCmpInst &I);
    void visitTruncInst(TruncInst &I);
    void visitZExtInst(ZExtInst &I);
    void visitSExtInst(SExtInst &I);
    void visitFPTruncInst(FPTruncInst &I);
    void visitFPExtInst(FPExtInst &I);

    void visitUIToFPInst(UIToFPInst &I);
    void visitSIToFPInst(SIToFPInst &I);
    void visitFPToUIInst(FPToUIInst &I);
    void visitFPToSIInst(FPToSIInst &I);
    void visitPtrToIntInst(PtrToIntInst &I);
    void visitIntToPtrInst(IntToPtrInst &I);
    void visitBitCastInst(BitCastInst &I);

    void visitShl(BinaryOperator &I)  { return visitBinaryOperator( I ); }
    void visitLShr(BinaryOperator &I) { return visitBinaryOperator( I ); }
    void visitAShr(BinaryOperator &I) { return visitBinaryOperator( I ); }

    /* Memory. */
    void visitAllocaInst(AllocaInst &I);
    void visitLoadInst(LoadInst &I);
    void visitStoreInst(StoreInst &I);
    void visitGetElementPtrInst(GetElementPtrInst &I);
    void visitFenceInst(FenceInst &I);
    void visitVAArgInst(VAArgInst &I) { assert_die(); } // TODO

    /* Fallthrough. */
    void visitInstruction(Instruction &I) { assert_die(); }

    void jumpTo( ProgramInfo::Value v );
    void jumpTo( PC target  );
    void switchBB( PC target );
    void leaveFrame(); /* without a return value */
    void leaveFrame( Type *ty, ProgramInfo::Value result );

private:  // Helper functions
    /* The following two are used by getConstantValue */
    void *getPointerToFunction(Function *F) { assert_die(); }
    void *getPointerToNamedFunction(const std::string &, bool) { assert_die(); }
    void *getPointerToBasicBlock(BasicBlock *BB) { assert_die(); }

    void initializeExecutionEngine() { }

private:
    virtual GenericValue runFunction(Function *,
                                     const std::vector<GenericValue> &)
    { assert_die(); }
    virtual void freeMachineCodeForFunction(Function *) { assert_die(); }
    virtual void *recompileAndRelinkFunction(Function*) { assert_die(); }
};

}
}

#endif
