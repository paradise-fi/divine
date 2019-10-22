// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/stash.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/abstract/operation.h>

namespace lart::abstract {

    auto function( llvm::Module * m, const std::string & name )
    {
        auto fn = m->getFunction( name );
        ASSERT( fn, "Missing " + name + " function." );
        return fn;
    }

    auto unstash_function( llvm::Module * m )
    {
        return function( m, "__lart_unstash" );
    }

    auto stash_function( llvm::Module * m )
    {
        return function( m, "__lart_stash" );
    }


    struct ArgumentsBundle : LLVMUtil< ArgumentsBundle >
    {
        using Self = LLVMUtil< ArgumentsBundle >;

        using Self::get_function;

        ArgumentsBundle( llvm::Function *fn )
            : function( fn ), module( fn->getParent() )
        {}

        auto arguments() const noexcept
        {
            return query::query( function->args() )
                .map( query::refToPtr )
                .filter( [] ( auto arg ) {
                    return meta::argument::has( arg );
                } )
                .freeze();
        }

        auto type() const noexcept
        {
            if ( !bundle ) {
                Types types( arguments().size(), i8PTy() );
                bundle = llvm::StructType::create( types );
                bundle->setName( function->getName().str() + ".lart.bundle" );
            }

            return bundle;
        }

        auto packed( llvm::CallSite call, const Matched & matched ) const noexcept
            -> llvm::Value *
        {
            llvm::IRBuilder<> irb( call.getInstruction() );

            auto ty = type();
            auto addr = irb.CreateAlloca( ty );

            auto pack = undef( ty );

            auto is_abstract = [&] ( auto val ) {
                return matched.abstract.count( val );
            };

            uint32_t idx = 0;
            for ( auto arg : arguments() ) {
                auto op = call.getArgOperand( arg->getArgNo() );
                auto val = is_abstract( op )
                         ? matched.abstract.at( op )
                         : null_ptr( i8PTy() );
                pack = irb.CreateInsertValue( pack, val, { idx++ } );
            }

            irb.CreateStore( pack, addr );
            auto fn = stash_function( module );
            auto arg = irb.CreateBitCast( addr, i8PTy() );
            return irb.CreateCall( fn, arg );
        }

        void match( llvm::Argument * arg, llvm::Instruction * unstashed ) noexcept
        {
            llvm::IRBuilder<> irb( unstashed->getNextNode() );
            Values args = { arg, unstashed };

            auto name = std::string( unpacked_argument ) + llvm_name( arg->getType() );
            auto ret = voidTy();

            auto intr = get_function( name, ret, types_of( args ) );
            irb.CreateCall( intr, args );
        }

        auto unpacked() noexcept -> llvm::Value *
        {
            auto ip = function->getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
            auto fn = unstash_function( module );
            auto addr = create_call( ip, fn );

            llvm::IRBuilder<> irb( addr );
            auto ptr = irb.CreateBitCast( addr, type()->getPointerTo() );
            auto pack = irb.CreateLoad( type(), ptr );

            uint32_t idx = 0;
            for ( auto arg : arguments() ) {
                auto ex = irb.CreateExtractValue( pack, { idx++ } );
                match( arg, llvm::cast< llvm::Instruction >( ex ) );
            }

            addr->moveBefore( llvm::cast< llvm::Instruction >( ptr ) );
            return addr;
        }

        mutable llvm::StructType * bundle = nullptr;

        llvm::Function * function;
        llvm::Module * module;
    };


    auto unstash( llvm::CallInst * call ) -> llvm::Value *
    {
        auto fn = unstash_function( call->getModule() );
        return create_call_after( call, fn );
    }

    auto unstash( llvm::Function * fn ) -> llvm::Value *
    {
        return ArgumentsBundle( fn ).unpacked();
    }

    auto unstashed( llvm::CallInst * call ) -> llvm::CallInst *
    {
        auto next = call->getNextNonDebugInstruction();
        if ( auto un = llvm::dyn_cast< llvm::CallInst >( next ) ) {
            auto fn = unstash_function( call->getModule() );
            if ( un->getCalledFunction() == fn )
                return un;
        }

        return nullptr;
    }

    auto stash( llvm::ReturnInst * ret, llvm::Value * val ) -> llvm::Value *
    {
        auto fn = stash_function( ret->getModule() );
        return create_call( ret, fn, Values{ val } );
    }

    auto stash( llvm::Function *fn, llvm::CallSite call, const Matched & matched ) -> llvm::Value *
    {
        return ArgumentsBundle( fn ).packed( call, matched );
    }

    auto unpacked_arguments( llvm::Module * m ) -> std::vector< llvm::CallInst * >
    {
        return query::query( *m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( [&] ( auto call ) {
                auto fn = call->getCalledFunction();
                if ( !fn || !fn->hasName() )
                    return false;
                return fn->getName().startswith( unpacked_argument );
            } )
            .freeze();
    }

    auto has_abstract_args() noexcept
    {
        return [] ( auto fn ) {
            return meta::has( fn, meta::tag::function::arguments );
        };
    }

    auto fns_with_abstract_args( llvm::Module & m ) noexcept
    {
        return query::query( m )
            .map( query::refToPtr )
            .filter( has_abstract_args() )
            .freeze();
    }

    void StashPass::run( llvm::Module & m )
    {
        Matched matched;
        matched.init( m );

        auto rets = query::query( meta::enumerate( m, meta::tag::abstract_return ) )
            .map( query::llvmdyncast< llvm::ReturnInst > )
            .filter( query::notnull )
            .freeze();

        for ( auto ret : rets )
        {
            auto val = matched.abstract.at( ret->getReturnValue() );
            stash( ret, val );
        }

        for ( auto fn : fns_with_abstract_args( m ) )
            each_call( fn, [&]( auto call ) { stash( fn, call, matched ); } );
    }


    void UnstashPass::run( llvm::Module & m )
    {
        for ( auto fn : fns_with_abstract_args( m ) )
            unstash( fn );

        auto abstract = query::query( meta::enumerate( m ) )
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( [] ( auto call ) { return !is_faultable( call ); } )
            .freeze();

        for ( auto * call : abstract )
            unstash( call );
    }

} // namespace lart::abstract
