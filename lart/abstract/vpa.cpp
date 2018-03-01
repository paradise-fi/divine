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
#include <lart/abstract/metadata.h>

namespace lart {
namespace abstract {

namespace {

template< typename Fields >
AbstractValues abstractArgs( const AbstractValues & reached,
                             const Fields & fields,
                             const llvm::CallSite & cs )
{
    auto fn = cs.getCalledFunction();
    AbstractValues res;
    for ( size_t i = 0; i < cs.getNumArgOperands(); ++i ) {
        auto find = std::find_if( reached.begin(), reached.end(), [&] ( const auto & a ) {
            if ( a.value == cs.getArgOperand( i ) ) {
                if ( !isScalarType( a.value->getType() ) ) {
                    if ( fields.has( a.value ) )
                        return true;
                } else {
                    return true;
                }
            }
            return false;
        } );
        auto arg = std::next( fn->arg_begin(), i );
        if ( find != reached.end() )
            res.emplace_back( arg, find->domain );
    }
    return res;
}


template< typename Fields >
AbstractValues abstractArgs( const RootsSet * roots,
                             const Fields & fields,
                             const llvm::CallSite & cs )
{
    return abstractArgs( reachFrom( { roots->cbegin(), roots->cend() } ), fields, cs );
}

llvm::StoreInst * isArgStoredTo( llvm::Value * v ) {
    auto stores = llvmFilter< llvm::StoreInst >( v->users() );
    for ( auto & s : stores )
        if ( llvm::isa< llvm::Argument >( s->getValueOperand() ) )
            return s;
    return nullptr;
}

Path createFieldPath( llvm::LoadInst * load ) {
    return { load->getPointerOperand(), load, { LoadStep{} } };
}

Path createFieldPath( llvm::StoreInst * store ) {
    return { store->getPointerOperand(), store->getValueOperand(), { LoadStep{} } };
}

Indices gepIndices( llvm::GetElementPtrInst * gep ) {
    return query::query( gep->idx_begin(), gep->idx_end() )
        .map ( [] ( const auto & idx ) -> Step {
            if ( auto c = llvm::dyn_cast< llvm::ConstantInt >( idx ) )
                return c->getZExtValue();
            throw( std::invalid_argument( "Nonconstant gep index." ) );
        } ).freeze();
}

Path createFieldPath( llvm::GetElementPtrInst * gep ) {
    return { gep->getPointerOperand(), gep, gepIndices( gep ) };
}

struct LowerConstExpr {
    static void run( llvm::Function * fn ) {
        for ( auto & bb : *fn )
            for ( auto & i : bb )
                process( &i );
    }

