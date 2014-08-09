// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI

#include <wibble/maybe.h>

#include <divine/llvm/program.h>
#include <divine/llvm/machine.h>

#include <divine/llvm/wrap/Function.h>
#include <divine/llvm/wrap/Module.h>
#include <divine/llvm/wrap/Instructions.h>

#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Target/TargetData.h>
#else
  #include <divine/llvm/wrap/DataLayout.h>
  #define TargetData DataLayout
#endif
#undef PACKAGE_VERSION
#undef PACKAGE_TARNAME
#undef PACKAGE_STRING
#undef PACKAGE_NAME
#undef PACKAGE_BUGREPORT

#include <llvm/ADT/OwningPtr.h>
#include <divine/llvm/wrap/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/system_error.h>
#include <llvm/Bitcode/ReaderWriter.h>

#include <divine/graph/label.h>

#include <numeric>

#ifndef DIVINE_LLVM_INTERPRETER_H
#define DIVINE_LLVM_INTERPRETER_H

namespace divine {
namespace llvm {

template< typename T > struct Interpreter;
using graph::Label;

using namespace ::llvm;

struct BitCode {
    OwningPtr< MemoryBuffer > input;
    LLVMContext *ctx;
    std::shared_ptr< ::llvm::Module > module; /* The bitcode. */
    ProgramInfo *info;

    BitCode( std::string file )
    {
        ctx = new ::llvm::LLVMContext();
        MemoryBuffer::getFile( file, input );
        module.reset( ParseBitcodeFile( &*input, *ctx ) );
        info = new ProgramInfo( module.get() );
        assert( module );
    }

    BitCode( std::shared_ptr< ::llvm::Module > m )
        : ctx( nullptr ), module( m )
    {
        assert( module );
        info = new ProgramInfo( module.get() );
    }

    ~BitCode() {
        if ( ctx )
            assert_eq( module.use_count(), 1 );
        module.reset();
        delete info;
        delete ctx;
    }
};

template< typename HeapMeta >
struct Interpreter
{
    Pool &pool;
    std::shared_ptr< BitCode > bc;
    TargetData TD;
    MachineState< HeapMeta > state; /* the state we are dealing with */
    std::map< std::string, std::string > properties;

    bool jumped;
    Choice choice;
    int tid;

    bool tauminus, tauplus, taustores;

    ProgramInfo &info() { return *bc->info; }

    void parseProperties( Module *M );
    bool observable( const std::set< PC > &s )
    {
        if ( !tauminus )
            return true;

        if ( tauplus && s.empty() )
            return false; /* this already caused an interrupt, if applicable */

        bool store = isa< StoreInst >( instruction().op );
        if ( store || isa< AtomicRMWInst >( instruction().op ) ||
             isa< LoadInst >( instruction().op ) )
        {
            if ( !taustores )
                return true;

            Pointer *p = reinterpret_cast< Pointer * >(
                dereference( instruction().operand( store ? 1 : 0 ) ) );
            return p && !state.isPrivate( state._thread, *p );
        }
        return instruction().builtin == BuiltinInterrupt || instruction().builtin == BuiltinAp;
    }

    // the currently executing one, i.e. what pc of the top frame of the active thread points at
    ProgramInfo::Instruction instruction() { return info().instruction( pc() ); }
    MDNode *node( MDNode *root ) { return root; }

    template< typename N, typename... Args >
    MDNode *node( N *root, int n, Args... args ) {
        if (root)
            return node( cast< MDNode >( root->getOperand(n) ), args... );
        return NULL;
    }

    MDNode *findEnum( std::string lookup ) {
        assert( bc->module );
        auto meta =  bc->module->getNamedMetadata( "llvm.dbg.cu" );
        // sadly metadata for enums are located at different locations in
        // IR produced by clang 3.2 and 3.3
        // to make it worse I didn't find way to distinguish between those
        // by other method then trying
        auto e = findEnum( node( meta, 0, 10, 0 ), lookup, 2 ); // clang <= 3.2
        return e ? e : findEnum( node( meta, 0, 7 ), lookup, 3 ); // clang >= 3.3

    }

    MDNode *findEnum( MDNode *enums, std::string lookup, int nameOperand ) {
        if ( !enums )
            return nullptr;
        for ( int i = 0; i < int( enums->getNumOperands() ); ++i ) {
            MDNode *n = dyn_cast_or_null< MDNode >( enums->getOperand(i) );
            if ( !n )
                return nullptr; // this would means we hit wrong metadata list, not enums
            auto no = n->getNumOperands();
            if ( no <= 10 )
                return nullptr; // same here
            MDString *name = dyn_cast_or_null< MDString >( n->getOperand( nameOperand ) );
            if ( !name )
                return nullptr;
            if ( name->getString() == lookup )
                return cast< MDNode >( n->getOperand(10) ); // the list of enum items
        }
        return nullptr;
    }

