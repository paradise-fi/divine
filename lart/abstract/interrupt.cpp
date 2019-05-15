// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/interrupt.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/util.h>
#include <lart/support/query.h>

using namespace lart::abstract;
using namespace lart;
using namespace llvm;

using lart::util::get_module;
using lart::util::get_or_insert_function;

namespace {

Function * interrupt_function( Module &m ) {
    auto void_ty = Type::getVoidTy( m.getContext() );
    auto fty = FunctionType::get( void_ty, false );
    auto interrupt = get_or_insert_function( &m, fty, "__dios_suspend" );
    interrupt->addFnAttr( Attribute::NoUnwind );
    return interrupt;
}

struct RecursiveCalls {
    std::vector< Function* > get( Module &m ) {
        std::set< Function* > recursive;

        for ( auto & fn : m )
            if ( meta::abstract::roots( &fn ) )
                if ( is_recursive( &fn ) )
                    recursive.insert( &fn );

        return { recursive.begin(), recursive.end() };
    }

    bool is_recursive( Function *fn ) {
        std::set< Function* > seen;
        std::vector< Function* > stack;

        stack.push_back( fn );

        bool recursive = false;
        while ( !stack.empty() ) {
            auto curr = stack.back();
            stack.pop_back();

            auto calls = query::query( *curr ).flatten()
                .map( query::refToPtr )
                .map( query::llvmdyncast< CallInst > )
                .filter( query::notnull )
                .freeze();

            for ( auto call : calls ) {
                run_on_potentially_called_functions( call, [&] ( auto called_fn ) {
                    if ( called_fn->empty() )
                        return;
                    if ( called_fn == fn )
                        recursive = true;
                    if ( !seen.count( called_fn ) ) {
                        seen.insert( called_fn );
                        if ( meta::abstract::roots( called_fn ) )
                            stack.push_back( called_fn );
                    }
                } );
            }
        }

        return recursive;
    }
};

} // anonymous namespace

void CallInterrupt::run( llvm::Module &m )
{
    auto interrupt = interrupt_function( m );
    for ( auto &fn : RecursiveCalls().get( m ) )
    {
        auto entry = fn->getEntryBlock().getFirstInsertionPt();
        IRBuilder<>( &*entry ).CreateCall( interrupt );
    }
}