    static void process( llvm::Instruction * i ) {
        for ( auto & o : i->operands() ) {
            if ( auto ce = llvm::dyn_cast< llvm::ConstantExpr >( o ) ) {
                auto io = ce->getAsInstruction();
                io->insertBefore( i );
                o->replaceAllUsesWith( io );
            }
        }
    }
};


} // anonymous namespace


void VPA::preprocess( llvm::Function * fn ) {
    LowerConstExpr::run( fn );

    auto lowerSwitchInsts = std::unique_ptr< llvm::FunctionPass >(
                            llvm::createLowerSwitchPass() );
    lowerSwitchInsts.get()->runOnFunction( *fn );

    LowerSelect lowerSelectInsts;
    lowerSelectInsts.runOnFunction( *fn );

    llvm::UnifyFunctionExitNodes ufen;
    ufen.runOnFunction( *fn );
}

VPA::Roots VPA::run( llvm::Module & m ) {
    for ( const auto & mdv : abstract_metadata( m ) ) {
        auto val = mdv.value();
        if ( !( val->getType()->isIntegerTy() ) &&
             !( val->getType()->isPointerTy() &&
                val->getType()->getPointerElementType()->isIntegerTy() ) )
        {
            throw std::runtime_error( "only annotation of integer values is allowed" );
        }

        auto fn = getFunction( mdv.value() );
        record( fn );

        dispach( PropagateDown( mdv.abstract_value(), reached[ fn ].annotations(), nullptr ) );
    }

    while ( !tasks.empty() ) {
        auto t = tasks.front();
        if ( auto pd = t.template asptr< PropagateDown >() )
            propagateDown( *pd );
        else if ( auto pu = t.template asptr< PropagateUp >() )
            propagateUp( *pu );
        else if ( auto si = t.template asptr< StepIn >() )
            stepIn( *si );
        else if ( auto so = t.template asptr< StepOut >() )
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
        assert( !reached.count( fn ) );
        reached[ fn ].init();
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

    auto args = abstractArgs( si.parent->roots, fields, cs );
    auto ins = argIndices( args );
    if ( reached.count( fn ) ) {
        if ( reached[ fn ].has( ins ) ) {
            if ( auto av = reached[ fn ].returns( ins ) )
                dispach( StepOut( fn, av.value(), si.parent ) );
            return; // We have already seen 'fn' with this abstract signature
        }
    } else {
        record( fn );
    }

    auto roots = reached[ fn ].argDepRoots( ins );
    for ( const auto & av : args ) {
        auto arg = av.get< llvm::Argument >();
        auto op = cs.getInstruction()->getOperand( arg->getArgNo() );
        if ( !isScalarType( op->getType() ) )
            fields.alias( op, arg );
        dispach( PropagateDown( av, roots, si.parent ) );
    }
}

void VPA::stepOut( const StepOut & so ) {
    auto dom = so.value.domain;
    auto ret = Ret( so.value )->getReturnValue();
    if ( auto p = so.parent ) {
        auto call = p->callsite.getInstruction();
        if ( !isScalarType( ret->getType() ) )
            fields.alias( ret, call );

        AbstractValue av{ call, dom };
        dispach( PropagateDown( av, p->roots, p->parent ) );
    } else {
        for ( const auto & u : so.function->users() ) {
            if ( auto cs = llvm::CallSite( u ) ) {
                auto call = cs.getInstruction();
                if ( !isScalarType( ret->getType() ) )
                    fields.alias( ret, call );

                AbstractValue av{ call, dom };
                auto fn = getFunction( call );
                if ( !reached.count( fn ) )
                    record( fn );
                dispach( PropagateDown( av, reached[ fn ].annotations(), nullptr ) );
            }
        }
    }
}

template< typename Fields >
llvm::Value * origin( llvm::Value * val, Fields & fields ) {
    llvm::Value * curr = val, * prev = nullptr;
    while ( curr != prev ) {
        prev = curr;
        if ( auto g = llvm::dyn_cast< llvm::GetElementPtrInst >( curr ) ) {
            fields.insert( createFieldPath( g ) );
            curr = g->getPointerOperand();
        } else if ( auto l = llvm::dyn_cast< llvm::LoadInst >( curr ) ) {
            fields.insert( createFieldPath( l ) );
            curr = l->getPointerOperand();
        } else if ( auto bc = llvm::dyn_cast< llvm::BitCastInst >( curr ) ) {
            curr = bc->getOperand( 0 );
        }
    }
    return curr;
}

void VPA::propagateFromGEP( llvm::GetElementPtrInst * gep, Domain dom, RootsSet * roots, ParentPtr parent ) {
    auto o = origin( gep, fields );
    dispach( PropagateDown( AbstractValue{ o, dom }, roots, parent ) );
}

template< typename Fields >
void visitOrigins( llvm::Value * val, Fields & fields ) {
    std::vector< Path > visited;

    llvm::Value * curr = val;
    while ( true ) {
        if ( fields.has( curr ) )
            break;
        if ( auto g = llvm::dyn_cast< llvm::GetElementPtrInst >( curr ) ) {
            curr = g->getPointerOperand();
            Indices inds = gepIndices( g );
            visited.push_back( { curr, g, inds } );
        } else if ( auto l = llvm::dyn_cast< llvm::LoadInst >( curr ) ) {
            curr = l->getPointerOperand();
            Indices inds = { LoadStep{} };
            visited.push_back( { curr, l, inds } );
        } else if ( auto a = llvm::dyn_cast< llvm::AllocaInst >( curr ) ) {
            fields.create( a );
        } else {
            return;
        }
    }

     for ( auto & v : lart::util::reverse( visited ) )
        fields.insert( v );
}

template< typename Fields >
bool isBackEdge( const Path & path, Fields & fields ) {
    if ( !fields.has( path.from ) || !fields.has( path.to ) )
        return false;
    return fields.reachable( path.to, path.from );
}

void VPA::propagatePtrOrStructDown( const PropagateDown & t ) {
    auto val = t.value.value;

    auto filter = [] ( const AbstractValue & av ) {
        if ( Load( av ) )
            return false;
        if ( GEP( av ) )
            return false;
        if ( CallSite( av ) )
            return false;
        return isScalarType( av.value->getType() );
    };

    auto rf = reachFrom( t.value, filter );
    for ( auto & av : lart::util::reverse( rf ) ) {
        if ( auto a = Alloca( av ) ) {
            t.roots->insert( av );
            if ( !fields.has( a ) )
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
            fields.insert( path );

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
            auto val = s->getValueOperand();

            if ( auto c = llvm::dyn_cast< llvm::Constant >( val ) )
                if ( c->isNullValue() )
                    return; // storing of null is boring

            if ( !isScalarType( val ) ) {
                visitOrigins( val, fields );
                visitOrigins( ptr, fields );
            }

            if ( fields.has( path.from ) &&
                 fields.has( path.to ) &&
                 !fields.inSameTrie( path.from, path.to ) )
            {
                fields.storeUnder( path.to, path.from ); // We are storing to another trie
            } else {
                if ( isBackEdge( path, fields ) )
                    fields.insertBackEdge( path );
                else
                    fields.insert( path );
            }

            if ( !isScalarType( val ) ) {
                auto o = origin( ptr, fields );
                if ( auto a = llvm::dyn_cast< llvm::Argument >( o ) )
                    dispach( PropagateUp( a, t.roots, t.parent ) );
                dispach( PropagateDown( { o, Domain::Symbolic }, t.roots, t.parent ) );
            }
        }
        else if ( auto gep = GEP( av ) ) {
            auto path = createFieldPath( gep );
            fields.insert( path );

            if ( llvm::isa< llvm::GlobalValue >( gep->getPointerOperand() ) )
                t.roots->insert( av );
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
            fields.alias( mem->getOperand( 1 ), dest );
            dispach( PropagateDown( AbstractValue{ dest, av.domain }, t.roots, t.parent ) );
        }
        else if ( auto cs = CallSite( av ) ) {
            if ( cs.getInstruction() == val )
                t.roots->insert( av ); // we are propagating from this call
            else
                dispach( StepIn( make_parent( cs, t.parent, t.roots ) ) );
        }
        else if ( auto ret = Ret( av ) ) {
            if ( fields.has( ret->getReturnValue() ) )
                dispach( StepOut( getFunction( ret ), av, t.parent ) );
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
        else if ( auto s = Store( av ) ) {
            auto ptr = s->getPointerOperand();
            Path path = createFieldPath( s );
            fields.insert( path );
            if ( !fields.getDomain( path ) )
                fields.setDomain( path, dom );

            auto o = origin( ptr, fields );
            if ( auto a = llvm::dyn_cast< llvm::Argument >( o ) )
                dispach( PropagateUp( a, t.roots, t.parent ) );
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
            dispach( StepOut( getFunction( av.value ), av, t.parent ) );
        }
    }
}

void VPA::markGlobal( llvm::GlobalValue * value ) {
    Domain dom = Domain::Undefined;
    if ( auto maybedom = fields.getDomain( { value, { LoadStep{} } } ) )
        dom = maybedom.value();

    AbstractValue av = { value, dom };

    if ( globals.count( av ) )
        return;
    globals.insert( av );

    for ( const auto & u : value->users() ) {
        if ( auto l = llvm::dyn_cast< llvm::LoadInst >( u ) ) {
            auto fn = getFunction( l );
            if ( !reached.count( fn ) )
                record( fn );

            AbstractValue al = { l, dom };
            dispach( PropagateDown( al, reached[ fn ].annotations(), nullptr ) );
        }
        else if ( auto s = llvm::dyn_cast< llvm::StoreInst >( u ) ) {
            auto fn = getFunction( s );
            if ( !reached.count( fn ) )
                record( fn );
            auto o = origin( s->getValueOperand(), fields );
            AbstractValue ao = { o, dom };
            dispach( PropagateDown( ao, reached[ fn ].annotations(), nullptr ) );
        }
        else if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( u ) ) {
            auto fn = getFunction( gep );
            if ( !reached.count( fn ) )
                record( fn );

            auto path = createFieldPath( gep );
            if ( auto maybedom = fields.getDomain( path ) )
                dom = maybedom.value();

            AbstractValue ag = { gep, dom };
            dispach( PropagateDown( ag, reached[ fn ].annotations(), nullptr ) );
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
        auto o = origin( val, fields );
        if ( auto arg = Argument( t.value ) ) {
            if ( arg->getType()->isPointerTy() )
                dispach( PropagateUp( arg, t.roots, t.parent ) );
        } else if ( auto s = isArgStoredTo( o ) ) {
            auto arg = llvm::cast< llvm::Argument >( s->getValueOperand() );
            if ( arg->getType()->isPointerTy() ) {
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

    auto av =  AbstractValue{ t.arg, Domain::Symbolic };
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
        auto o = origin( op, fields );
        auto root = AbstractValue{ o, Domain::Symbolic };
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
