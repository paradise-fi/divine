// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/decast.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

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

template< typename Replacer >
void replace_calls( Function * original, Function * nouveau, Replacer replacer ) {
    for ( auto user : original->users() ) {
        if ( auto call = dyn_cast< CallInst >( user ) ) {
            IRBuilder<> irb( call );

            Values args = { call->arg_begin(), call->arg_end() };
            auto replace = irb.CreateCall( nouveau, args );
            replace->copyMetadata( *call );

            replacer( call, replace );

            for ( auto calluser : call->users() ) {
                if( auto inst = dyn_cast< Instruction >( calluser ) )
                    inst->eraseFromParent();
            }
        }
    }

    for ( auto user : original->users() ) {
        if( auto inst = dyn_cast< Instruction >( user ) )
            inst->eraseFromParent();
    }
}

void ptr_to_int_replacer( CallInst * call, Value * replace ) {
    for ( auto cu : call->users() ) {
        auto pti = cast< PtrToIntInst >( cu );
        ASSERT_EQ( replace->getType(), pti->getDestTy() );
        pti->replaceAllUsesWith( replace );
    }
};

Value * ptr_to_int( Value * val, Instruction * pos ) {
    ASSERT( val->getType()->isPointerTy() );
    auto i64  = IntegerType::get( val->getContext(), 64 );
    if ( isa< ConstantPointerNull >( val ) ) {
        return ConstantInt::get( i64, 0 );
    } else {
        return IRBuilder<>( pos ).CreatePtrToInt( val, i64 );
    }
}

void icmp_replacer( CallInst * call, Value * replace ) {
    for ( auto cu : call->users() ) {
        auto cmp = cast< ICmpInst >( cu );

        bool first = cmp->getOperand( 0 ) == call;
        Value * lhs = first ? replace : ptr_to_int( cmp->getOperand( 0 ), cmp );
        Value * rhs = first ? ptr_to_int( cmp->getOperand( 1 ), cmp ) : replace;

        auto new_cmp = cast< Instruction >(
            IRBuilder<>( cmp ).CreateICmp( cmp->getPredicate(), lhs, rhs ) );

        cmp->replaceAllUsesWith( new_cmp );
        new_cmp->copyMetadata( *cmp );
    }
}

void Decast::run( Module &m ) {
    using query::isa;

    auto casts = query::query( m ).flatten().flatten()
        .map( query::llvmdyncast< IntToPtrInst > )
        .filter( query::notnull )
        .filter( [] ( const auto& inst ) {
            return meta::abstract::has( inst );
        } )
        .freeze();

    Functions remove;

    auto replace = [&remove] ( auto cast, auto replacer ) {
        auto fn = cast->getFunction();
        auto stripped = strip_int_to_ptr( cast );
        replace_calls( fn, stripped, replacer );
        remove.push_back( fn );
    };

    for ( const auto & cast : casts ) {
        bool only_returned = query::all( cast->users(), isa< ReturnInst > );

        if ( only_returned ) {
            auto fn = cast->getFunction();
            auto users = query::query( fn->users() )
                .map( [] ( auto val ) {
                    return Values{ val->user_begin(), val->user_end() };
                } )
                .flatten().freeze();

            // TODO enable mixing
            if ( query::all( users, isa< PtrToIntInst > ) ) {
                replace( cast, ptr_to_int_replacer );
            }
            else if ( query::all( users, isa< ICmpInst > ) ) {
                replace( cast, icmp_replacer );
            }
        }
    }

    for ( auto fn : remove ) {
        fn->eraseFromParent();
    }
}

} /* namespace abstract */
} /* namespace lart */
