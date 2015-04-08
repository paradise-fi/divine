
// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#include <brick-types.h>
#include <brick-data.h>

#include <divine/llvm/program.h>
#include <divine/llvm/machine.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>

#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
  #include <llvm/Target/TargetData.h>
#else
  #include <llvm/IR/DataLayout.h>
  #define TargetData DataLayout
#endif
#undef PACKAGE_VERSION
#undef PACKAGE_TARNAME
#undef PACKAGE_STRING
#undef PACKAGE_NAME
#undef PACKAGE_BUGREPORT

#include <llvm/ADT/OwningPtr.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/system_error.h>
#include <llvm/Bitcode/ReaderWriter.h>

#include <divine/graph/label.h>

#include <numeric>
#include <unordered_set>

#ifndef DIVINE_LLVM_INTERPRETER_H
#define DIVINE_LLVM_INTERPRETER_H

namespace divine {
namespace llvm {

template< typename T, typename L > struct Interpreter;

using namespace ::llvm;
using brick::types::Maybe;

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
		if ( !module )
			throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
        info = new ProgramInfo( module.get() );
    }

    BitCode( std::shared_ptr< ::llvm::Module > m )
        : ctx( nullptr ), module( m )
    {
        ASSERT( module );
        info = new ProgramInfo( module.get() );
    }

    ~BitCode() {
        if ( ctx )
            ASSERT_EQ( module.use_count(), 1 );
        module.reset();
        delete info;
        delete ctx;
    }
};

template< typename HeapMeta, typename Label = graph::NoLabel >
struct Interpreter
{
    Pool &pool;
    std::shared_ptr< BitCode > bc;
    TargetData TD;
    MachineState< HeapMeta > state; /* the state we are dealing with */
    std::map< std::string, std::string > properties;

    bool jumped;
    Choice choice;

    bool tauminus, tauplus, taustores;

    using SeenPC = std::unordered_set< PC >;

    ProgramInfo &info() { return *bc->info; }

    void parseProperties( Module *M );
    bool observable( const SeenPC &s )
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
    ProgramInfo::Instruction &instruction() { return info().instruction( pc() ); }
    MDNode *node( MDNode *root ) { return root; }

    template< typename N, typename... Args >
    MDNode *node( N *root, int n, Args... args ) {
        if ( root && isa< MDNode >( root->getOperand( n ) ) )
            return node( cast< MDNode >( root->getOperand(n) ), args... );
        return NULL;
    }

