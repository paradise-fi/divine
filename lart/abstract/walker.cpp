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

struct ValuesPostOrder {
    ValuesPostOrder( const AbstractValues & roots )
        : roots( roots ) {}

    auto succs() const {
        return [&] ( const AbstractValue& n ) -> AbstractValues {
            // do not propagate through calls that are not in roots
            // i.e. that are those calls which do not return an abstract value
            bool root = std::find( roots.begin(), roots.end(), n ) != roots.end();
            if ( llvm::CallSite( n.value ) && !root )
                return {};
            if ( !n.isAbstract() )
                return {};

            return query::query( n.value->users() )
            .map( [&] ( const auto & val ) -> AbstractValue {
                return { val, n.domain };
            } ).freeze();
        };
    }

    AbstractValues get() const {
        return lart::analysis::postorder( roots, succs() );
    }

    const AbstractValues & roots;
};

// FunctionNode
AbstractValues FunctionNode::postorder() const {
    AbstractValues unordered = { origins.begin(), origins.end() };
    return ValuesPostOrder( unordered ).get();
}

// Walker
std::vector< FunctionNodePtr > Walker::postorder() const {
    auto succs = [] ( const FunctionNodePtr& f ) { return f->succs; };
    return analysis::postorder( functions, succs );
}

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
    for ( auto & a : annotations( m ) ) {
        auto fn = a.getFunction();
        record( fn );

        auto key = std::vector< Domain >( fn->arg_size(), Domain::LLVM );
        reached[ fn ].argRoots[ key ] = {};

        dispach( Propagate( a, reached[ fn ].annRoots, nullptr ) );
    }
    while ( !tasks.empty() ) {
        auto t = tasks.front();
        if ( auto p = std::get_if< Propagate >( &t ) ) {
            propagate( *p );
        }
        tasks.pop_front();
    }
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

void VPA::propagate( const Propagate & t ) {
    if ( t.roots.count( t.value ) )
        return; // value has been already propagated in this RootsSet
    auto rf = reachFrom( t.value );
    for ( auto & v : lart::util::reverse( rf ) ) {
        if ( v.isa< llvm::AllocaInst >() ) {
            t.roots.insert( v );
        } else if ( auto s = v.safeGet< llvm::StoreInst >() ) {
            auto av = AbstractValue( s->getPointerOperand(), v.domain );
            dispach( Propagate( av, t.roots, t.parent ) );
        }
    }
}

} // namespace abstract
} // namespace lart
