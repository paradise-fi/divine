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

#include <wibble/mixin.h>
#include <divine/toolkit/lens.h>
#include <divine/graph/allocator.h> // hmm.
#include <divine/llvm/arena.h>

namespace llvm {

class IntrinsicLowering;
struct FunctionInfo;
template<typename T> class generic_gep_type_iterator;
class ConstantExpr;
typedef generic_gep_type_iterator<User::const_op_iterator> gep_type_iterator;

}

namespace divine {
namespace llvm2 {

struct Interpreter;

using namespace ::llvm;

void static align( int &v, int a ) {
    if ( v % a )
        v += a - (v % a);
}

struct PC : wibble::mixin::Comparable< PC >
{
    uint32_t function:10;
    uint32_t block:12;
    uint32_t instruction:9;
    bool masked:1;
    explicit PC( int f = 0, int b = 0, int i = 0 )
        : function( f ), block( b ), instruction( i ), masked( false )
    {}

    bool operator<= ( PC o ) const {
        /* masked is irrelevant for equality! */
        return std::make_tuple( int( function ), int( block ), int( instruction ) )
            <= std::make_tuple( int( o.function ), int( o.block ), int( o.instruction ) );
    }
};

/*
 * We use a *non-overlapping* segmented memory scheme, with *variable-sized
 * segments*. Each allocation creates a new segment. Pointers cannot cross
 * segment boundaries through pointer arithmetic or any other manipulation.
 */
struct Pointer : wibble::mixin::Comparable< Pointer > {
    bool valid:1; /* make a (0, 0) pointer different from NULL */
    uint32_t segment:16; /* at most 64k objects */
    uint32_t offset:15; /* each at most 32kB */
    Pointer operator+( int relative ) {
        return Pointer( segment, offset + relative );
    }
    Pointer( int segment, int offset )
        : valid( true ), segment( segment ), offset( offset )
    {}
    Pointer() : valid( false ), segment( 0 ), offset( 0 ) {}
    bool null() { return !valid; } // all size = 0 pointers represent NULL

    bool operator<=( Pointer o ) const {
        if ( segment < o.segment )
            return true;
        return segment == o.segment && offset <= o.offset;
    }
};

enum Builtin {
    NotBuiltin = 0,
    BuiltinChoice = 1,
    BuiltinMask = 2,
    BuiltinUnmask = 3,
    BuiltinGetTID = 4,
    BuiltinNewThread = 5
};

struct ProgramInfo {
    TargetData &target;
    /* required for getConstantValue/StoreValueToMemory */
    Interpreter &interpreter;

    struct Value {
        bool global:1;
        bool constant:1;
        bool pointer:1;
        uint32_t offset:20;
        uint32_t width:9;

        bool operator<( Value v ) const {
            return *reinterpret_cast< const uint32_t * >( this ) < *reinterpret_cast< const uint32_t * >( &v );
        }

        Value()
            : global( false ), constant( false ), pointer( false ),
              offset( 0 ), width( 0 )
        {}
    };

    struct Instruction {
        Value result;
        std::vector< Value > operands;

        int builtin; /* non-zero if this is a call to a builtin */
        ::llvm::Instruction *op; /* the actual operation */
        Instruction() : op( nullptr ), builtin( NotBuiltin ) {}
        /* next instruction is in the same BB unless op == NULL */
    };

    struct BB {
        std::vector< Instruction > instructions;
        ::llvm::BasicBlock *bb;
        Instruction &instruction( PC pc ) {
            assert_leq( int( pc.instruction ), int( instructions.size() ) - 1 );
            return instructions[ pc.instruction ];
        }
        BB() : bb( nullptr ) {}
    };

    struct Function {
        int framesize;
        std::vector< Value > values;
        std::vector< BB > blocks;
        BB &block( PC pc ) {
            assert_leq( int( pc.block ), int( blocks.size() ) - 1 );
            return blocks[ pc.block ];
        }
    };

    std::vector< Function > functions;
    std::vector< Value > globals;
    std::vector< char > constdata;
    int globalsize, constdatasize;

    std::map< ::llvm::Value *, Value > valuemap;
    std::map< Value, ::llvm::Value * > llvmvaluemap;
    std::map< ::llvm::Instruction *, PC > pcmap;

    std::map< ::llvm::BasicBlock *, int > blockmap;
    std::map< ::llvm::Function *, int > functionmap;

    template< typename Container >
    static void makeFit( Container &c, int index ) {
        c.resize( std::max( index + 1, int( c.size() ) ) );
    }