    MDNode *findEnum( std::string lookup ) {
        ASSERT( bc->module );
        auto meta =  bc->module->getNamedMetadata( "llvm.dbg.cu" );
        if ( !meta )
            return nullptr;

        for ( int modid = 0; modid < int(meta->getNumOperands()); ++modid )
            if ( auto e = findEnum( node( meta, modid, 7 ), lookup, 3 ) )
                return e;
        return nullptr;
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

    brick::data::Bimap< int, std::string > describeAPs();

    template< typename Alloc >
    void rewind( Alloc alloc, Blob b ) { state.rewind( alloc, b, -1 ); }
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

    template< typename Yield, typename Alloc >
    void run( Blob b, Yield yield, Alloc alloc, int _tid = -1, bool include = true ) {
        state.rewind( alloc, b, -1 ); /* rewind first to get sense of thread count */
        state.flags().ap = 0; /* TODO */
        int tid = 0;
        /* cache, to avoid problems with thread creation/destruction */
        int threads = state._thread_count;

        if ( _tid >= 0 && include ) {
            if ( _tid < threads )
                run( _tid, yield, Label( _tid ), alloc );
        } else while ( threads ) {
            while ( tid < threads && !state.stack( tid ).get().length() )
                ++tid;
            if ( _tid < 0 || _tid != tid )
                run( tid, yield, Label( tid ), alloc );
            if ( ++tid == threads )
                break;
            state.rewind( alloc, b, -1 );
            state.flags().ap = 0; /* TODO */
        }
    }

    void evaluate();
    void evaluateSwitchBB( PC to );

    template< typename Yield, typename Alloc >
    void run( int tid, Yield yield, Label p, Alloc alloc ) {
        SeenPC seen;
        run( tid, yield, p, seen, alloc );
    }

    bool interrupt( SeenPC &seen ) {
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

    using Leaves = std::vector< std::vector< std::pair< Blob, Label > > >;

    template< typename Yield, typename Alloc >
    void probabilistic( Yield yield, Label l, Alloc alloc, Leaves &leaves ) {
        std::vector< std::vector< std::vector< std::pair< Blob, Label > > > > clusters;
        std::vector< std::vector< int > > qs; /* coordinates */

        int dim = 1, max = 0;

        for ( auto &br : leaves ) {
            clusters.emplace_back();
            for ( auto r : br ) {
                bool done = false;
                for ( int i = 0; !done && i < int( clusters.back().size() ); ++i )
                    if ( clusters.back()[ i ][ 0 ].second.cluster() == r.second.cluster() ) {
                        clusters.back()[ i ].push_back( r );
                        done = true;
                    }
                if ( !done ) {
                    clusters.back().emplace_back();
                    clusters.back().back().push_back( r );
                }
                max = std::max( max, r.second.cluster() );
            }
            dim *= clusters.back().size();
        }

        for ( int i = 0; i < dim; ++i ) {
            int x = i;
            qs.emplace_back();
            for ( auto &clusterset: clusters ) {
                qs[ i ].push_back( x % clusterset.size() );
                x /= clusterset.size();
            }
        }

        for ( auto &q : qs ) {
            int cluster = 0;
            for ( int i = 0; i < int( q.size() ); ++ i )
                cluster += q[ i ] * std::pow( max, i );
            Label cl = l.levelup( cluster );
            for ( int i = 0; i < int( q.size() ); ++i ) {
                for ( auto r : clusters[ i ][ q[ i ] ] ) {
                    r.second.cluster( cl.cluster() );
                    Blob b = alloc.get( pool, pool.size( r.first ) );
                    pool.copy( r.first, b );
                    yield( b, r.second );
                }
            }
        }

        for ( auto &br : leaves )
            for ( auto l : br )
                alloc.drop( pool, l.first );
    }

    template< typename Yield, typename Alloc >
    void run( int tid, Yield yield, Label l, SeenPC &seen, Alloc alloc )
    {
        if ( !state._thread_count )
            return; /* no more successors for you */

        ASSERT_LEQ( tid, state._thread_count - 1 );
        if ( state._thread != tid )
            state.switch_thread( tid );

        ASSERT( state.stack().get().length() );

        while ( true ) {

            if ( interrupt( seen ) ) {
                yield( state.snapshot( alloc ), l );
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
                ASSERT( !jumped );
                Blob fork = state.snapshot( alloc );
                Leaves leaves;
                SeenPC s = seen;
                Choice c = choice; /* make a copy, sublings must overwrite the original */

                for ( int i = 0; i < c.options; ++i ) {
                    state.rewind( alloc, fork, tid );
                    choose( i );
                    advance();
                    seen = s;
                    Label pp = l.levelup( i );
                    if ( !c.p.empty() ) { // probabilistic
                        pp = pp * std::make_pair( c.p[ i ], std::accumulate( c.p.begin(), c.p.end(), 0 ) );
                        leaves.resize( i + 1 );
                        std::function< void( Blob, Label ) > f = [&]( Blob b, Label lbl ) {
                            leaves[ i ].push_back( std::make_pair( b, lbl ) );
                        };
                        run( tid, f, pp, seen, alloc );
                    } else // non-deterministic
                        run( tid, yield, pp, seen, alloc );
                }

                alloc.drop( pool, fork );

                if ( !c.p.empty() )
                    probabilistic( yield, l, alloc, leaves );

                return;
            }

            if ( !jumped )
                advance();
        }

        yield( state.snapshot( alloc ), l );
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
    bool validate( Pointer p, bool heap ) { return state.validate( p, heap ); }

    /* ControlContext interface. */
    int stackDepth() { return state.stack().get().length(); }
    machine::Frame &frame( int depth = 0 ) { return state.frame( -1, depth ); }
    machine::Flags &flags() { return state.flags(); }
    void problem( Problem::What p, Pointer ptr = Pointer() ) { state.problem( p, ptr ); }
    void leave() { state.leave(); }
    void enter( int fun ) { state.enter( fun ); }
    int new_thread( Function *f );
    int new_thread( PC pc, Maybe< Pointer > arg, MemoryFlag );
    int threadId() { return state._thread; }
    int threadCount() { return state._thread_count; }
    void switch_thread( int t ) { state.switch_thread( t ); }
    PC &pc() { return state._frame->pc; }

    template< typename Alloc >
    divine::Blob initial( Alloc alloc, Function *f, bool is_start = false )
    {
        Blob pre_initial = alloc.get( pool, state._slack + state.size( 0, 0, 0, 0 ) );
        pool.clear( pre_initial );
        state.rewind( alloc, pre_initial, 0 ); // there isn't a thread really
        std::copy( info().globaldata.begin(), info().globaldata.end(), state.global().memory() );
        auto fl = state.global().memoryflag( info() );
        for ( int i = 0; i < int( info().globaldata.size() ); ++ i ) {
            fl.set( MemoryFlag::Data );
            ++ fl;
        }
        int tid = state.new_thread(); // switches automagically
        ASSERT_EQ( tid, 0 ); // just to be on the safe side...
        static_cast< void >( tid );
        state.enter( info().functionmap[ f ] );

        if ( is_start ) {
            auto &fun = info().function( PC( info().functionmap[ f ], 0 ) );
            auto ctors = info().module->getNamedGlobal( "llvm.global_ctors" );
            if ( ctors ) {
                auto ctor_arr = ::llvm::cast< ::llvm::ConstantArray >( ctors->getInitializer() );
                auto ctors_val = info().valuemap[ ctors ];

                ASSERT_EQ( fun.values[ 0 ].width, sizeof( int ) );
                ASSERT_EQ( fun.values[ 1 ].width, ctors_val.width );
                ASSERT_EQ( fun.values[ 2 ].width, sizeof( int ) );
                ASSERT( info().module->getFunction( "main" ) );

                for ( int i = 0; i <= 2; ++i )
                    state.memoryflag( fun.values[ i ] ).set( MemoryFlag::Data );

                *reinterpret_cast< int * >( state.dereference( fun.values[ 0 ] ) ) =
                    ctor_arr->getNumOperands();
                memcopy( ctors_val, fun.values[ 1 ], ctors_val.width, state, state );
                *reinterpret_cast< int * >( state.dereference( fun.values[ 2 ] ) ) =
                    info().module->getFunction( "main" )->arg_size();
            }
        }

        Blob result = state.snapshot( alloc );
        state.rewind( alloc, result, 0 ); // so that we don't wind up in an invalid state...
        alloc.drop( pool, pre_initial );
        return result;
    }

};

}
}

#include <llvm/Config/config.h>
#if ( LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2 )
#  include <llvm/Support/IRBuilder.h>
#else
#  include <llvm/IR/IRBuilder.h>
#endif

namespace divine_test {
namespace llvm {

using namespace ::llvm;
namespace dlvm = divine::llvm;

struct TestLLVM {
    LLVMContext &ctx;
    IRBuilder<> builder;
    divine::Pool pool;
    std::shared_ptr< Module > module;
    using Interpreter = dlvm::Interpreter< dlvm::machine::NoHeapMeta, divine::graph::NoLabel >;

