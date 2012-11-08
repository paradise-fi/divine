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

#ifndef LLI_INTERPRETER_H
#define LLI_INTERPRETER_H

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

#define NO_RTTI

#include <divine/toolkit/lens.h>
#include <divine/llvm/arena.h>

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

struct PC {
    uint32_t function:10;
    uint32_t block:12;
    uint32_t instruction:10;
    explicit PC( int f = 0, int b = 0, int i = 0 ) : function( f ), block( b ), instruction( i ) {}
};

struct Pointer {
    uint32_t thread:10;  /* thread id */
    uint32_t address:15; /* pagetable offset */
    Pointer( int i = 0 ) {
        assert_die();
    };
};

struct ProgramInfo {
    TargetData target;
    Interpreter *interpreter;

    struct Value {
        bool global:1;
        bool constant:1;
        uint32_t offset:20;
        uint32_t width:10;
        Value() : global( 0 ), constant( 0 ), offset( 0 ), width( 0 ) {}
    };

    struct Instruction {
        Value result;
        std::vector< Value > operands;

        bool noninterleaved; /* applies to stores */
        llvm::Instruction *op; /* the actual operation */
        /* next instruction is in the same BB unless op == NULL */
    };

    struct BB {
        std::vector< Instruction > instructions;
        Instruction &instruction( PC pc ) {
            assert_leq( pc.instruction, instructions.size() - 1 );
            return instructions[ pc.instruction ];
        }
    };

    struct Function {
        int framesize;
        std::vector< Value > values;
        std::vector< BB > blocks;
        BB &block( PC pc ) {
            assert_leq( pc.block, blocks.size() - 1 );
            return blocks[ pc.block ];
        }
    };

    std::vector< Function > functions;
    std::vector< Value > globals;
    std::vector< char > constdata;
    int globalsize, constdatasize;

    std::map< llvm::Value *, Value > valuemap;
    std::map< llvm::Instruction *, PC > pcmap;

    std::map< llvm::BasicBlock *, int > blockmap;
    std::map< llvm::Function *, int > functionmap;

    template< typename Container >
    static void makeFit( Container &c, int index ) {
        c.resize( std::min( index + 1, int( c.size() ) ) );
    }

    Instruction &instruction( PC pc ) {
        assert_leq( pc.block, function( pc ).blocks.size() - 1 );
        return function( pc ).block( pc ).instruction( pc );
    }

    Function &function( PC pc ) {
        assert_leq( pc.function, functions.size() - 1 );
        return functions[ pc.function ];
    }

    template< typename T >
    T &constant( Value v ) {
        return *reinterpret_cast< T * >( &constdata[ v.offset ] );
    }

    char *allocateConstant( Value &result, int width ) {
        result.offset = constdatasize;
        result.width = width;
        constdatasize += width;
        constdata.resize( constdata.size() + width );
        return &constdata[ result.offset ];
    }

    void storeConstant( Value &result, int v ) {
        allocateConstant( result, sizeof( int ) );
        constant< int >( result ) = v;
    }

    void storeConstant( Value &result, PC pc ) {
        allocateConstant( result, sizeof( PC ) );
        constant< PC >( result ) = pc;
    }

    void storeConstant( Value &result, GenericValue GV, Type *ty );

    void insert( PC pc, llvm::Instruction *i );
    Value insert( int function, llvm::Value *val );
};

struct MachineState {

    struct Frame {
        PC pc;
        char memory[0];
        // TODO varargs
        int _size( ProgramInfo &info ) {
            return sizeof( Frame ) + info.function(pc).framesize;
        }
        int end() { return 0; }
    };

