// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/stash.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/abstract/placeholder.h>

namespace lart::abstract {

    namespace {

        template< Placeholder::Type T >
        Placeholder process( llvm::Value * val, llvm::IRBuilder<> & irb )
        {
            llvm::Value * arg = val;
            if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( val ) )
                arg = ret->getReturnValue();

            return APlaceholderBuilder().construct< T >( arg, irb );
        }

        Placeholder stash( llvm::Value * val, llvm::IRBuilder<> & irb )
        {
            return process< Placeholder::Type::Stash >( val, irb );
        }

        Placeholder unstash( llvm::Value * val, llvm::IRBuilder<> & irb )
        {
            auto ph = process< Placeholder::Type::Unstash >( val, irb );
            meta::make_duals( val, ph.inst );
            return ph;
        }

        Placeholder unstash( llvm::Instruction * inst )
        {
            llvm::IRBuilder<> irb( inst );
            return unstash( inst, irb );
        }
    } // anonymous namespace

    void Stash::run( llvm::Module & m )
    {
        run_on_abstract_calls( [&] ( auto call ) {
            if ( !is_transformable( call ) ) {
                run_on_potentialy_called_functions( call, [&] ( auto fn ) {
                    if ( !meta::has( fn, meta::tag::abstract ) )
                        if ( !stashed.count( fn ) )
                            process_return_value( call, fn );
                    stashed.insert( fn );
                } );
            }
        }, m );
    }

    void Stash::process_return_value( llvm::CallInst * call, llvm::Function * fn )
    {
        if ( auto ret = returns_abstract_value( call, fn ) ) {
            auto inst = llvm::cast< llvm::Instruction >( ret );
            llvm::IRBuilder<> irb( inst );
            auto ph = stash( inst, irb );
            meta::make_duals( inst, ph.inst );
        }
    }

    void Unstash::run( llvm::Module & m )
    {
        run_on_abstract_calls( [&] ( auto call ) {
            if ( !is_transformable( call ) ) {
                run_on_potentialy_called_functions( call, [&] ( auto fn ) {
                    if ( !meta::has( fn, meta::tag::abstract ) )
                        process_arguments( call, fn );
                } );

                process_return_value( call );
            }
        }, m );
    }

    void Unstash::process_arguments( llvm::CallInst * call, llvm::Function * fn )
    {
        assert( !fn->empty() && "unstashing function without defininition" );
        auto concrete = [&] ( llvm::Argument * arg ) {
            return is_concrete( call->getOperand( arg->getArgNo() ) );
        };

        auto in_concern = [&] ( llvm::Argument * arg ) {
            auto  m = fn->getParent();
            auto dom = Domain::get( arg );
            return is_base_type_in_domain( m, arg, dom );
        };

        auto processed = [&] ( llvm::Argument * arg ) {
            if ( auto dual = meta::get_dual( arg ) ) {
                auto inst = llvm::cast< llvm::Instruction >( dual );
                ASSERT( Placeholder::is( inst ) );
                ASSERT( Placeholder( inst ).type == Placeholder::Type::Unstash );
                return true;
            }
            return false;
        };

        llvm::IRBuilder<> irb( fn->getEntryBlock().getFirstNonPHIOrDbgOrLifetime() );

        query::query( fn->args() )
            .map( query::refToPtr )
            .filter( query::negate( concrete ) )
            .filter( in_concern )
            .filter( query::negate( processed ) )
            .map( [&] ( auto * arg ) { return unstash( arg, irb ); } )
            .freeze();
    }

    void Unstash::process_return_value( llvm::CallInst * call )
    {
        auto returns = query::query( get_potentialy_called_functions( call ) )
            .filter( [call] ( const auto & fn ) {
                return returns_abstract_value( call, fn );
            } )
            .freeze();

        if ( !returns.empty() ) {
            auto ph = unstash( call );
            ph.inst->moveAfter( call );
        }
    }

} // namespace lart::abstract