    Instruction &instruction( PC pc ) {
        assert_leq( int( pc.block ), int( function( pc ).blocks.size() ) - 1 );
        return block( pc ).instruction( pc );
    }

    BB &block( PC pc ) {
        assert_leq( int( pc.block ), int( function( pc ).blocks.size() ) - 1 );
        return function( pc ).block( pc );
    }

    Function &function( PC pc ) {
        assert_leq( int( pc.function ), int( functions.size() ) - 1 );
        return functions[ pc.function ];
    }

    template< typename T >
    T &constant( Value v ) {
        return *reinterpret_cast< T * >( &constdata[ v.offset ] );
    }

    char *allocateConstant( Value &result ) {
        result.constant = true;
        result.offset = constdatasize;
        constdatasize += result.width;
        constdata.resize( constdata.size() + result.width );
        return &constdata[ result.offset ];
    }

    void allocateValue( int fun, Value &result ) {
        result.constant = false;
        if ( fun ) {
            result.global = false;
            result.offset = functions[ fun ].framesize;
            functions[ fun ].framesize += result.width;
        } else {
            result.global = true;
            result.offset = globalsize;
            globalsize += result.width;
        }
    }

    void storeGV( char *where, GenericValue GV, Type *ty, int width );

    void storeConstant( Value &result, int v ) {
        result.width = sizeof( int );
        allocateConstant( result );
        constant< int >( result ) = v;
    }

    void storeConstant( Value &result, PC pc ) {
        result.width = sizeof( PC );
        allocateConstant( result );
        constant< PC >( result ) = pc;
    }

    void storeConstant( Value &result, GenericValue GV, Type *ty );

    struct Position {
        PC pc;
        ::llvm::BasicBlock::iterator I;
        Position( PC pc, ::llvm::BasicBlock::iterator I ) : pc( pc ), I( I ) {}
    };

    Position insert( Position );
    Position lower( Position ); // convert intrinsic into normal insns
    void builtin( Position );
    Value insert( int function, ::llvm::Value *val );

    ProgramInfo( TargetData &td, Interpreter &i ) : target( td ), interpreter( i )
    {
        constdatasize = 0;
        globalsize = 0;
    }
};

struct MachineState
{
    struct StateAddress : lens::LinearAddress
    {
        ProgramInfo *_info;

        StateAddress( StateAddress base, int index, int offset )
            : LinearAddress( base, index, offset ), _info( base._info )
        {}

        StateAddress( ProgramInfo *i, Blob b, int offset )
            : LinearAddress( b, offset ), _info( i )
        {}

        StateAddress copy( StateAddress to, int size ) {
            std::copy( dereference(), dereference() + size, to.dereference() );
            return StateAddress( _info, to.b, to.offset + size );
        }
    };

    struct Frame {
        PC pc;
        char memory[0];

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, sizeof( Frame ) + a._info->function(pc).framesize );
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

    struct Heap {
        int segcount;
        char _memory[0];

        int size() {
            return _memory[ segcount ];
        }

        int bitmapSize() {
            return size() / 32 + (size() % 32 ? 4 : 0);
        }

        int jumptableSize() {
            /* 4-align, extra item for "end of memory" */
            return 2 * (2 + segcount - segcount % 2);
        }

        uint16_t &jumptable( Pointer p ) {
            assert( owns( p ) );
            return reinterpret_cast< uint16_t * >( _memory )[ p.segment ];
        }

        uint16_t &bitmap( Pointer p ) {
            assert( owns( p ) );
            return reinterpret_cast< uint16_t * >( _memory + jumptableSize() )[ p.segment / 64 ];
        }

        int offset( Pointer p ) {
            assert( owns( p ) );
            return int( jumptable( p ) * 4 ) + p.offset;
        }

        uint16_t mask( Pointer p ) {
            assert_eq( offset( p ) % 4, 0 );
            return 1 << ((offset( p ) % 64) / 4);
        }

        StateAddress advance( StateAddress a, int ) {
            return StateAddress( a, 0, sizeof( Heap ) + size() + jumptableSize() + bitmapSize() );
        }
        int end() { return 0; }

        void setPointer( Pointer p, bool ptr ) {
            if ( ptr )
                bitmap( p ) |= mask( p );
            else
                bitmap( p ) &= ~mask( p );
        }

        bool isPointer( Pointer p ) {
            return bitmap( p ) & mask( p );
        }

        bool owns( Pointer p ) {
            return p.segment < segcount;
        }

        char *dereference( Pointer p ) {
            assert( owns( p ) );
            return reinterpret_cast< char * >( _memory )
                   + bitmapSize() + jumptableSize()
                   + offset( p );
        }
    };

