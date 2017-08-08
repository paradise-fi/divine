// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/substitution.h>

#include <lart/abstract/types/common.h>
#include <lart/abstract/walker.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/query.h>
#include <lart/analysis/postorder.h>

#include <iterator>

namespace lart {
namespace abstract {
namespace {

void removeInvalidAttributes( llvm::Function* fn ) {
    using AttrSet = llvm::AttributeSet;
    using Kind = llvm::Attribute::AttrKind;

    auto removeAttributes = [&] ( size_t i, const auto& attrs ) {
        auto rem = AttrSet::get( fn->getContext(), i, attrs );
        fn->removeAttributes( i, rem );
    };
    auto attrs = llvm::ArrayRef< Kind >{ Kind::SExt, Kind::ZExt };

    if ( isAbstract( fn->getReturnType() ) )
        removeAttributes( 0, attrs );
    for ( size_t i = 0; i < fn->arg_size(); ++i )
        if ( isAbstract( fn->getFunctionType()->getParamType( i ) ) )
            removeAttributes( i + 1, attrs );
}

} // anonymous namespace

void Substitution::run( llvm::Module & m ) {
    init( m );
    auto functions = query::query( m )
        .map( query::refToPtr )
        .filter( []( llvm::Function * f ) {
            return f->hasName() && !f->getName().startswith( "lart." );
        } )
        .filter( []( llvm::Function * f ) {
            return query::query( f->args() )
                .any( [] ( auto& arg ) { return isAbstract( arg.getType() ); } );
        } ).freeze();
    // TODO solve returns of functions without arguments
    // = move changing of returns to here
    for ( const auto & fn : functions )
        subst.process( fn );

    auto intrinsics = query::query( m ).flatten().flatten()
        .map( query::refToPtr )
        .map( query::llvmdyncast< llvm::CallInst > )
        .filter( query::notnull )
        .filter( [] ( llvm::CallInst * call ) {
            return isIntrinsic( call );
        } ).freeze();

    auto allocas = query::query( intrinsics )
        .filter( []( const Intrinsic & intr ) {
            return intr.name() == "alloca";
        } ).freeze();

    auto lifts = query::query( intrinsics )
        .filter( []( const Intrinsic & intr ) {
            return isLift( intr );
        } ).freeze();

    std::vector< llvm::Instruction * > abstract;
    abstract.reserve( allocas.size() + lifts.size() );
    abstract.insert( abstract.end(), allocas.begin(), allocas.end() );
    abstract.insert( abstract.end(), lifts.begin(), lifts.end() );

    std::map< llvm::Function *, std::vector< llvm::Instruction * > > funToValMap;
    for ( const auto &a : abstract ) {
        auto inst = llvm::dyn_cast< llvm::Instruction >( a );
        assert( inst );

        llvm::Function * fn = inst->getParent()->getParent();
        if ( subst.visited( fn ) )
            continue;
        funToValMap[ fn ].push_back( inst );
    }

    for ( auto & fn : funToValMap ) {
        if ( fn.first->hasName() && fn.first->getName().startswith( "lart." ) )
            continue;
        removeInvalidAttributes( fn.first );
        for ( auto & inst : fn.second )
            process( inst );
    }

    auto retAbsVal = query::query( m )
                    .map( query::refToPtr )
                    .filter( [&]( llvm::Function * fn ) {
                        return isAbstract( fn->getReturnType() ) && !isIntrinsic( fn );
                    } )
                    .filter( [&]( llvm::Function * fn ) {
                        return !subst.visited( fn )
                             || funToValMap.count( fn );
                    } ).freeze();

    auto retorder = analysis::callPostorder< llvm::Function * >( m, retAbsVal );

    for ( auto fn : retorder )
        changeReturn( fn );
    subst.clean( m );
}

void Substitution::process( llvm::Value * val ) {
    using Values = std::vector< llvm::Value * >;
    auto succs = [&] ( llvm::Value * v ) -> Values {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( v ) ) {
            auto fn = call->getCalledFunction();
            if ( !isAbstract( fn->getReturnType() ) && !isIntrinsic( fn ) )
                return {};
        }
        return { v->user_begin(), v->user_end() };
    };

    auto deps = analysis::postorder( Values{ val }, succs );
    for ( auto dep : lart::util::reverse( deps ) )
        if( auto i = llvm::dyn_cast< llvm::Instruction >( dep ) )
            subst.process( i );
}

void Substitution::changeReturn( llvm::Function * fn ) {
    subst.clean( fn );
    if ( !subst.visited( fn ) ) {
        auto newfn = changeReturnType( fn,
             subst.process( fn->getReturnType() ) );
        subst.store( fn, newfn );
    }

    for ( auto user : fn->users() ) {
        subst.changeCallFunction( llvm::cast< llvm::CallInst >( user ) );
        for ( auto val : user->users() )
            process( val );
    }
}

} // namespace abstract
} // namespace lart