    struct Globals {
        char memory[0];
        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, a._info->globalsize );
        }
        int end() { return 0; }
    };

    Blob _blob, _stack, _heap;
    ProgramInfo &_info;
    int _slack;
    int _thread; /* the currently scheduled thread */
    Frame *_frame; /* pointer to the currently active frame */
    int _size_difference;

    template< typename T >
    using Lens = lens::Lens< lens::LinearAddress, T >;

    struct Flags {
        uint64_t assert:1;
        uint64_t null_dereference:1;
        uint64_t invalid_dereference:1;
        uint64_t invalid_argument:1;
        uint64_t ap:48;
        uint64_t buchi:12;
    };

    typedef lens::FixedArray< short > PageTable;
    typedef lens::FixedArray< char > Memory;
    typedef lens::Tuple< PageTable, Memory > Heap;

    typedef lens::Array< Frame > Stack;
    typedef lens::Tuple< Stack, Heap > Thread;
    typedef lens::Array< Thread > Threads;
    typedef lens::Tuple< Flags, Globals, Threads > State;

    Lens< State > state() {
        return Lens< State >( lens::LinearAddress( _blob, _slack ) );
    }

    /*
     * Get a pointer to the value storage corresponding to "v". If "v" is a
     * local variable, look into thread "thread" (default, i.e. -1, for the
     * currently executing one) and in frame "frame" (default, i.e. 0 is the
     * topmost frame, 1 is the frame just below that, etc...).
     */
    std::pair< llvm::Type *, char * > dereference( llvm::Type *t,
                                                   ProgramInfo::Value v,
                                                   int thread = -1,
                                                   int frame = 0 )
    {
        if ( thread < 0 )
            thread = _thread;

        assert_die();
        // return std::make_pair( t, ... );
    }

    void rewind( Blob to, int thread )
    {
        _blob = to;
        _thread = thread;
        _size_difference = 0;
        assert_die();
        // stack( _thread ) = _blob_thread( _thread ).get( Stack() );
        // heap( _thread ) = _blob_thread( _thread ).get( Heap() );
    }

    Lens< Thread > _blob_thread( int i ) {
        return state().sub( Threads(), i );
    }

    Lens< Stack > stack( int thread = -1 ) {
        if ( thread == _thread || thread < 0 )
            return Lens< Stack >( lens::LinearAddress( _stack, 0 ) );
        else
            return _blob_thread( thread ).sub( Stack() );
    }

    Lens< Heap > heap( int thread = -1 ) {
        if ( thread == _thread || thread < 0 )
            return Lens< Heap >( lens::LinearAddress( _heap, 0 ) );
        else
            return _blob_thread( thread ).sub( Heap() );
    }

    Lens< Frame > frame( int thread = -1, int idx = 0 ) {
        return stack( thread ).sub( idx );
    }

    void enter( int function ) {
        int framesize = _info.functions[ function ].framesize;
        int depth = stack().get().length();
        stack().get().length() ++;

        _frame = &stack().get( depth );
        _frame->pc = PC( function, 0, 0 );
        std::fill( _frame->memory, _frame->memory + framesize, 0 );
    }

    void leave() {
        stack().get().length() --;
        _frame = &stack().get( stack().get().length() - 1 );
    }

    Blob snapshot( Pool &pool ) {
        Blob b( pool, _blob.size() + _size_difference );
        lens::LinearAddress address( b, _slack );
        int i = 0;

        address = state().sub( Flags() ).copy( address );
        for ( ; i < _thread ; ++i )
            address = state().sub( Threads(), i ).copy( address );

        auto to = Lens< Thread >( lens::LinearAddress( b, address.offset ) );
        stack( _thread ).copy( to.address( Stack() ) );

        auto target = to.sub( Heap() );
        auto source = heap( _thread );

        // TODO canonicalize( source, target );
        address = source.copy( target.address() );

        for ( ; i < state().get( Threads() ).length(); ++i )
            address = state().sub( Threads(), i ).copy( address );

        assert_eq( address.offset, b.size() );
        return b;
    }

    MachineState( ProgramInfo &i ) : _stack( 4096 ), _heap( 4096 ), _info( i )
    {}

};

struct Nil {};

// Interpreter - This class represents the entirety of the interpreter.
//
class Interpreter : public ::llvm::ExecutionEngine, public ::llvm::InstVisitor<Interpreter>
{
    IntrinsicLowering *IL;
    friend ProgramInfo; // for getConstantValue

public:
    TargetData TD;

    std::map< std::string, std::string > properties;
    llvm::Module *module; /* The bitcode. */
    MachineState state; /* the state we are dealing with */
    ProgramInfo info;

    void parseProperties( Module *M );
    void buildInfo( Module *M );

    std::pair< Type *, char * > dereference( Type *, ProgramInfo::Value );
    std::pair< Type *, char * > dereferenceOperand( const ProgramInfo::Instruction &i, int n );
    std::pair< Type *, char * > dereferenceResult( const ProgramInfo::Instruction &i );
    char * dereferencePointer( Pointer );
    BasicBlock *dereferenceBB( Pointer );

    template< template< typename > class Fun, typename Cons >
    typename Fun< Nil >::T implement( ProgramInfo::Instruction i, Cons list );

    template< template< typename > class Fun, typename Cons, typename... Args >
    typename Fun< Nil >::T implement( ProgramInfo::Instruction i, Cons list,
                                      std::pair< Type *, char * > arg, Args... args );

    template< template< typename > class Fun, typename... Args >
    typename Fun< Nil >::T implementN( Args... args );

    template< template< typename > class I >
    void implement1( ProgramInfo::Instruction i );

    template< template< typename > class I >
    void implement2( ProgramInfo::Instruction i );

    template< template< typename > class I >
    void implement3( ProgramInfo::Instruction i );

    // the currently executing one, i.e. what pc of the top frame of the active thread points at
    ProgramInfo::Instruction instruction() { return info.instruction( pc() ); }
    PC pc() { return state._frame->pc; }

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

    explicit Interpreter(Module *M);
    ~Interpreter();

    typedef std::pair< std::string, char * > Describe;
    typedef std::set< std::pair< int, Type * > > DescribeSeen;

    Describe describeAggregate( Type *t, char *where, DescribeSeen& );
    Describe describeValue( Type *t, char *where, DescribeSeen& );
    std::string describePointer( Type *t, int idx, DescribeSeen& );
    std::string describeGenericValue( int vindex, GenericValue vvalue, DescribeSeen * = 0 );
    std::string describe();

    Blob initial( Function *f ); /* Make an initial state from Function. */
    void rewind( Blob );

    template< typename Yield > /* non-determistic choice */
    void runUninterruptible( int thread, Yield yield );

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

    void switchBB( BasicBlock *Dest );
    void leaveFrame(); /* without a return value */
    void leaveFrame( Type *ty, ProgramInfo::Value result );

private:  // Helper functions
    /* The following two are used by getConstantValue */
    void *getPointerToFunction(Function *F) { assert_die(); }
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