    TestLLVM() : ctx( getGlobalContext() ), builder( ctx ) {}

    std::shared_ptr< dlvm::BitCode > bitcode() {
        return std::make_shared< dlvm::BitCode >( module );
    }

    Function *_function( Module *m = NULL, const char *name = "testf", int pcount = 0 ) {
        if ( !m ) {
            module = std::make_shared< Module >( "testm", ctx );
            m = module.get();
        }
        std::vector< Type * > args;
        for ( int i = 0; i < pcount; ++ i )
            args.push_back( Type::getInt32Ty( ctx ) );
        FunctionType *t = FunctionType::get(Type::getInt32Ty(ctx), args, false);
        Function *f = Function::Create(t, Function::ExternalLinkage, name, m);
        BasicBlock *BB = BasicBlock::Create(ctx, "entry", f);
        builder.SetInsertPoint( BB );
        return f;
    }

    Function *code_ret() {
        Function *main = _function();
        Value *result = ConstantInt::get(ctx, APInt(32, 1));
        builder.CreateRet( result );
        return main;
    }

    Function *code_loop() {
        Function *f = _function();
        builder.CreateBr( &*(f->begin()) );
        return f;
    }

    Function *code_add() {
        Function *f = _function();
        Value *a = ConstantInt::get(ctx, APInt(32, 1)),
              *b = ConstantInt::get(ctx, APInt(32, 2));
        Value *result = builder.Insert( BinaryOperator::Create( Instruction::Add, a, b ), "meh" );
        builder.CreateBr( &*(f->begin()) );
        builder.CreateRet( result );
        return f;
    }

