// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/vpa.h>

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

AbstractValues abstractArgs( const RootsSet * roots, const llvm::CallSite & cs ) {
    return abstractArgs( reachFrom( { roots->cbegin(), roots->cend() } ), cs );
}

llvm::Value * origin( llvm::Value * value  ) {
    if ( auto a = llvm::dyn_cast< llvm::AllocaInst >( value ) )
        return a;
    if ( auto a = llvm::dyn_cast< llvm::Argument >( value ) )
        return a;
    if ( auto l = llvm::dyn_cast< llvm::LoadInst >( value ) )
        return origin( l->getPointerOperand() );
    if ( auto g = llvm::dyn_cast< llvm::GetElementPtrInst >( value ) )
        return origin( g->getPointerOperand() );
    if ( auto c = llvm::dyn_cast< llvm::CastInst >( value ) )
        return origin( c->getOperand( 0 ) );
    UNREACHABLE( "Dont know how to get root!." );
}

llvm::Argument * isArgStoredTo( llvm::Value * v ) {
    if ( auto a = llvm::dyn_cast< llvm::Argument >( v ) )
        return a;

    auto stores = llvmFilter< llvm::StoreInst >( v->users() );
    for ( auto & s : stores )
        if ( auto a = llvm::dyn_cast< llvm::Argument >( s->getValueOperand() ) )
            return a;
    return nullptr;
}

ValueField< llvm::Value * > createFieldPath( llvm::LoadInst * load ) {
    return { load->getPointerOperand(), load, { LoadStep{} } };
}

ValueField< llvm::Value * > createFieldPath( llvm::GetElementPtrInst * gep ) {
    auto inds = query::query( gep->idx_begin(), gep->idx_end() )
        .map ( [] ( const auto & idx ) -> Step {
            return llvm::cast< llvm::ConstantInt >( idx )->getZExtValue();
        } ).freeze();
    return { gep->getPointerOperand(), gep, inds };
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
        dispach( PropagateDown( a, reached[ fn ].annotations(), nullptr ) );
    }

    while ( !tasks.empty() ) {
        auto t = tasks.front();
        if ( auto pd = std::get_if< PropagateDown >( &t ) )
            propagateDown( *pd );
        else if ( auto pu = std::get_if< PropagateUp >( &t ) )
            propagateUp( *pu );
        else if ( auto si = std::get_if< StepIn >( &t ) )
            stepIn( *si );
        else if ( auto so = std::get_if< StepOut >( &t ) )
            stepOut( *so );
        tasks.pop_front();
    }

    // TODO cleanup

    return std::move( reached );
}