    explicit Interpreter( Pool &a, int slack, std::shared_ptr< BitCode > bc );

    std::string describe( bool demangle = false, bool detailed = false );
    std::string describeConstdata();

    Blob initial( Function *f, bool is_start = false );
    void rewind( Blob b ) { state.rewind( b, -1 ); }
    void choose( int32_t i );
    void dump();

    void advance() {
        pc().instruction ++;
        if ( !instruction().op ) {
            PC to = pc();
            to.instruction ++;
            evaluateSwitchBB( to );
        }
    }

    template< typename Yield >
    void run( Blob b, Yield yield ) {
        state.rewind( b, -1 ); /* rewind first to get sense of thread count */
        state.flags().ap = 0; /* TODO */
        tid = 0;
        /* cache, to avoid problems with thread creation/destruction */
        int threads = state._thread_count;

        while ( threads ) {
            while ( tid < threads && !state.stack( tid ).get().length() )
                ++tid;
            run( tid, yield, Label( tid ) );
            if ( ++tid == threads )
                break;
            state.rewind( b, -1 );
            state.flags().ap = 0; /* TODO */
        }
    }

    void evaluate();
    void evaluateSwitchBB( PC to );

    template< typename Yield >
    void run( int tid, Yield yield, Label p ) {
        std::set< PC > seen;
        run( tid, yield, p, seen );
    }

    bool interrupt( std::set< PC > &seen ) {
        if ( pc().masked || seen.empty() )
            return false;

        if ( !tauplus && !tauminus )
            return true;

        if ( observable( seen ) || seen.count( pc() ) )
            return true;

        if ( !tauplus && jumped )
            return true;

        if ( state.flags().problemcount || state.problems.size() )
           return true;

        return false;
    }

    template< typename Yield >
    void run( int tid, Yield yield, Label l, std::set< PC > &seen ) {

        if ( !state._thread_count )
            return; /* no more successors for you */

        assert_leq( tid, state._thread_count - 1 );
        if ( state._thread != tid )
            state.switch_thread( tid );

        assert( state.stack().get().length() );

        while ( true ) {

            if ( interrupt( seen ) ) {
                yield( state.snapshot(), l );
                return;
            }

            jumped = false;
            choice.p.clear();
            choice.options = 0;
            seen.insert( pc() );
            evaluate();

            if ( !state.stack().get().length() )
                break; /* this thread is done */

            if ( choice.options ) {
                assert( !jumped );
                Blob fork = state.snapshot();
                Choice c = choice; /* make a copy, sublings must overwrite the original */
                for ( int i = 0; i < c.options; ++i ) {
                    state.rewind( fork, tid );
                    choose( i );
                    advance();
                    auto pp = c.p.empty() ? l.levelup( i ) :
                              l * std::make_pair( c.p[ i ], std::accumulate( c.p.begin(), c.p.end(), 0 ) );
                    run( tid, yield, pp, seen );
                }
                pool.free( fork );
                return;
            }

            if ( !jumped )
                advance();
        }

        yield( state.snapshot(), l );
    }

    /* EvalContext interface. */
    char *dereference( ValueRef v ) { return state.dereference( v ); }
    char *dereference( Pointer p ) { return state.dereference( p ); }
    int pointerSize( Pointer p ) { return state.pointerSize( p ); }
    Pointer malloc( int size, int id ) { return state.malloc( size, id ); }
    bool free( Pointer p ) { return state.free( p ); }
    std::vector< int > pointerId( ::llvm::Instruction *insn ) {
        return state.heapMeta().pointerId( insn ); }
    int pointerId( Pointer p ) { return state.pointerId( p ); }

    template< typename X > MemoryBits memoryflag( X p ) { return state.memoryflag( p ); }
    template< typename X > bool inBounds( X p, int o ) { return state.inBounds( p, o ); }

    /* ControlContext interface. */
    int stackDepth() { return state.stack().get().length(); }
    machine::Frame &frame( int depth = 0 ) { return state.frame( -1, depth ); }
    machine::Flags &flags() { return state.flags(); }
    void problem( Problem::What p, Pointer ptr = Pointer() ) { state.problem( p, ptr ); }
    void leave() { state.leave(); }
    void enter( int fun ) { state.enter( fun ); }
    int new_thread( Function *f );
    int new_thread( PC pc, Maybe< Pointer > arg, MemoryFlag );
    int threadId() { return tid; }
    int threadCount() { return state._thread_count; }
    void switch_thread( int t ) { state.switch_thread( t ); }
    PC &pc() { return state._frame->pc; }
};

}
}

#endif
