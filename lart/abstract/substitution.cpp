// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/substitution.h>

#include <lart/abstract/vpa.h>
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

decltype(auto) callSitesOf( llvm::Function * fn ) {
    return query::query( fn->users() )
        .filter( [] ( const auto & v ) {
            return llvm::CallSite( v );
        } )
        .freeze();
}

decltype(auto) callSitesOf( const Functions & fns ) {
    return query::query( fns )
        .map( [] ( const auto & fn ) { return callSitesOf( fn ); } )
        .flatten()
        .freeze();
}

template< typename A, typename B >
void concat_impl( A & lhs, B && rhs ) {
    lhs.insert( lhs.end(), std::make_move_iterator( rhs.begin() ),
                           std::make_move_iterator( rhs.end() ) );
}

template< typename... Vs >
Values concat( Vs&&... vs ) {
    Values res;
    ( concat_impl( res, std::forward< Vs >( vs ) ), ... );
    return res;
}

} // anonymous namespace

void Substitution::run( llvm::Module & m )
{
    std::vector< llvm::GlobalVariable * > globals;
    {
        // begin RAII substitution builder
        auto sb = make_sbuilder( data.tmap, fns, m );
        auto functions = abstractFunctions( m, data.tmap );

        for ( auto & fn : functions ) {
            assert( fn->getName() != "main" );
            fns[ fn ] = sb.prototype( fn );
        }

        for ( auto & g : m.globals() ) {
            if ( isAbstract( g.getType(), data.tmap ) ) {
                sb.process( &g );
                globals.push_back( &g );
            }
        }

        auto intrs = intrinsics( &m );

        auto allocas = query::query( intrs ).filter( [] ( const auto & i ) {
            return isAlloca( i );
        } ).freeze();

        auto lifts = query::query( intrs ).filter( [] ( const auto & i ) {
            return isLift( i );
        } ).freeze();

        auto btcsts = query::query( llvmFilter< llvm::BitCastInst >( &m ) )
        .filter( [&] ( const auto & i ) {
            return isAGEPCast( i, data.tmap );
        } ).freeze();

        auto loads = query::query( intrs ).filter( [] ( const auto & i ) {
            if ( isLoad( i ) ) {
                if ( llvm::isa< llvm::GlobalVariable >( i->getOperand( 0 ) ) )
                    return true;
                if ( auto bc = llvm::dyn_cast< llvm::BitCastOperator >( i->getOperand( 0 ) ) )
                    if ( llvm::isa< llvm::GlobalVariable >( bc->getOperand( 0 ) ) )
                        return true;
                if ( llvm::isa< llvm::Argument >( i->getOperand( 0 ) ) )
                    return true;
            }
            return false;
        } ).freeze();

        auto calls = callSitesOf( functions );

        auto abstract = concat( allocas, lifts, calls, btcsts, loads );

        Map< llvm::Function *, Values > funToValMap;
        for ( const auto &a : abstract )
            funToValMap[ getFunction( a ) ].push_back( a );

        auto succs = [&] ( llvm::Value * v ) -> Values {
            if ( auto cs = llvm::CallSite( v ) ) {
                auto fn = cs.getCalledFunction();
                if ( !isAbstract( fn->getReturnType(), data.tmap ) && !isIntrinsic( fn ) )
                    return {};
            }
            return { v->user_begin(), v->user_end() };
        };

        for ( auto & fn : m )
            for ( auto & arg : fn.args() )
                if ( isAbstract( arg.getType(), data.tmap ) )
                    funToValMap[ &fn ].push_back( &arg );

        for ( auto & fn : funToValMap ) {
            if ( fn.first->hasName() && fn.first->getName().startswith( "lart." ) )
                continue;
            removeInvalidAttributes( fn.first, data.tmap );

            auto deps = analysis::postorder( fn.second, succs );

            for ( const auto & dep : lart::util::reverse( deps ) ) {
                if( const auto & a = llvm::dyn_cast< llvm::Argument >( dep ) )
                    sb.process( a );
                if( const auto & i = llvm::dyn_cast< llvm::Instruction >( dep ) )
                    sb.process( i );
            }

            for ( const auto & dep : deps )
                if ( llvm::isa< llvm::PHINode >( dep ) )
                    sb.process( dep );
        }
    }

    for ( const auto & g : globals )
    {
        g->replaceAllUsesWith( llvm::UndefValue::get( g->getType() ) );
        g->eraseFromParent();
    }

    auto remapArg = [&] ( llvm::Argument & a ) {
        if ( !a.use_empty() ) {
            if ( const auto & cs = llvm::CallSite( *a.user_begin() ) )
                if ( cs.getCalledFunction()->hasName() &&
                     cs.getCalledFunction()->getName().startswith( "__lart_lift" ) )
                {
                    cs.getInstruction()->replaceAllUsesWith( &a );
                    cs.getInstruction()->eraseFromParent();
                }
            if ( auto bc = llvm::dyn_cast< llvm::BitCastInst >( *a.user_begin() ) )
                if ( bc->getSrcTy() == bc->getDestTy() ) {
                    bc->replaceAllUsesWith( &a );
                    bc->eraseFromParent();
                }
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
