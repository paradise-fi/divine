// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/substitution.h>

#include <lart/abstract/types.h>
#include <lart/abstract/walker.h>
#include <lart/abstract/intrinsic.h>
#include <lart/support/query.h>
#include <lart/analysis/postorder.h>

#include <iterator>

namespace lart {
namespace abstract {
namespace {

auto argEntries( llvm::Module & m )
    -> std::map< llvm::Function *, std::vector< llvm::Argument * > >
{
    auto args = query::query( m )
                .map( []( llvm::Function & f ) {
                    return f.args();
                } ).flatten()
                .map( query::refToPtr )
                .filter( [&] ( llvm::Value * v ) {
                    return types::isAbstract( v->getType() );
                } )
                .freeze();

    std::map< llvm::Function *, std::vector< llvm::Argument * > > funToArgMap;

    for ( const auto &arg : args ) {
        llvm::Function * fn = arg->getParent();
        assert( fn != nullptr );
        if ( fn->hasName() && fn->getName().startswith( "lart." ) )
            continue;
        if ( funToArgMap.count( fn ) )
            funToArgMap[ fn ].push_back( arg );
        else
            funToArgMap[ fn ] = { arg };
    }
    return funToArgMap;
}

auto CallDomainFilter( std::string name ) {
    return [&name]( llvm::CallInst * call ) -> bool {
        auto fn = call->getCalledFunction();
        return fn != nullptr && ( intrinsic::name( call ) == name );
    };
};

template < typename Value, typename Filter >
auto identify( llvm::Module &m, Filter filter ) -> std::vector< Value * > {
     return query::query( m ).flatten().flatten()
                  .map( query::refToPtr )
                  .map( query::llvmdyncast< Value > )
                  .filter( query::notnull )
                  .filter( filter )
                  .freeze();
}

} //empty namespace

llvm::PreservedAnalyses Substitution::run( llvm::Module & m ) {
    init( m );
    // process arguments
    auto entries = argEntries( m );
    std::vector< llvm::Function * > unordered;
    for ( auto it : entries )
        unordered.push_back( it.first );
    auto order = analysis::callPostorder< llvm::Function * >( m, unordered );

    for ( auto & fn : order ) {
        builder.process( fn );
        for ( auto & arg : entries[ fn ] ) {
            auto newarg = &(*std::next( fn->arg_begin(), arg->getArgNo() ));
            builder.store( newarg, newarg );
            process( newarg );
        }
    }

    //process allocas and lifts and entries
    auto allocaFilter = CallDomainFilter( "alloca" );
    auto allocas = identify< llvm::CallInst, decltype(allocaFilter) >( m, allocaFilter );

    auto liftFilter = CallDomainFilter( "lift" );
    auto lifts = identify< llvm::CallInst, decltype(liftFilter) >( m, liftFilter );

    std::vector< llvm::Instruction * > abstract;
    abstract.reserve( allocas.size() + lifts.size() );
    abstract.insert( abstract.end(), allocas.begin(), allocas.end() );
    abstract.insert( abstract.end(), lifts.begin(), lifts.end() );

    std::map< llvm::Function *, std::vector< llvm::Instruction * > > funToValMap;
    for ( const auto &a : abstract ) {
        auto inst = llvm::dyn_cast< llvm::Instruction >( a );
        assert( inst );

        llvm::Function * fn = inst->getParent()->getParent();
        if ( builder.stored( fn ) )
            continue;
        if ( funToValMap.count( fn ) )
            funToValMap[ fn ].push_back( inst );
        else
            funToValMap[ fn ] = { inst };
    }

    for ( auto & fn : funToValMap ) {
        if ( fn.first->hasName() && fn.first->getName().startswith( "lart." ) )
            continue;
        for ( auto & inst : fn.second )
            process( inst );
    }

    // FIXME maybe can be done at the start - more optimally
    auto retAbsVal = query::query( m )
                    .map( query::refToPtr )
                    .filter( [&]( llvm::Function * fn ) {
                        return types::isAbstract( fn->getReturnType() )
                            && ! intrinsic::is( fn );
                    } )
                    .filter( [&]( llvm::Function * fn ) {
                        return !builder.stored( fn )
                             || funToValMap.count( fn );
                    } ).freeze();

    auto retorder = analysis::callPostorder< llvm::Function * >( m, retAbsVal );

    for ( auto fn : retorder )
        changeReturn( fn );
    builder.clean( m );

    return llvm::PreservedAnalyses::none();
}

void Substitution::init( llvm::Module & m ) {
    std::unique_ptr< abstract::Common > abstraction;
    switch ( type ) {
        case Trivial: {
            abstraction = std::make_unique< abstract::Trivial >();
            break;
        }
        case Zero: {
            abstraction = std::make_unique< abstract::Zero >( m );
            break;
        }
        case Symbolic: {
            abstraction = std::make_unique< abstract::Symbolic >( m );
            break;
        }
    }

    builder = SubstitutionBuilder( std::move( abstraction ) );
}

void Substitution::process( llvm::Value * val ) {
    auto succs = [&] ( llvm::Value * v ) -> std::vector< llvm::Value * > {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( v ) ) {
            auto fn = call->getCalledFunction();
            if ( !intrinsic::is( fn ) && !types::isAbstract( fn->getReturnType() ) )
                return {};
        }
        return { v->user_begin(), v->user_end() };
    };
    auto deps = analysis::postorder< llvm::Value * >( { val }, succs );
    for ( auto dep : lart::util::reverse( deps ) )
        if( auto i = llvm::dyn_cast< llvm::Instruction >( dep ) )
            builder.process( i );
}

void Substitution::changeReturn( llvm::Function * fn ) {
    builder.clean( fn );
    if ( ! builder.stored( fn ) ) {
        auto newfn = changeReturnType( fn,
             builder.abstraction->abstract( fn->getReturnType() ) );
        builder.store( fn, newfn );
    }

    for ( auto user : fn->users() ) {
        builder.changeCallFunction( llvm::cast< llvm::CallInst >( user ) );
        for ( auto val : user->users() )
            process( val );
    }
}

} // namespace abstract
} // namespace lart
