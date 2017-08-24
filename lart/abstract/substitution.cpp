// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/substitution.h>

#include <lart/abstract/walker.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/util.h>
#include <lart/support/query.h>
#include <lart/analysis/postorder.h>

#include <iterator>

namespace lart {
namespace abstract {
namespace {

void removeInvalidAttributes( llvm::Function * fn, TMap & tmap ) {
    using AttrSet = llvm::AttributeSet;
    using Kind = llvm::Attribute::AttrKind;

    auto removeAttributes = [&] ( size_t i, const auto& attrs ) {
        auto rem = AttrSet::get( fn->getContext(), i, attrs );
        fn->removeAttributes( i, rem );
    };
    auto attrs = llvm::ArrayRef< Kind >{ Kind::SExt, Kind::ZExt };

    if ( isAbstract( fn->getReturnType(), tmap ) )
        removeAttributes( 0, attrs );
    for ( size_t i = 0; i < fn->arg_size(); ++i )
        if ( isAbstract( fn->getFunctionType()->getParamType( i ), tmap ) )
            removeAttributes( i + 1, attrs );
}

template< typename Fns >
void clean( Fns & fns, llvm::Module & m ) {
    for ( auto & f : fns ) {
        f.first->replaceAllUsesWith( llvm::UndefValue::get( f.first->getType() ) );
        f.first->eraseFromParent();
    }

    auto intrinsics = query::query( m )
        .map( query::refToPtr )
        .filter( []( llvm::Function * fn ) {
            return isIntrinsic( fn );
        } ).freeze();
    for ( auto & in : intrinsics )
        in->eraseFromParent();
}

bool isAbstract( llvm::Function * fn, TMap & tmap ) {
    bool args = query::query( fn->args() ).any( [&] ( const auto & arg ) {
        return isAbstract( arg.getType(), tmap );
    } );
    return args || isAbstract( fn->getReturnType(), tmap );
}

Functions abstractFunctions( llvm::Module & m, TMap & tmap ) {
    return query::query( m )
        .map( query::refToPtr )
        .filter( []( llvm::Function * fn ) {
            return fn->hasName() && ! fn->getName().startswith( "lart." );
        } )
        .filter( [&]( llvm::Function * fn ) {
            return isAbstract( fn, tmap );
        } ).freeze();
}

decltype(auto) callsOf( llvm::Function * fn ) {
    return llvmFilter< llvm::CallInst >( fn->users() );
}

decltype(auto) callsOf( const Functions & fns ) {
    return query::query( fns )
        .map( [] ( const auto & fn ) { return callsOf( fn ); } )
        .flatten()
        .freeze();
}

} // anonymous namespace

void Substitution::run( llvm::Module & m ) {
    { // begin RAII substitution builder
    auto sb = make_sbuilder( data.tmap, fns, m );
    auto functions = abstractFunctions( m, data.tmap );
    for ( auto & fn : functions ) {
        assert( fn->getName() != "main" );
        fns[ fn ] = sb.prototype( fn );
    }

    // TODO solve returns of functions without arguments
    // = move changing of returns to here
    auto intrs = intrinsics( &m );

    auto allocas = query::query( intrs ).filter( [] ( const auto & i ) {
        return isAlloca( i );
    } ).freeze();

    auto lifts = query::query( intrs ).filter( [] ( const auto & i ) {
        return isLift( i );
    } ).freeze();

    auto calls = callsOf( functions );

    Insts abstract;
    abstract.reserve( allocas.size() + lifts.size() );
    abstract.insert( abstract.end(), allocas.begin(), allocas.end() );
    abstract.insert( abstract.end(), lifts.begin(), lifts.end() );
    abstract.insert( abstract.end(), calls.begin(), calls.end() );

    Map< llvm::Function *, Values > funToValMap;
    for ( const auto &a : abstract )
        funToValMap[ getFunction( a ) ].push_back( a );

    auto succs = [&] ( llvm::Value * v ) -> Values {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( v ) ) {
            auto fn = call->getCalledFunction();
            if ( !isAbstract( fn->getReturnType(), data.tmap ) && !isIntrinsic( fn ) )
                return {};
        }
        return { v->user_begin(), v->user_end() };
    };

    for ( auto & fn : funToValMap ) {
        if ( fn.first->hasName() && fn.first->getName().startswith( "lart." ) )
            continue;
        for ( auto & arg : fn.first->args() )
            sb.process( &arg );
        removeInvalidAttributes( fn.first, data.tmap );
        auto deps = analysis::postorder( fn.second, succs );
        for ( auto dep : lart::util::reverse( deps ) )
            if( auto i = llvm::dyn_cast< llvm::Instruction >( dep ) )
                sb.process( i );
    }

    } // end RAII substitution builder

    auto remapArg = [] ( llvm::Argument & a ) {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( *a.user_begin() ) )
            if ( call->getCalledFunction()->hasName() &&
                 call->getCalledFunction()->getName().startswith( "__lart_lift" ) )
            {
                call->replaceAllUsesWith( &a );
                call->eraseFromParent();
            }
    };

    for ( auto & fn : fns ) {
        llvm::ValueToValueMapTy vtvmap;
        cloneFunctionInto( fn.second, fn.first, vtvmap );
        for ( auto & arg : fn.second->args() )
            remapArg( arg );
    }
    clean( fns, m );
}

} // namespace abstract
} // namespace lart
