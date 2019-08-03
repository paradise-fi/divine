#include <string>
#include <iostream>
#include <memory>
#include <cassert>
#include <vector>
#include <system_error>
#include <queue>
#include <functional>
#include <utility>
#include <unordered_set>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/Casting.h>
#include <llvm/Support/SourceMgr.h>

#include <llvm/IRReader/IRReader.h>

#include <llvm/Bitcode/BitcodeWriter.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include <llvm/Analysis/CallGraph.h>

#include <llvm/Transforms/Utils/Cloning.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-assert>

#include <divine/mc/bitcode.hpp>
#include <divine/mc/job.tpp>
#include <divine/mc/safety.hpp>

#include <divine/vm/divm.h>
#include <divine/vm/memory.hpp>

#include <divine/ui/log.hpp>

#include <divine/dbg/context.hpp>
#include <divine/dbg/stepper.hpp>

using namespace std::string_literals;

std::unique_ptr< llvm::Module > get_module( const char *path, llvm::LLVMContext &ctx );
bool check_signatures( const llvm::Module &mod );
bool has_scalar_signature( const llvm::Function &fun );
std::queue< llvm::Function * > get_function_queue( llvm::Module &mod );

bool verify_queue( std::queue< llvm::Function * > &queue, std::shared_ptr< llvm::LLVMContext > ctx );
bool verify_function( llvm::Function &func, std::shared_ptr< llvm::LLVMContext > ctx );

void inject_symbolic_dummy_body( llvm::Function &fun );
llvm::Function *get_symbolic_value_getter( llvm::Module *mod, llvm::Type *type );
llvm::Function *generate_test_wrapper( llvm::Function *fun );

int main( int argc, char **argv )
{
    if ( argc != 2 )
    {
        std::cout << "Usage: shoop <input.bc>" << std::endl;

        return 0;
    }

    const auto *const tested_file_path = argv[1];

    std::cout << "Loading the bitcode from '" << tested_file_path << "'... " << std::flush;

    auto ctx = std::make_shared< llvm::LLVMContext >();

    auto mod = get_module( tested_file_path, *ctx );
    if ( !mod )
    {
        std::cout << "ERROR" << std::endl;
        std::cerr << "Error: could not load the bitcode" << std::endl;

        return 1;
    }

    std::cout << "DONE" << std::endl;
    std::cout << "Checking function signatures... " << std::flush;

    if ( !check_signatures( *mod ) )
    {
        std::cout << "ERROR" << std::endl;
        std::cerr << "Error: there is at least one function with nontrivial signature" << std::endl;
        std::cerr << "Note: at the moment, only scalar types"
                  << "(i.e. types representable as an integral or floating point value, and void)"
                  << "are supported"
                  << std::endl;

        return 1;
    }

    std::cout << "DONE" << std::endl;
    std::cout << "Building function queue... " << std::flush;

    auto testing_queue = get_function_queue( *mod );

    std::cout << "DONE" << std::endl;
    std::cout << "Verifying the functions..." << std::endl;

    auto res = verify_queue( testing_queue, ctx );

    std::cout << "Status: " << (res ? "OK" : "NOK") << std::endl;

    return 0;
}

std::unique_ptr< llvm::Module > get_module( const char *path, llvm::LLVMContext &ctx )
{
    llvm::SMDiagnostic diag;
    auto mod = llvm::parseIRFile( path, diag, ctx );

    if ( mod )
        mod->setTargetTriple( "x86_64-unknown-none-elf" );

    return mod;
}

bool check_signatures( const llvm::Module &mod )
{
    for ( const auto &fun : mod.functions() )
    {
        if ( !has_scalar_signature( fun ) )
            return false;
    }

    return true;
}

bool has_scalar_signature( const llvm::Function &fun )
{
    auto is_scalar_type = []( llvm::Type *const t )
    {
        return t->isIntegerTy() || t->isFloatingPointTy() || t->isVoidTy();
    };

    auto ft = fun.getFunctionType();

    auto res = is_scalar_type( ft->getReturnType() );
    for ( auto t : ft->params() )
    {
        res = res && is_scalar_type( t );
    }

    return res;
}

