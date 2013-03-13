// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI

#include <wibble/maybe.h>

#include <divine/llvm/program.h>
#include <divine/llvm/machine.h>
#include <divine/graph/allocator.h> // hmm.

#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>
#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Target/TargetData.h>
#else
  #include <llvm/DataLayout.h>
  #define TargetData DataLayout
#endif

#ifndef DIVINE_LLVM_INTERPRETER_H
#define DIVINE_LLVM_INTERPRETER_H

namespace divine {
namespace llvm {

struct Interpreter;
using wibble::Maybe;

using namespace ::llvm;

struct Interpreter
{
    TargetData TD;
    std::map< std::string, std::string > properties;
    ::llvm::Module *module; /* The bitcode. */
    ProgramInfo info;
    Allocator &alloc;
    MachineState state; /* the state we are dealing with */

    bool jumped;
    int choice;
    int tid;

    bool tauminus, tauplus, taustores;

    void parseProperties( Module *M );
    bool observable( const std::set< PC > &s )
    {
        if ( !tauminus )
            return true;

        if ( tauplus && s.empty() )
            return false; /* this already caused an interrupt, if applicable */

        if ( isa< StoreInst >( instruction().op ) ) {
            if ( !taustores )
                return true;

            Pointer *p = reinterpret_cast< Pointer * >( dereference( instruction().operand( 1 ) ) );
            return p && !state.isPrivate( state._thread, *p );
        }
        return instruction().builtin == BuiltinInterrupt || instruction().builtin == BuiltinAp;
    }

    // the currently executing one, i.e. what pc of the top frame of the active thread points at
    ProgramInfo::Instruction instruction() { return info.instruction( pc() ); }
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
        for ( int i = 0; i < int( enums->getNumOperands() ); ++i ) {
            MDNode *n = cast< MDNode >( enums->getOperand(i) );
            MDString *name = cast< MDString >( n->getOperand(2) );
            if ( name->getString() == lookup )
                return cast< MDNode >( n->getOperand(10) ); // the list of enum items
        }
        return NULL;
    }

    explicit Interpreter(Allocator &a, Module *M);

    typedef std::set< std::pair< Pointer, Type * > > DescribeSeen;

    std::string describeAggregate( Type *t, Pointer where, DescribeSeen& );
    std::string describeValue( Type *t, Pointer where, DescribeSeen& );
    std::string describePointer( Type *t, Pointer p, DescribeSeen& );
    std::string describeValue( const ::llvm::Value *, ValueRef vref, Pointer p,
                               DescribeSeen &, int *anonymous = nullptr,
                               std::vector< std::string > *container = nullptr );
    std::string describeValue( std::pair< ::llvm::Type *, std::string >, ValueRef vref, Pointer p,
                               DescribeSeen &, int *anonymous = nullptr,
                               std::vector< std::string > *container = nullptr );
    std::string describe( bool detailed = false );

    Blob initial( Function *f ); /* Make an initial state from Function. */
    void rewind( Blob b ) { state.rewind( b, -1 ); }
    void choose( int32_t i );

    void advance() {
        pc().instruction ++;
        if ( !instruction().op ) {
            PC to = pc();
            to.block ++;
            to.instruction = 0;
            evaluateSwitchBB( to );
        }
    }

    template< typename Yield >
    void run( Blob b, Yield yield ) {
        state.rewind( b, -1 ); /* rewind first to get sense of thread count */
        state.flags().clear();
        state.flags().ap = 0; /* TODO */
        tid = 0;
        /* cache, to avoid problems with thread creation/destruction */
        int threads = state._thread_count;

        while ( threads ) {
            while ( tid < threads && !state.stack( tid ).get().length() )
                ++tid;
            run( tid, yield );
            if ( ++tid == threads )
                break;
            state.rewind( b, -1 );
            state.flags().clear();
            state.flags().ap = 0; /* TODO */
        }
    }

    void evaluate();
    void evaluateSwitchBB( PC to );

    template< typename Yield >
    void run( int tid, Yield yield ) {
        std::set< PC > seen;
        run( tid, yield, seen );
    }

    template< typename Yield >
    void run( int tid, Yield yield, std::set< PC > &seen ) {

        if ( !state._thread_count )
            return; /* no more successors for you */

        assert_leq( tid, state._thread_count - 1 );
        if ( state._thread != tid )
            state.switch_thread( tid );
        assert( state.stack().get().length() );

        while ( true ) {

            if ( !pc().masked && !seen.empty() ) {
                if ( tauplus ) {
                    if ( observable( seen ) || seen.count( pc() ) ) {
                        yield( state.snapshot() );
                        return;
                    }
                } else if ( tauminus ) {
                    /* look at seen too, because jumps might have been masked */
                    if ( observable( seen ) || jumped || seen.count( pc() ) ) {
                        yield( state.snapshot() );
                        return;
                    }
                } else {
                    yield( state.snapshot() );
                    return;
                }
            }

            jumped = false;
            choice = 0;
            seen.insert( pc() );
            evaluate();

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
                    run( tid, yield, seen );
                }
                fork.free( alloc.pool() );
                return;
            }

            if ( !jumped )
                advance();
        }

        yield( state.snapshot() );
    }

    /* EvalContext interface. */
    char *dereference( ValueRef v ) { return state.dereference( v ); }
    char *dereference( Pointer p ) { return state.dereference( p ); }
    Pointer malloc( int size ) { return state.malloc( size ); }
    void free( Pointer p ) { return state.free( p ); }

    template< typename X > bool isPointer( X p ) { return state.isPointer( p ); }
    template< typename X > void setPointer( X p, bool is ) { state.setPointer( p, is ); }
    template< typename X > bool inBounds( X p, int o ) { return state.inBounds( p, o ); }

    /* ControlContext interface. */
    int stackDepth() { return state.stack().get().length(); }
    MachineState::Frame &frame( int depth = 0 ) { return state.frame( -1, depth ); }
    MachineState::Flags &flags() { return state.flags(); }
    void problem( Problem::What p ) { state.problem( p ); }
    void leave() { state.leave(); }
    void enter( int fun ) { state.enter( fun ); }
    int new_thread( Function *f );
    int new_thread( PC pc, Maybe< Pointer > arg, bool = false );
    int threadId() { return tid; }
    PC &pc() { return state._frame->pc; }
};

}
}

#endif
