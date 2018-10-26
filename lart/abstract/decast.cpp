// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/decast.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/metadata.h>
#include <lart/abstract/util.h>
#include <lart/support/query.h>
#include <lart/support/util.h>

namespace lart {

namespace query {
    template< typename T >
    bool isa( llvm::Value * val ) {
        return llvm::isa< T >( val );
    }
} // namespace query

namespace abstract {

using namespace llvm;

ReturnInst * get_single_return( Function * fn ) {
    auto rets = query::query( *fn ).flatten()
        .map( query::refToPtr )
        .filter( query::isa< ReturnInst > )
        .freeze();

    ASSERT_EQ( rets.size(), 1 );
    return cast< ReturnInst >( rets[ 0 ] );
}

Function * strip_int_to_ptr( IntToPtrInst * itp ) {
    auto fn = itp->getFunction();
    auto fty = fn->getFunctionType();
    auto new_fty = FunctionType::get( itp->getSrcTy(), fty->params(), fty->isVarArg() );

    auto stripped = cloneFunction( fn, new_fty );

    auto ret = get_single_return( stripped );
    auto citp = cast< IntToPtrInst >( ret->getReturnValue() );

    IRBuilder<> irb( ret );
    auto new_ret = irb.CreateRet( citp->getOperand( 0 ) );
    new_ret->copyMetadata( *ret );

    ret->eraseFromParent();
    citp->eraseFromParent();

    return stripped;
}

void replace_calls( Function * original, Function * nouveau ) {
    for ( auto user : original->users() ) {
        auto call = cast< CallInst >( user );
        IRBuilder<> irb( call );

        Values args = { call->arg_begin(), call->arg_end() };
        auto replace = irb.CreateCall( nouveau, args );
        replace->copyMetadata( *call );

        for ( auto cu : call->users() ) {
            auto pti = cast< PtrToIntInst >( cu );
            ASSERT_EQ( replace->getType(), pti->getDestTy() );
            pti->replaceAllUsesWith( replace );
        }

        for ( auto cu : call->users() ) {
            cast< Instruction >( cu )->eraseFromParent();
        }
    }

    for ( auto user : original->users() ) {
        cast< Instruction >( user )->eraseFromParent();
    }
}

void Decast::run( Module &m ) {
    using query::isa;

    auto casts = query::query( m ).flatten().flatten()
        .map( query::llvmdyncast< IntToPtrInst > )
        .filter( query::notnull )
        .filter( has_abstract_metadata )
        .freeze();

    Functions remove;
    for ( auto cast : casts ) {
        bool only_returned = query::all( cast->users(), isa< ReturnInst > );

        if ( only_returned ) {
            auto fn = cast->getFunction();
            auto users = query::query( fn->users() )
                .map( [] ( auto val ) {
                    return Values{ val->user_begin(), val->user_end() };
                } )
                .flatten().freeze();

            bool casts_back = query::all( users, isa< PtrToIntInst > );

            if ( casts_back ) {
                auto stripped = strip_int_to_ptr( cast );
                replace_calls( fn, stripped );
                remove.push_back( fn );
            }
        }
    }

    for ( auto fn : remove ) {
        fn->eraseFromParent();
    }
}

} /* namespace abstract */
} /* namespace lart */