//Note: This function currently depends on the fact that the callgraph of the provided module forms
//      a forest (It can theoretically deal with a basic form of DAGs, too, as long as the roots are
//      independent of each other, but this is currently only very basic and extremely limited.)
std::queue< llvm::Function * > get_function_queue( llvm::Module &mod )
{
    llvm::CallGraph graph { mod };

    std::vector< llvm::CallGraphNode * > roots;
    for ( auto [inst, node] : *graph.getExternalCallingNode() )
    {
        roots.push_back( node );
    }

    std::queue< llvm::Function * > queue;
    std::unordered_set< llvm::Function * > closed;

    std::function< void( llvm::CallGraphNode * ) > push_subtree = [&]( llvm::CallGraphNode *root )
    {
        for ( auto [inst, node] : *root )
        {
            // The node representing a call out of the callgraph has a null function
            if ( node != graph.getCallsExternalNode() )
                push_subtree( node );
        }

        llvm::Function *fun = root->getFunction();

        if ( closed.count( fun ) == 0 )
        {
            // If it's just a declaration, then we'll consider it trivially verified
            if ( fun->isDeclaration() )
                inject_symbolic_dummy_body( *fun );
            else
                queue.push( fun );

            closed.insert( fun );
        }
    };

    for (auto root : roots)
    {
        push_subtree( root );
    }

    return queue;
}

void inject_symbolic_dummy_body( llvm::Function &fun )
{
    auto prev_linkage = fun.getLinkage();

    // This automatically sets the linkage to external
    // TODO: Check whether it's safe (i.e. if it doesn't erase too much data)
    fun.deleteBody();

    auto entry = llvm::BasicBlock::Create( fun.getParent()->getContext(), "", &fun );
    llvm::IRBuilder<> builder { entry };

    if ( !fun.getReturnType()->isVoidTy() )
    {
        auto c = builder.CreateCall( get_symbolic_value_getter( fun.getParent(), fun.getReturnType() ) );
        builder.CreateRet( c );
    }
    else
        builder.CreateRetVoid();

    fun.setLinkage( prev_linkage );
}

llvm::Function *get_symbolic_value_getter( llvm::Module *mod, llvm::Type *type )
{
    auto &ctx = mod->getContext();

    llvm::FunctionType *fun_type = nullptr;
    const char *name = nullptr;

    // uint64_t __sym_val_i64( void );
    if ( type->isIntegerTy( 64 ) )
    {
        fun_type = llvm::FunctionType::get( llvm::Type::getInt64Ty( ctx ), false );
        name = "__sym_val_i64";
    }
    // uint32_t __sym_val_i32( void );
    else if ( type->isIntegerTy( 32 ) )
    {
        fun_type = llvm::FunctionType::get( llvm::Type::getInt32Ty( ctx ), false );
        name = "__sym_val_i32";
    }
    // uint16_t __sym_val_i16( void );
    else if ( type->isIntegerTy( 16 ) )
    {
        fun_type = llvm::FunctionType::get( llvm::Type::getInt16Ty( ctx ), false );
        name = "__sym_val_i16";
    }
    // uint8_t __sym_val_i8( void );
    else if ( type->isIntegerTy( 8 ) )
    {
        fun_type = llvm::FunctionType::get( llvm::Type::getInt8Ty( ctx ), false );
        name = "__sym_val_i8";
    }
    // double __sym_val_float64( void );
    else if ( type->isDoubleTy() )
    {
        fun_type = llvm::FunctionType::get( llvm::Type::getDoubleTy( ctx ), false );
        name = "__sym_val_float64";
    }
    // float __sym_val_float32( void );
    else if ( type->isFloatTy() )
    {
        fun_type = llvm::FunctionType::get( llvm::Type::getFloatTy( ctx ), false );
        name = "__sym_val_float32";
    }
    else
        assert( 0 && "called 'get_symbolic_value_getter' with a non-scalar value" );

    // We don't accept different signatures
    return llvm::cast< llvm::Function >( mod->getOrInsertFunction( name, fun_type ) );
}

