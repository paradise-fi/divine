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

llvm::StoreInst * isArgStoredTo( llvm::Value * v ) {
    auto stores = llvmFilter< llvm::StoreInst >( v->users() );
    for ( auto & s : stores )
        if ( llvm::isa< llvm::Argument >( s->getValueOperand() ) )
            return s;
    return nullptr;
}

using Path = AbstractFields< llvm::Value * >::Path;

Path createFieldPath( llvm::LoadInst * load ) {
    return { load->getPointerOperand(), load, { LoadStep{} } };
}

Path createFieldPath( llvm::StoreInst * store ) {
    return { store->getPointerOperand(), store->getValueOperand(), { LoadStep{} } };
}

Path createFieldPath( llvm::GetElementPtrInst * gep ) {
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

VPA::Roots VPA::run( llvm::Module & m ) {
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

    // TODO cleanup - remove unnecessery prototypes
    fields.clean();
    return std::make_tuple( std::move( reached ), globals, std::move( fields ) );
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
    auto cs = si.parent->callsite;
    auto fn = cs.getCalledFunction();
    if ( fn->isIntrinsic() )
        return;
    auto args = abstractArgs( si.parent->roots, cs );
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

    for ( const auto & av : args ) {
        auto roots = reached[ fn ].argDepRoots( doms );
        auto arg = av.get< llvm::Argument >();
        auto op = cs.getInstruction()->getOperand( arg->getArgNo() );
        if ( !isScalarType( op->getType() ) )
            fields.alias( op, arg ); // TODO maybe copy
        dispach( PropagateDown( av, roots, si.parent ) );
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

llvm::Value * VPA::origin( llvm::Value * val ) {
    llvm::Value * curr = val, * prev = nullptr;
    while ( curr != prev ) {
        prev = curr;
        if ( auto g = llvm::dyn_cast< llvm::GetElementPtrInst >( curr ) ) {
            fields.insert( createFieldPath( g ) );
            curr = g->getPointerOperand();
        } else if ( auto l = llvm::dyn_cast< llvm::LoadInst >( curr ) ) {
            fields.insert( createFieldPath( l ) );
            curr = l->getPointerOperand();
        }
    }
    return curr;
}

void VPA::propagateFromGEP( llvm::GetElementPtrInst * gep, Domain dom, RootsSet * roots, ParentPtr parent ) {
    auto o = origin( gep );
    // TODO set domain
    dispach( PropagateDown( AbstractValue{ o, dom }, roots, parent ) );
}

void VPA::propagatePtrOrStructDown( const PropagateDown & t ) {
    auto val = t.value.value;
    assert( !isScalarType( val ) );

    auto rf = reachFrom( t.value );
    rf = query::query( rf ).filter( [] ( const auto & av ) {
        if ( Load( av ) )
            return true;
        if ( auto ret = Ret( av ) )
            return true;
        if ( CallSite( av ) )
            return true; // maybe check args and ret
        return !isScalarType( av.value );
    } ).freeze();

    for ( auto & av : lart::util::reverse( rf ) ) {
        if ( auto a = Alloca( av ) ) {
            t.roots->insert( av );
            fields.create( a );

            if ( isScalarType( a->getAllocatedType() ) ) {
                Path path = { a, { LoadStep{} } };
                if ( !fields.getDomain( path ) )
                    fields.setDomain( path, t.value.domain );
            }
        }
        else if ( Argument( av ) ) {
            reached[ getFunction( av.value ) ].insert( av, t.roots );
        }
        else if ( auto l = Load( av ) ) {
            Path path = createFieldPath( l );
            fields.addPath( path );

            if ( isScalarType( l ) ) {
                if ( auto dom = fields.getDomain( l ) ) {
                    AbstractValue v = { l, dom.value() };
                    dispach( PropagateDown( v, t.roots, t.parent ) );
                }
            }
        }
        else if ( auto s = Store( av ) ) {
            Path path = createFieldPath( s );
            auto ptr = s->getPointerOperand();
            // TODO copy
            fields.create( ptr );
            fields.addPath( path );

            if ( !isScalarType( s->getValueOperand() ) ) {
                auto o = origin( ptr );
                dispach( PropagateDown( { o, Domain::Symbolic }, t.roots, t.parent ) );
            }
        }
        else if ( auto gep = GEP( av ) ) {
            auto path = createFieldPath( gep );
            fields.addPath( path );
        }
        else if ( auto bc = BitCast( av ) ) {
            fields.alias( bc->getOperand( 0 ), bc );
        }
        else if ( auto phi = PHINode( av ) ) {
            for ( auto & v : phi->incoming_values() ) {
                if ( fields.has( v ) ) {
                    fields.alias( v,  phi );
                    break;
                }
            }
        }
        else if ( auto mem = MemIntrinsic( av ) ) {
            auto dest = mem->getDest();
            // TODO copy
            fields.alias( mem->getOperand( 1 ), dest );
            dispach( PropagateDown( AbstractValue{ dest, av.domain }, t.roots, t.parent ) );
        }
        else if ( auto cs = CallSite( av ) ) {
            if ( cs.getInstruction() == val )
                t.roots->insert( av ); // we are propagating from this call
            else
                dispach( StepIn( make_parent( cs, t.parent, t.roots ) ) );
        }
        else if ( Ret( av ) ) {
            assert( Ret( av )->getReturnValue()->getType()->isSingleValueType() &&
                    "We don't know to return abstract struct type." );
            if ( fields.has( av.value ) )
                dispach( StepOut( getFunction( av.value ), av.domain, t.parent ) );
        }
    }
}

void VPA::propagateIntDown( const PropagateDown & t ) {
    auto isRoot = [&] ( const auto & v ) { return v == t.value.value; };
    auto rf = reachFrom( t.value );
    for ( auto & av : lart::util::reverse( rf ) ) {
        auto dom = av.domain;
        assert( dom != Domain::Undefined );

        if ( Argument( av )  ) {
            reached[ getFunction( av.value ) ].insert( av, t.roots );
        }
        else if ( GEP( av ) ) {
            t.roots->insert( av );
        }
        else if ( auto l = Load( av ) ) {
            if ( isRoot( l ) ) {
                auto a = llvm::dyn_cast< llvm::AllocaInst >( l->getPointerOperand() );
                if ( a && a->getAllocatedType()->isIntegerTy() )
                    continue; // root of this load is alloca
                if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( l->getPointerOperand() ) )
                    t.roots->insert( { gep, dom } );
                else
                    t.roots->insert( av );
            }
        }
        else if ( auto s = Store( av ) ) {
            auto ptr = s->getPointerOperand();
            Path path = { ptr, { LoadStep{} } };
            AbstractValue root = { ptr, dom };

            if ( !fields.has( ptr ) )
                fields.create( ptr );
            if ( !fields.getDomain( path ) )
                fields.setDomain( path, dom );

            auto o = origin( ptr );
            dispach( PropagateDown( { o,  dom }, t.roots, t.parent ) );
        }
        else if ( auto l = Load( av ) ) {
            if ( llvm::isa< llvm::GlobalValue >( l->getPointerOperand() ) )
                t.roots->insert( av );
        }
        else if ( auto phi = PHINode( av ) ) {
            for ( auto & v : phi->incoming_values() ) {
                if ( fields.has( v ) ) {
                    fields.alias( v,  phi );
                    break;
                }
            }
        }
        else if ( auto cs = CallSite( av ) ) {
            if ( isRoot( cs.getInstruction() ) )
                t.roots->insert( av ); // we are propagating from this call
            else
                dispach( StepIn( make_parent( cs, t.parent, t.roots ) ) );
        }
        else if ( Ret( av ) ) {
            dispach( StepOut( getFunction( av.value ), dom, t.parent ) );
        }
    }
}

void VPA::markGlobal( llvm::GlobalValue * value ) {
    auto dom = fields.getDomain( { value, { LoadStep{} } } );
    assert( dom );
    AbstractValue av = { value, dom.value() };

    if ( globals.count( av ) )
        return;
    globals.insert( av );

    for ( const auto & u : value->users() ) {
        if ( auto l = llvm::dyn_cast< llvm::LoadInst >( u ) ) {
            auto fn = getFunction( l );
            if ( !reached.count( fn ) )
                record( fn );
            AbstractValue al = { l, dom.value() };
            dispach( PropagateDown( al, reached[ fn ].annotations(), nullptr ) );
        }
    }
}

void VPA::propagateDown( const PropagateDown & t ) {
    auto val = t.value.value;
    auto dom = t.value.domain;
    if ( seen[ t.roots ].count( val ) )
        return; // The value has been already propagated.

    if ( auto g = llvm::dyn_cast< llvm::GlobalValue >( val ) ) {
        markGlobal( g );
        return;
    }

    if ( !llvm::CallSite( val ) ) {
        auto o = origin( val );
        if ( auto arg = Argument( t.value ) ) {
            if ( arg->getType()->isPointerTy() )
                dispach( PropagateUp( arg, t.roots, t.parent ) );
        } else if ( auto s = isArgStoredTo( o ) ) {
            auto vop = s->getValueOperand();
            auto arg = llvm::cast< llvm::Argument >( vop );
            if ( arg->getType()->isPointerTy() ) {
                // TODO copy trie
                Path path = { arg, { LoadStep{} } };
                if ( !fields.getDomain( path ) ) {
                    // TODO use copy
                    fields.create( arg );
                    fields.setDomain( path, dom );
                }

                dispach( PropagateDown( AbstractValue{ o, dom }, t.roots, t.parent ) );
                dispach( PropagateUp( arg, t.roots, t.parent ) );
            }
        }
    }

    if ( val->getType()->isIntegerTy() )
        propagateIntDown( t );
    else
        propagatePtrOrStructDown( t );

    seen[ t.roots ].insert( val );
}

void VPA::propagateUp( const PropagateUp & t ) {
    assert( !isScalarType( t.arg ) );
    assert( t.arg->getType()->isPointerTy() && "Propagating up non pointer type is forbidden." );

    Path path = { t.arg, { LoadStep{} } };
    // FIXME getting correct domain
    auto dom = fields.getDomain( path ) ? fields.getDomain( path ).value() : Domain::Symbolic;

    auto av =  AbstractValue{ t.arg, dom };
    auto ano = t.arg->getArgNo();

    if ( t.parent ) {
        auto r = reachFrom( { t.parent->roots->begin(), t.parent->roots->end() } );
        auto arg = t.parent->callsite->getOperand( ano );
        auto abs = std::find_if( r.begin(), r.end(), [&] ( const auto & a ) {
            return a.value == arg;
        } );
        if ( abs != r.end() )
            return; // already abstracted
    }

    reached[ getFunction( av.value ) ].insert( av, t.roots );

    auto propagate = [&] ( auto op, auto roots, auto parent ) {
        fields.alias( t.arg, op );
        auto o = origin( op );
        auto root = AbstractValue{ o, dom };
        dispach( PropagateDown( root, roots, parent ) );
    };

    if ( t.parent ) {
        auto op = t.parent->callsite->getOperand( ano );
        propagate( op, t.parent->roots, t.parent->parent );
    } else {
        for ( auto u : t.arg->getParent()->users() ) {
            auto fn = getFunction( u );
            record( fn );
            auto op = u->getOperand( ano );
            propagate( op, reached[ fn ].annotations(), nullptr );
        }
    }
}

} // namespace abstract
} // namespace lart