    Function *code_mem() {
        Function *f = _function();
        auto *mem = builder.CreateAlloca(Type::getInt32Ty(ctx), 0, "foo");
        builder.CreateStore(ConstantInt::get(ctx, APInt(32, 33)), mem);
        BasicBlock *BB = BasicBlock::Create(ctx, "tail", f);
        builder.SetInsertPoint( BB );
        builder.CreateBr( BB );
        return f;
    }

    Function *code_call() {
        Function *helper = _function( NULL, "helper", 0 );
        builder.CreateBr( &*(helper->begin()) );
        Function *f = _function( helper->getParent() );
        builder.CreateCall( helper );
        return f;
    }

    Function *code_callarg() {
        Function *helper = _function( NULL, "helper", 1 );
        builder.Insert( BinaryOperator::Create( Instruction::Add,
                                                helper->arg_begin(),
                                                helper->arg_begin() ), "meh" );
        builder.CreateBr( &*(helper->begin()) );
        Function *f = _function( helper->getParent() );
        builder.CreateCall( helper, ConstantInt::get(ctx, APInt(32, 7)) );
        return f;
    }

    Function *code_callret() {
        Function *helper = _function( NULL, "helper", 0 );
        builder.CreateRet( ConstantInt::get(ctx, APInt(32, 42)) );
        Function *f = _function( helper->getParent() );
        builder.CreateCall( helper, "meh" );
        builder.CreateBr( &*(f->begin()) );
        return f;
    }

    divine::Blob _ith( Function *f, int step ) {
        Interpreter interpreter( pool, 0, bitcode() );
        divine::Blob ini = interpreter.initial( LongTerm(), f ), fin;
        fin = ini;

        for ( int i = 0; i < step; ++i ) {
            fin = divine::Blob();
            interpreter.run( ini, [&]( divine::Blob b, divine::graph::NoLabel ) {
                    ASSERT( !pool.valid( fin ) ); // only one allowed
                    fin = b;
                }, LongTerm() );
            ini = fin;
            ASSERT( pool.valid( fin ) );
        }

        return fin;
    }

    std::string _descr( Function *, divine::Blob b ) {
        Interpreter interpreter( pool, 0, bitcode() );
        interpreter.rewind( LongTerm(), b );
        return interpreter.describe();
    }

    TEST(initial)
    {
        Function *main = code_ret();
        Interpreter i( pool, 0, bitcode() );
        i.initial( LongTerm(), main );
    }

    TEST(successor1)
    {
        ASSERT_EQ( fmtblob( pool, _ith( code_ret(), 1 ) ),
                   "[ 0, 0, 0, 0, 0 ]" );
    }