    struct Nursery {
        std::vector< int > offsets;
        std::vector< char > memory;
        std::vector< bool > pointer;
        int segshift;

        Pointer malloc( int size ) {
            int segment = offsets.size() - 1;
            int start = offsets[ segment ];
            int end = start + size;
            align( end, 4 );
            offsets.push_back( end );
            memory.resize( end );
            pointer.resize( end / 4 );
            std::fill( memory.begin() + start, memory.end(), 0 );
            std::fill( pointer.begin() + start / 4, pointer.end(), 0 );
            return Pointer( segment + segshift, 0 );
        }

        bool owns( Pointer p ) {
            return p.segment - segshift < offsets.size() - 1;
        }

        int offset( Pointer p ) {
            assert( owns( p ) );
            return offsets[ p.segment - segshift ] + p.offset;
        }

        bool isPointer( Pointer p ) {
            assert_eq( p.offset % 4, 0 );
            return pointer[ offset( p ) / 4 ];
        }

        void setPointer( Pointer p, bool is ) {
            pointer[ offset( p ) / 4 ] = is;
        }

        char *dereference( Pointer p ) {
            assert( owns( p ) );
            return &memory[ offset( p ) ];
        }

        void reset( int shift ) {
            segshift = shift;
            memory.clear();
            offsets.clear();
            offsets.push_back( 0 );
            pointer.clear();
        }
    };

    Blob _blob, _stack;
    Nursery nursery;
    ProgramInfo &_info;
    Allocator &_alloc;
    int _thread; /* the currently scheduled thread */
    int _thread_count;
    Frame *_frame; /* pointer to the currently active frame */
    int _stack_difference;

    template< typename T >
    using Lens = lens::Lens< StateAddress, T >;

    struct Flags {
        uint64_t assert:1;
        uint64_t null_dereference:1;
        uint64_t invalid_dereference:1;
        uint64_t invalid_argument:1;
        uint64_t ap:48;
        uint64_t buchi:12;
    };

    typedef lens::Array< Frame > Stack;
    typedef lens::Array< Stack > Threads;
    typedef lens::Tuple< Flags, Globals, Heap, Threads > State;

    char *dereference( Pointer p ) {
        assert( validate( p ) );
        if ( heap().owns( p ) )
            return heap().dereference( p );
        else
            return nursery.dereference( p );
    }

    bool validate( Pointer p ) {
        return p.valid && ( heap().owns( p ) || nursery.owns( p ) );
    }

    Pointer followPointer( Pointer p ) {
        if ( !p.valid )
            return Pointer();

        Pointer next = *reinterpret_cast< Pointer * >( dereference( p ) );
        if ( heap().owns( p ) ) {
            if ( heap().isPointer( p ) )
                return next;
        } else if ( nursery.isPointer( p ) )
            return next;
        return Pointer();
    }

    Lens< State > state() {
        return Lens< State >( StateAddress( &_info, _blob, _alloc._slack ) );
    }

    /*
     * Get a pointer to the value storage corresponding to "v". If "v" is a
     * local variable, look into thread "thread" (default, i.e. -1, for the
     * currently executing one) and in frame "frame" (default, i.e. 0 is the
     * topmost frame, 1 is the frame just below that, etc...).
     */
    char *dereference( ProgramInfo::Value v, int tid = -1, int frame = 0 )
    {
        char *block = _frame->memory;

        if ( tid < 0 )
            tid = _thread;

        if ( !v.global && !v.constant && ( frame || tid != _thread ) )
            block = stack( tid ).get( stack( tid ).get().length() - frame - 1 ).memory;

        if ( v.global )
            block = state().get( Globals() ).memory;

        if ( v.constant )
            block = &_info.constdata[0];

        return block + v.offset;
    }

    void rewind( Blob to, int thread = 0 )
    {
        _blob = to;
        _thread = -1; // special
        _stack_difference = 0;

        _thread_count = threads().get().length();
        nursery.reset( heap().segcount );

        if ( thread >= 0 && thread < threads().get().length() )
            switch_thread( thread );
        // else everything becomes rather unsafe...
    }

    void switch_thread( int thread )
    {
        assert_leq( thread, threads().get().length() - 1 );
        _thread = thread;

        StateAddress stackaddr( &_info, _stack, 0 );
        _blob_stack( _thread ).copy( stackaddr );
        _frame = &stack().get( stack().get().length() - 1 );
   }