void VPA::record( llvm::Function * fn ) {
    if ( !reached.count( fn ) ) {
        preprocess( fn );
        reached[ fn ].init( fn->arg_size() );
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
    auto doms = argDomains( args );
    if ( reached.count( fn ) ) {
        if ( reached[ fn ].has( doms ) ) {
            auto dom = reached[ fn ].returns( doms );
            if ( dom != Domain::LLVM )
                dispach( StepOut( fn, dom, si.parent ) );
            return; // We have already seen 'fn' with this abstract signature
        }
    } else {
        record( fn );
    }

    for ( const auto & arg : args ) {
        auto roots = reached[ fn ].argDepRoots( doms );
        dispach( PropagateDown( arg, roots, si.parent ) );
    }
}

void VPA::stepOut( const StepOut & so ) {
    if ( auto p = so.parent ) {
        AbstractValue av{ p->callsite.getInstruction(), so.domain };
        dispach( PropagateDown( av, p->roots, p->parent ) );
    } else {
        for ( const auto & u : so.function->users() ) {
            if ( auto cs = llvm::CallSite( u ) ) {
                AbstractValue av{ cs.getInstruction(), so.domain };
                auto fn = getFunction( cs.getInstruction() );
                if ( !reached.count( fn ) )
                    record( fn );
                dispach( PropagateDown( av, reached[ fn ].annotations(), nullptr ) );
            }
        }
    }
}

void VPA::propagateFromGEP( llvm::GetElementPtrInst * gep, Domain dom, RootsSet * roots, ParentPtr parent ) {
    llvm::Value * curr = gep, * prev = nullptr;

    while ( curr != prev ) {
        prev = curr;
        if ( auto g = llvm::dyn_cast< llvm::GetElementPtrInst >( curr ) ) {
            fields.insert( createFieldPath( g ), dom );
            curr = g->getPointerOperand();
        }
    }

    // curr is now origin of gep
    assert( llvm::isa< llvm::AllocaInst >( curr ) );
    dispach( PropagateDown( AbstractValue{ curr, Domain::Undefined }, roots, parent ) );
}

void VPA::propagateStructDown( const PropagateDown & t ) {
    auto rf = reachFrom( t.value );
    for ( auto & v : lart::util::reverse( rf ) ) {
        if ( auto gep = v.safeGet< llvm::GetElementPtrInst >() ) {
            if ( auto dom = fields.getDomain( createFieldPath( gep ) ) ) {
                auto av = AbstractValue{ gep, dom.value() };
                t.roots->insert( av );
                dispach( PropagateDown( av, t.roots, t.parent ) );
            }
        }
        if ( auto bc = v.safeGet< llvm::BitCastInst >() ) {
            fields.alias( bc->getOperand( 0 ), bc );
        }
        if ( auto mem = v.safeGet< llvm::MemIntrinsic >() ) {
            auto dest = mem->getDest();
            assert( llvm::isa< llvm::AllocaInst >( dest ) );
            fields.alias( mem->getOperand( 1 ), dest );
            dispach( PropagateDown( AbstractValue{ dest, Domain::Undefined }, t.roots, t.parent ) );
        }
    }
}

void VPA::propagateScalarDown( const PropagateDown & t ) {
    if ( !llvm::CallSite( t.value.value ) ) {
        auto o = origin( t.value.value );
        if ( auto a = isArgStoredTo( o ) ) {
            dispach( PropagateDown( AbstractValue{ o, t.value.domain }, t.roots, t.parent ) );
            dispach( PropagateUp( a, t.value.domain, t.roots, t.parent ) );
        }
    }

    auto rf = reachFrom( t.value );
    for ( auto & v : lart::util::reverse( rf ) ) {
        if ( v.isa< llvm::AllocaInst >() ) {
            t.roots->insert( v );
        } else if ( v.isa< llvm::Argument >() ) {
            reached[ getFunction( v.value ) ].insert( v, t.roots );
        } else if ( v.isa< llvm::GetElementPtrInst >() ) {
            t.roots->insert( v );
        } else if ( auto s = v.safeGet< llvm::StoreInst >() ) {
            auto av = AbstractValue( s->getPointerOperand(), v.domain );
            if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( s->getPointerOperand() ) )
                propagateFromGEP( gep, v.domain, t.roots, t.parent );
            else
                dispach( PropagateDown( av, t.roots, t.parent ) );
        } else if ( auto cs = llvm::CallSite( v.value ) ) {
            if ( t.value.value != v.value )
                dispach( StepIn( make_parent( cs, t.parent, t.roots ) ) );
            else
                t.roots->insert( v ); // we are propagating from this call
        } else if ( v.isa< llvm::ReturnInst >() ) {
            dispach( StepOut( getFunction( v.value ), v.domain, t.parent ) );
        }
    }
}

void VPA::propagateDown( const PropagateDown & t ) {
    if ( seen[ t.roots ].count( t.value.value ) ) {
        return; // The value has been already propagated.
    }
    auto type = stripPtrs( t.value.value->getType() );
    if ( type->isStructTy() )
        propagateStructDown( t );
    if ( type->isSingleValueType() )
        propagateScalarDown( t );
    seen[ t.roots ].insert( t.value.value );
}

void VPA::propagateUp( const PropagateUp & t ) {
    auto av =  AbstractValue{ t.arg, t.domain };
    if ( t.parent ) {
        auto r = reachFrom( { t.parent->roots->begin(), t.parent->roots->end() } );
        auto arg = t.parent->callsite->getOperand( t.arg->getArgNo() );
        auto abs = std::find_if( r.begin(), r.end(), [&] ( const auto & a ) {
            return a.value == arg;
        } );
        if ( abs != r.end() )
            return; // already abstracted
    }

    reached[ getFunction( av.value ) ].insert( av, t.roots );

    assert( t.arg->getType()->isPointerTy() && "Propagating up non pointer type is forbidden.");
    if ( t.parent ) {
        auto o = origin( t.parent->callsite->getOperand( t.arg->getArgNo() ) );
        auto root = AbstractValue{ o, t.domain };
        dispach( PropagateDown( root, t.parent->roots, t.parent->parent ) );
    } else {
        for ( auto u : t.arg->getParent()->users() ) {
            auto fn = getFunction( u );
            record( fn );

            auto o = origin( u->getOperand( t.arg->getArgNo() ) );
            auto root = AbstractValue{ o, t.domain };
            dispach( PropagateDown( root, reached[ fn ].annotations(), nullptr ) );
        }
    }
}

} // namespace abstract
} // namespace lart