bool verify_queue( std::queue< llvm::Function * > &queue, std::shared_ptr< llvm::LLVMContext > ctx )
{
    while ( !queue.empty() )
    {
        auto current = queue.front();

        std::cout << "Verifying '" << current->getName().str() << "'... " << std::flush;

        auto r = verify_function( *current, ctx );

        if ( r )
            std::cout << "OK" << std::endl;
        else
        {
            std::cout << "NOK (yaml log dumped to stderr)" << std::endl;
            return false;
        }

        queue.pop();
    }

    return true;
}

bool verify_function( llvm::Function &source_func, std::shared_ptr< llvm::LLVMContext > ctx )
{
    auto source_mod = source_func.getParent();
    auto tested_mod = llvm::CloneModule( *source_mod );

    auto tested_func = tested_mod->getFunction( source_func.getName() );
    assert( tested_func && "could not find the duplicated function" );

    // Insert a dummy main if no main is present (needed to appease the runtime)
    if ( !tested_mod->getFunction( "main" ) )
    {
        auto type = llvm::FunctionType::get( llvm::Type::getInt32Ty( *ctx ), false );
        auto m = llvm::cast< llvm::Function >( tested_mod->getOrInsertFunction( "main", type ) );

        auto entry = llvm::BasicBlock::Create( *ctx, "main.0", m );
        llvm::IRBuilder<> builder { entry };

        builder.CreateRet( llvm::ConstantInt::get( llvm::Type::getInt32Ty( *ctx ), 0, true) );
    }

    // Generate the test wrapper [ void __tester( void ) ]
    generate_test_wrapper( tested_func );

    // Now let's run Divine
    auto bc = std::make_shared< divine::mc::BitCode >( std::move( tested_mod ), ctx );
    bc->dios_config( "static" );

    bc->symbolic( true );
    bc->solver( "stp" );
    bc->init();

    auto safety = divine::mc::make_job< divine::mc::Safety >( bc, divine::ss::passive_listen() );
    safety->start( 1 );
    safety->wait();

    if ( safety->result() == divine::mc::Result::Valid )
    {
        inject_symbolic_dummy_body( source_func );
        return true;
    }
    else if ( safety->result() == divine::mc::Result::Error )
    {
        divine::dbg::Context< divine::vm::CowHeap > dbg {  bc->program(), bc->debug() };
        auto trace = safety->ce_trace();
        auto log = divine::ui::make_yaml( std::cerr, false );

        log->info( "\n" );
        log->result( safety->result(), trace );

        safety->dbg_fill( dbg );
        dbg.load( trace.final );
        dbg._lock = trace.steps.back();
        dbg._lock_mode = decltype( dbg )::LockBoth;

        divine::vm::setup::scheduler( dbg );
        using Stepper = divine::dbg::Stepper< decltype( dbg ) >;
        Stepper step;
        step._stop_on_error = true;
        step._stop_on_accept = true;
        step._ff_components = divine::dbg::Component::Kernel;
        step.run( dbg, Stepper::Quiet );

        log->backtrace( dbg, 10 ); // How detailed we want the log to be?

        return false;
    }
    else if ( safety->result() == divine::mc::Result::BootError )
    {
        // TEST
        std::cerr << "\nFATAL ERROR: Could not boot" << std::endl;
        return false;
        // ENDTEST
    }
    else if ( safety->result() == divine::mc::Result::None )
    {
        // TEST
        std::cerr << "\nFATAL ERROR: Result is none" << std::endl;
        return false;
        // ENDTEST
    }

    UNREACHABLE( "enum divine::mc::Result not exhausted" );
    return false;
}

llvm::Function *generate_test_wrapper( llvm::Function *fun )
{
    auto &ctx = fun->getContext();
    auto mod = fun->getParent();

    const auto name = "__tester"s;
    const auto type = llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ), false );

    // Assume that the function has the correct signature, if it exists
    auto wrapper = llvm::cast< llvm::Function >( mod->getOrInsertFunction( name, type ) );

    auto entry = llvm::BasicBlock::Create( ctx, name + ".0", wrapper );
    llvm::IRBuilder<> builder { entry };

    std::vector< llvm::Value * > call_args;
    for ( auto &t : fun->getFunctionType()->params() )
    {
        auto sym = builder.CreateCall( get_symbolic_value_getter( mod, t ) );
        call_args.push_back( sym );
    }

    builder.CreateCall( fun, call_args );
    builder.CreateRetVoid();

    return wrapper;
}