    TEST(successor2)
    {
        ASSERT_EQ( fmtblob( pool, _ith( code_loop(), 1 ) ),
                   "[ 0, 0, 0, 0, 1, 1, 2147745792 ]" );
    }

    TEST(successor3)
    {
        ASSERT_EQ( fmtblob( pool, _ith( code_add(), 2 ) ),
                   "[ 0, 0, 0, 0, 1, 1, 2147745792, 3, 1 ]" );
        ASSERT_EQ( fmtblob( pool, _ith( code_add(), 4 ) ),
                   "[ 0, 0, 0, 0, 1, 1, 2147745792, 3, 1 ]" );
    }

    TEST(describe1)
    {
        divine::Blob b UNUSED = _ith( code_loop(), 1 );
        ASSERT_EQ( _descr( code_loop(), b ),
                   "thread 0:\n  #1: <testf> << br label %entry >> []\n" );
    }

    TEST(describe2)
    {
        Function *f = code_loop();
        Interpreter interpreter( pool, 0, bitcode() );
        divine::Blob b = _ith( code_loop(), 1 );
        interpreter.rewind( LongTerm(), b );
        interpreter.new_thread( f );
        ASSERT_EQ( "thread 0:\n  #1: <testf> << br label %entry >> []\n"
                   "thread 1:\n  #1: <testf> << br label %entry >> []\n", interpreter.describe() );
    }

    TEST(describe3)
    {
        divine::Blob b = _ith( code_add(), 0 );
        ASSERT_EQ( _descr( code_add(), b ),
                   "thread 0:\n  #1: <testf> << %meh = add i32 1, 2 >> [ meh = ? ]\n" );

        b = _ith( code_add(), 2 );
        ASSERT_EQ( _descr( code_add(), b ),
                   "thread 0:\n  #1: <testf> << %meh = add i32 1, 2 >> [ meh = 3 ]\n" );
    }

    TEST(describe4)
    {
        divine::Blob b = _ith( code_call(), 0 );
        ASSERT_EQ( _descr( code_call(), b ),
                   "thread 0:\n  #1: <testf> << %0 = call i32 @helper() >> []\n" );
        b = _ith( code_call(), 1 );
        ASSERT_EQ( _descr( code_call(), b ),
                   "thread 0:\n"
                   "  #1: <testf> << %0 = call i32 @helper() >> []\n"
                   "  #2: <helper> << br label %entry >> []\n" );
    }

    TEST(describe5)
    {
        divine::Blob b = _ith( code_callarg(), 0 );
        ASSERT_EQ( _descr( code_callarg(), b ),
                   "thread 0:\n  #1: <testf> << %0 = call i32 @helper(i32 7) >> []\n" );
        b = _ith( code_callarg(), 3 );
        ASSERT_EQ( _descr( code_callarg(), b ),
                   "thread 0:\n"
                   "  #1: <testf> << %0 = call i32 @helper(i32 7) >> []\n"
                   "  #2: <helper> << %meh = add i32 %0, %0 >> [ meh = 14 ]\n" );
    }

    TEST(describe6)
    {
        divine::Blob b UNUSED = _ith( code_callret(), 3 );
        ASSERT_EQ( _descr( code_callret(), b ),
                   "thread 0:\n  #1: <testf> << %meh = call i32 @helper() >> [ meh = 42 ]\n" );
    }

    TEST(memory1)
    {
        divine::Blob b UNUSED = _ith( code_mem(), 2 );
        ASSERT_EQ( _descr( code_mem(), b ),
                   "thread 0:\n  #1: <testf> << br label %tail >> [ foo = @(0:0| 33) ]\n" );
    }

    TEST(idempotency)
    {
        Function *f = code_loop();
        Interpreter interpreter( pool, 0, bitcode() );
        divine::Blob b1 = interpreter.initial( LongTerm(), f ), b2;
        interpreter.rewind( LongTerm(), b1 );
        b2 = interpreter.state.snapshot( LongTerm() );
        ASSERT( pool.equal( b1, b2 ) );
    }
};

}
}

#endif
