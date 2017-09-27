// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/walker.h>

DIVINE_RELAX_WARNINGS
#include <llvm/ADT/Hashing.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <iterator>

#include <lart/support/util.h>
#include <lart/support/query.h>
#include <lart/support/lowerselect.h>
#include <lart/abstract/value.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

namespace {

AbstractValue getValueFromAnnotation( const llvm::CallInst * call ) {
    auto data = llvm::cast< llvm::GlobalVariable >(
                call->getOperand( 1 )->stripPointerCasts() );
    auto annotation = llvm::cast< llvm::ConstantDataArray >(
                      data->getInitializer() )->getAsCString();
    const std::string prefix = "lart.abstract.";
    assert( annotation.startswith( prefix ) );
    auto dom = DomainTable[ annotation.str().substr( prefix.size() ) ];
    return AbstractValue( call->getOperand( 0 )->stripPointerCasts(), dom );
}

AbstractValues annotations( llvm::Module & m ) {
    std::vector< std::string > prefixes = {
        "llvm.var.annotation",
        "llvm.ptr.annotation"
    };

    AbstractValues values;

    for ( const auto & pref : prefixes ) {
        auto annotated = query::query( m )
            .map( query::refToPtr )
            .filter( [&]( llvm::Function * fn ) {
                return fn->getName().startswith( pref );
            } ).freeze();

        for ( auto a : annotated ) {
            Values users = { a->user_begin(), a->user_end() };
            for ( const auto & call : llvmFilter< llvm::CallInst >( users ) )
                values.push_back( getValueFromAnnotation( call ) );
        }
    }

    return values;
}

AbstractValues abstractArgs( const AbstractValues & reached, const llvm::CallSite & cs ) {
    auto fn = cs.getCalledFunction();
    AbstractValues res;
    for ( size_t i = 0; i < cs.getNumArgOperands(); ++i ) {
        auto find = std::find_if( reached.begin(), reached.end(), [&] ( const auto & a ) {
            return a.value == cs.getArgOperand( i );
        } );
        if ( find != reached.end() )
            res.emplace_back( std::next( fn->arg_begin(), i ), find->domain );
    }
    return res;
}

ArgDomains argDomains( const AbstractValues & args ) {
    if ( args.empty() )
        return {};
    auto fn = getFunction( args[ 0 ].value );
    ArgDomains doms = ArgDomains( fn->arg_size(),  Domain::LLVM );
    for ( const auto & a : args )
        doms[ a.get< llvm::Argument >()->getArgNo() ] = a.domain;
    return doms;
};

AbstractValues abstractArgs( const RootsSet & roots, const llvm::CallSite & cs ) {
    return abstractArgs( reachFrom( { roots.begin(), roots.end() } ), cs );
}

} // anonymous namespace


void VPA::preprocess( llvm::Function * fn ) {
    auto lowerSwitchInsts = std::unique_ptr< llvm::FunctionPass >(
                            llvm::createLowerSwitchPass() );
    lowerSwitchInsts.get()->runOnFunction( *fn );

    LowerSelect lowerSelectInsts;
    lowerSelectInsts.runOnFunction( *fn );

    llvm::UnifyFunctionExitNodes ufen;
    ufen.runOnFunction( *fn );
}

Reached VPA::run( llvm::Module & m ) {
    for ( const auto & a : annotations( m ) ) {
        auto fn = a.getFunction();
        record( fn );

        auto key = ArgDomains( fn->arg_size(), Domain::LLVM );
        reached[ fn ].argRoots[ key ] = {};

        dispach( Propagate( a, reached[ fn ].annRoots, nullptr ) );
    }

    while ( !tasks.empty() ) {
        auto t = tasks.front();
        if ( auto p = std::get_if< Propagate >( &t ) )
            propagate( *p );
        else if ( auto si = std::get_if< StepIn >( &t ) )
            stepIn( *si );
        else if ( auto so = std::get_if< StepOut >( &t ) )
            stepOut( *so );
        tasks.pop_front();
    }

    // TODO cleanup

    return reached;
}

void VPA::record( llvm::Function * fn ) {
    if ( !reached.count( fn ) ) {
        preprocess( fn );
        reached[ fn ] = FunctionRoots();
    }
}

void VPA::dispach( Task && task ) {
    if ( std::find( tasks.begin(), tasks.end(), task ) == tasks.end() )
        tasks.emplace_back( std::move( task ) );
}

void VPA::stepIn( const StepIn & si ) {
    auto fn = si.parent->callsite.getCalledFunction();
    if ( fn->isIntrinsic() )
        return;
    auto args = abstractArgs( si.parent->roots, si.parent->callsite );
    if ( reached.count( fn ) ) {
        auto doms = argDomains( args );
        if ( reached[ fn ].argRoots.count( doms ) ) {
            auto rs = reached[ fn ].argRoots[ doms ];
            auto dom = returns( fn, rs );
            if ( dom != Domain::LLVM )
                dispach( StepOut( fn, dom, si.parent ) );
            return; // We have already seen 'fn' with this abstract signature
        }
    } else {
        record( fn );
    }

    auto doms = argDomains( args );
    for ( const auto & arg : args )
        dispach( Propagate( arg, reached[ fn ].argRoots[ doms ], si.parent ) );
}

void VPA::stepOut( const StepOut & so ) {
    if ( auto p = so.parent ) {
        AbstractValue av{ p->callsite.getInstruction(), so.domain };
        dispach( Propagate( av, p->roots, p->parent ) );
    } else {
        for ( const auto & u : so.function->users() ) {
            if ( auto cs = llvm::CallSite( u ) ) {
                AbstractValue av{ cs.getInstruction(), so.domain };
                auto fn = getFunction( cs.getInstruction() );
                if ( !reached.count( fn ) ) {
                    record( fn );
                    auto key = ArgDomains( fn->arg_size(), Domain::LLVM );
                    reached[ fn ].argRoots[ key ] = {};
                }
                dispach( Propagate( av, reached[ fn ].annRoots, nullptr ) );
            }
        }
    }
}

Domain VPA::returns( llvm::Function * fn, const RootsSet & rs ) {
    // TODO cache return results
    auto roots = unionRoots( reached[ fn ].annRoots, rs );
    auto rf = reachFrom( { roots.begin(), roots.end() } );
    for ( auto & v : lart::util::reverse( rf ) )
        if ( v.isa< llvm::ReturnInst >() )
            return v.domain;
    return Domain::LLVM;
}

void VPA::propagate( const Propagate & t ) {
    if ( t.roots.count( t.value ) )
        return; // value has been already propagated in this RootsSet
    auto rf = reachFrom( t.value );
    for ( auto & v : lart::util::reverse( rf ) ) {
        if ( v.isa< llvm::AllocaInst >() ) {
            t.roots.insert( v );
        } else if ( v.isa< llvm::Argument >() ) {
            t.roots.insert( v );
        }  else if ( auto s = v.safeGet< llvm::StoreInst >() ) {
            auto av = AbstractValue( s->getPointerOperand(), v.domain );
            dispach( Propagate( av, t.roots, t.parent ) );
        } else if ( auto cs = llvm::CallSite( v.value ) ) {
            if ( t.value.value != v.value )
                dispach( StepIn( make_parent( cs, t.parent, t.roots ) ) );
            else
                t.roots.insert( v ); // we are propagationg from this call
        } else if ( v.isa< llvm::ReturnInst >() ) {
            dispach( StepOut( getFunction( v.value ), v.domain, t.parent ) );
        }
    }
}

} // namespace abstract
} // namespace lart