    int new_thread()
    {
        Blob old = _blob;
        _blob = snapshot();
        old.free( _alloc.pool() );
        _thread = _thread_count ++;

        /* Set up an empty thread. */
        stack().get().length() = 0;
        _stack_difference += sizeof( int );
        return _thread;
    }

    Lens< Threads > threads() {
        return state().sub( Threads() );
    }

    Lens< Stack > _blob_stack( int i ) {
        assert_leq( 0, i );
        return state().sub( Threads(), i );
    }

    Lens< Stack > stack( int thread = -1 ) {
        if ( thread == _thread || thread < 0 ) {
            assert_leq( 0, _thread );
            return Lens< Stack >( StateAddress( &_info, _stack, 0 ) );
        } else
            return _blob_stack( thread );
    }

    Heap &heap() {
        return state().get( Heap() );
    }

    Frame &frame( int thread = -1, int idx = 0 ) {
         if ( !idx )
             return *_frame;

        auto s = stack( thread );
        return s.get( s.get().length() - idx - 1 );
    }

    Flags &flags() {
        return state().get( Flags() );
    }

    void enter( int function ) {
        int framesize = _info.functions[ function ].framesize;
        int depth = stack().get().length();
        bool masked = depth ? frame().pc.masked : false;
        stack().get().length() ++;

        _frame = &stack().get( depth );
        _frame->pc = PC( function, 0, 0 );
        _frame->pc.masked = masked; /* inherit */
        std::fill( _frame->memory, _frame->memory + framesize, 0 );
        _stack_difference += framesize + sizeof( Frame );
    }

    void leave() {
        int fun = frame().pc.function;
        auto &s = stack().get();
        s.length() --;
        _stack_difference -= _info.functions[ fun ].framesize + sizeof( Frame );
        if ( stack().get().length() )
            _frame = &stack().get( stack().get().length() - 1 );
        else
            _frame = nullptr;
    }

    Blob snapshot() {
        /* TODO build a pointer update map and work out heap size */

        Blob b( _alloc.pool(), _blob.size() + _stack_difference );
        StateAddress address( &_info, b, _alloc._slack );
        int i = 0;

        address = state().sub( Flags() ).copy( address );
        assert_eq( address.offset, _alloc._slack + sizeof( Flags ) );
        address = state().sub( Globals() ).copy( address );

        /* TODO canonicalize the heap; use a copying GC */
        address = state().sub( Heap() ).copy( address );

        address.as< int >() = _thread_count;
        address.offset += sizeof( int ); // ick. length of the threads array
        for ( ; i < _thread ; ++i )
            address = state().sub( Threads(), i ).copy( address );

        if ( _thread >= 0 ) { // we actually have a current thread to speak of
            address = stack( _thread ).copy( address );

            /* this thread also existed in the previous state */
            if ( _thread < state().get( Threads() ).length() )
                ++ i;
        }

        /* might have been that length here == _thread; that's OK */
        for ( ; i < state().get( Threads() ).length(); ++i )
            address = state().sub( Threads(), i ).copy( address );

        assert_eq( address.offset, b.size() );
        return b;
    }

    MachineState( ProgramInfo &i, Allocator &alloc )
        : _stack( 4096 ), _info( i ), _alloc( alloc )
    {
        _thread_count = 0;
        _stack_difference = 0;
        _frame = nullptr;
        nursery.reset( 0 ); /* nothing in the heap */
    }

};

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
    std::string describeValue( ProgramInfo::Value, int, DescribeSeen * = 0 );
    std::string describe();

    Blob initial( Function *f ); /* Make an initial state from Function. */
    void new_thread( Function *f );
    void rewind( Blob b ) { state.rewind( b, 0 ); }
    void choose( int32_t i );

    void advance() {
        if ( !jumped ) { // did not jump
            pc().instruction ++;
            if ( !instruction().op ) {
                PC to = pc();
                to.block ++;
                to.instruction = 0;
                switchBB( to );
            }
        }
    }

    template< typename Yield >
    void run( Blob b, Yield yield ) {
        state.rewind( b, -1 ); int tid = 0;
        while ( true ) {
            run( tid, yield );
            if ( ++tid == state._thread_count )
                break;
            state.rewind( b, -1 );
        }
    }

    template< typename Yield >
    void run( int tid, Yield yield ) {
        std::set< PC > seen;

        assert_leq( tid, state._thread_count - 1 );
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
                for ( int i = 0; i < choice; ++i ) {
                    state.rewind( fork, -1 );
                    choose( i );
                    advance();
                    run( tid, yield );
                }
                fork.free( alloc.pool() );
                return;
            }

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
