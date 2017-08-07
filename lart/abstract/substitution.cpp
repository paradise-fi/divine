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

auto CallDomainFilter( std::string name ) {
    return [&name]( llvm::CallInst * call ) -> bool {
        auto fn = call->getCalledFunction();
        return fn != nullptr && ( intrinsic::name( call ) == name );
    };
};

template < typename Filter >
auto identify( llvm::Module &m, Filter filter ) {
    return query::query( m ).flatten().flatten()
        .map( query::refToPtr )
        .map( query::llvmdyncast< llvm::CallInst > )
        .filter( query::notnull )
        .filter( filter )
        .freeze();
}

void removeInvalidAttributes( llvm::Function* fn ) {
    using AttrSet = llvm::AttributeSet;
    using Kind = llvm::Attribute::AttrKind;

    auto removeAttributes = [&] ( size_t i, const auto& attrs ) {
        auto rem = AttrSet::get( fn->getContext(), i, attrs );
        fn->removeAttributes( i, rem );
    };
    auto attrs = llvm::ArrayRef< Kind >{ Kind::SExt, Kind::ZExt };

    if ( types::isAbstract( fn->getReturnType() ) )
        removeAttributes( 0, attrs );
    for ( size_t i = 0; i < fn->arg_size(); ++i )
        if ( types::isAbstract( fn->getFunctionType()->getParamType( i ) ) )
            removeAttributes( i + 1, attrs );
}

} //empty namespace

llvm::PreservedAnalyses Substitution::run( llvm::Module & m ) {
    init( m );

    auto functions = query::query( m )
        .map( query::refToPtr )
        .filter( []( llvm::Function * f ) {
            return f->hasName() && !f->getName().startswith( "lart." );
        } )
        .filter( []( llvm::Function * f ) {
            return query::query( f->args() )
                .any( [] ( auto& arg ) { return types::isAbstract( arg.getType() ); } );
        } ).freeze();
    // TODO solve returns of functions without arguments
    // = move changing of returns to here
    for ( const auto & fn : functions )
        builder.process( fn );

    auto allocas = identify( m, CallDomainFilter( "alloca" ) );
    auto lifts = identify( m, CallDomainFilter( "lift" ) );

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
        funToValMap[ fn ].push_back( inst );
    }

    for ( auto & fn : funToValMap ) {
        if ( fn.first->hasName() && fn.first->getName().startswith( "lart." ) )
            continue;
        removeInvalidAttributes( fn.first );
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
    std::unique_ptr< Common > abstraction;
    switch ( domain ) {
        case Domain::Value::Trivial: {
            abstraction = std::make_unique< Trivial >();
            break;
        }
        case Domain::Value::Zero: {
            abstraction = std::make_unique< Zero >( m );
            break;
        }
        case Domain::Value::Symbolic: {
            abstraction = std::make_unique< Symbolic >( m );
            break;
        }
        default:
            UNREACHABLE( "Trying to use unsupported domain in substitution." );
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
