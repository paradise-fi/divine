// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>


DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/Transforms/IPO.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/abstraction.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/metadata.h>

namespace lart {
namespace abstract {

namespace {

template< typename Vs >
inline std::vector< llvm::CallInst * > lifts( const Vs & vs ) {
    return query::query( llvmFilter< llvm::CallInst >( vs ) )
    .filter( [&]( llvm::CallInst * c ) { return isLift( c ); } ).freeze();
}

struct CallInterupt {
    void run( llvm::Function * fn ) {
        auto vmInterrupt = interupt( fn->getParent() );
        auto inPt = fn->getEntryBlock().getFirstInsertionPt();
        llvm::IRBuilder<>( inPt ).CreateCall( vmInterrupt, { } );
    }

    llvm::Function * interupt( llvm::Module * m ) {
        auto fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy( m->getContext() ), false );
        auto vmInterrupt = llvm::cast< llvm::Function >(
            m->getOrInsertFunction( "__vmutil_interrupt", fty ) );
        ASSERT( vmInterrupt );
        vmInterrupt->addFnAttr( llvm::Attribute::NoUnwind );
        return vmInterrupt;
    }
};

bool isScalarOp( const AbstractValue & av ) {
    auto type = av.value->getType();
    if ( Branch( av ) )
        return true;
    else if ( auto cmp = Cmp( av ) )
        return !( cmp->getOperand( 0 )->getType()->isPointerTy() ||
                  cmp->getOperand( 1 )->getType()->isPointerTy() );
    else if ( auto s = Store( av ) )
        return isScalarType( s->getValueOperand()->getType() );
    else if ( auto r = Ret( av ) )
        return isScalarType( r->getReturnValue()->getType() );
    else if ( ( Argument( av ) || Alloca( av ) || GEP( av ) || CallSite( av ) )
        && type->isPointerTy() && isScalarType( type->getPointerElementType() ) )
        return true;
    else if ( auto bc = BitCast( av ) )
        return type->isPointerTy() &&
               isScalarType( bc->getSrcTy()->getPointerElementType() );
    else if ( isScalarType( av.value ) )
        return true;
    return false;
}

struct InitGlobals {
    using Globals = VPA::Globals;
    using FNode = Abstraction::FunctionNode;

    FNode run( llvm::Module & m, Globals & gvars ) {
        auto fty = llvm::FunctionType::get(
            llvm::Type::getVoidTy( m.getContext() ), false );

        auto initFunction = llvm::cast< llvm::Function >(
            m.getOrInsertFunction( "__lart_globals_initialize", fty ) );

        initFunction->deleteBody();
        auto bb = llvm::BasicBlock::Create( m.getContext(), "entry", initFunction );

        initializer = { initFunction, {} };
        auto irb = llvm::IRBuilder<>( bb );

        for ( auto & g : gvars )
            init( g, irb );

        irb.CreateRetVoid();
        return initializer;
    }

    void init( const AbstractValue & av, llvm::IRBuilder<> & irb ) {
        auto gvar = av.get< llvm::GlobalVariable >();
        assert( gvar->getInitializer() );
        auto store = irb.CreateStore( gvar->getInitializer(), gvar );
        initializer.roots().insert( { store, av.domain } );
    }

private:
    FNode initializer;
};

struct GlobMap : LiftMap< llvm::Value *, llvm::Value * > {
    GlobMap( PassData & data, llvm::Module & m, VPA::Globals & globals )
        : data( data ), m( m )
    {
        for ( auto & av : globals ) {
            if ( !av.value->getType()->getPointerElementType()->isStructTy() ) {
                auto glob = llvm::cast< llvm::GlobalVariable >( av.value );
                auto dom = av.domain;

                auto type = liftType( glob->getValueType(), dom, data.tmap );
                auto name = glob->getName() + "." + DomainTable[ dom ];
                auto ag = llvm::cast< llvm::GlobalVariable >(
                                      m.getOrInsertGlobal( name.str(), type ) );

                this->insert( glob, ag );
            }
        }
    }

private:
    PassData & data;
    llvm::Module & m;
};

using FNode = Abstraction::FunctionNode;

void sortFunctionNodes( std::vector< FNode > & fnodes ) {
    std::sort( fnodes.begin(), fnodes.end(), [] ( FNode l, FNode r ) {
        auto arghash = [] ( const FNode & fn ) -> size_t {
            size_t sum = 0;
            for ( auto & a : filterA< llvm::Argument >( fn.roots() ) )
                sum += a.get< llvm::Argument >()->getArgNo();
            return sum;
        };

        auto ln = l.first->getName().str();
        auto rn = r.first->getName().str();

        return ln == rn ? arghash( l ) < arghash( r ) : ln < rn;
    } );
}

template< typename Abstracted, typename Fields >
void cleanFNode( Abstracted & abstracted, Fields & fields ) {
    std::set< llvm::Value * > deps;
    for ( auto & av : abstracted )
        if ( isScalarOp( av ) && !av.template isa< llvm::GetElementPtrInst >() ) {
            deps.insert( av.value );
        } else if ( auto cs = llvm::CallSite( av.value ) ) {
            auto fn = cs.getCalledFunction();
            if ( MemIntrinsic( av ) )
                continue;
            else if ( fn->isIntrinsic() ) // LLVM intrinsic
                deps.insert( av.value );
            else if ( !isIntrinsic( fn ) ) // Not a LART intrinsic
                deps.insert( av.value );
        }

    auto is = llvmFilter< llvm::Instruction >( deps );
    for ( auto & inst : is ) {
        for ( auto & lift : liftsOf( inst ) )
            if ( std::find( is.begin(), is.end(), lift ) == is.end() )
                    lift->eraseFromParent();
        inst->removeFromParent();
    }
    for ( auto & inst : is )
        fields.erase( inst );
    for ( auto & inst : is )
        inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
    for ( auto & inst : is )
        delete inst;
}

template< typename Builder, typename Fields >
void processFNode( FNode & fnode, Fields & fields, Builder & builder ) {
    auto postorder = fnode.reached( fields );
    for ( auto & av : lart::util::reverse( postorder ) ) {
        if ( isScalarOp( av ) )
            builder.process( av );
        else
            builder.processStructOp( av );
    }

    cleanFNode( postorder, fields );
}

void removeArgumentLifts( llvm::Function * fn, PassData & data ) {
    for ( auto & arg : fn->args() ) {
        if ( data.tmap.isAbstract( arg.getType() ) ) {
            for ( auto & lift : lifts( arg.users() ) ) {
                lift->replaceAllUsesWith( &arg );
                lift->eraseFromParent();
            }
        }
    }
}

} // anonymous namespace

AbstractValues Abstraction::FunctionNode::reached( const Fields & fields ) const {
    auto filter = [&] ( const AbstractValue & av ) {
        if ( auto icmp = Cmp( av ) )
            return !isScalarType( icmp->getOperand( 0 )->getType() );
        if ( !isScalarOp( av ) )
            return false;
        if ( auto load = Load( av ) )
            return !fields.has( load );
        if ( auto store = Store( av ) )
            return !fields.has( store->getPointerOperand() );
        if ( auto gep = GEP( av ) )
            return !fields.has( gep );
        return false;
    };

    AbstractValues avroots;
    for ( const auto & av : roots() ) {
        Domain dom = av.domain;
        if ( auto maybedom = fields.getDomain( av.value ) )
            dom = maybedom.value();
        if ( auto maybedom = fields.getDomain( { av.value, { LoadStep{} } } ) )
            dom = maybedom.value();
        avroots.emplace_back( av.value, dom );
    }

    return reachFrom( avroots, filter );
}

void Abstraction::run( llvm::Module & m ) {
    // create function prototypes
    Map< FNode, llvm::Function * > prototypes;
    std::vector< FNode > fnodes;

    VPA::Globals globals;
    Reached functions;
    Fields fields;

    // Value propagation analysis
    std::tie( functions, globals, fields ) = VPA().run( m );

    fnodes.emplace_back( InitGlobals().run( m, globals ) );
    for ( auto & fn : functions )
        for ( const auto & rs : fn.second )
            fnodes.emplace_back( fn.first, rs );
    sortFunctionNodes( fnodes );

    GlobMap globmap( data, m, globals );

    for ( auto & fnode : fnodes )
        prototypes[ fnode ] = process( fnode, fields );

    std::set< llvm::Function * > remove;
    for ( const auto & p : prototypes ) {
        auto fnode = p.first;
        if ( fnode.roots().empty() )
            continue;

        auto vmap = globmap;
        auto builder = make_builder( vmap, data.tmap, fns, fields );

        // If signature changes create a new function declaration
        // if proccessed function is called with abstract argument create clone of it
        // to preserve original function for potential call without abstract argument
        bool called = query::query( fnode.roots() ).any( [] ( const auto & n ) {
            return n.template isa< llvm::Argument >();
        } );
        if ( called ) fnode = clone( fnode, fields );

        // Create abstract intrinsics in the function node
        processFNode( fnode, fields, builder );

        // Copy function to declaration and handle function uses
        auto fn = fnode.function();
        auto & changed = p.second;
        if ( changed != fn ) {
            remove.insert( fn );
            llvm::ValueToValueMapTy vtvmap;
            cloneFunctionInto( changed, fn, vtvmap );
            removeArgumentLifts( changed, data );
            CallInterupt().run( changed );
        }
    }
    for ( auto & fn : lart::util::reverse( remove ) )
        fn->eraseFromParent();

    for ( const auto & g : globals )
        if ( !g.value->getType()->getPointerElementType()->isStructTy() )
            llvm::cast< llvm::GlobalVariable >( g.value )->eraseFromParent();

    auto sdp = llvm::createStripDeadPrototypesPass();
    sdp->runOnModule( m );
}

llvm::Function * Abstraction::process( const FunctionNode & fnode, const Fields & fields ) {
    auto rets = filterA< llvm::ReturnInst >( fnode.reached( fields ) );
    auto args = filterA< llvm::Argument >( fnode.roots() );

    auto fn = fnode.first;
    auto frty = fn->getReturnType();
    // Signature does not need to be changed
    if ( ( rets.empty() || !isScalarType( frty ) ) && args.empty() )
        return fnode.first;

    auto dom = !rets.empty() ? rets[ 0 ].domain : Domain::LLVM;
    auto rty = !isScalarType( frty ) ? frty : liftType( fn->getReturnType(), dom, data.tmap );

    Map< unsigned, llvm::Type * > amap;
    for ( const auto & a : args ) {
        auto ano = a.get< llvm::Argument >()->getArgNo();
        amap[ ano ] = stripPtrs( a.value->getType() )->isStructTy()
                    ? a.value->getType() : a.type( data.tmap );
    }

    auto as = remapFn( fnode.first->args(), [&] ( const auto & a ) {
        return amap.count( a.getArgNo() ) ? amap[ a.getArgNo() ] : a.getType();
    } );

    auto fty = llvm::FunctionType::get( rty, as, fn->getFunctionType()->isVarArg() );
    auto newfn = llvm::Function::Create( fty, fn->getLinkage(), fn->getName(), fn->getParent() );

    fns.insert( fn, argIndices( args ), newfn );
    return newfn;
}

Abstraction::FunctionNode Abstraction::clone( const FunctionNode & node, Fields & fields ) {
    llvm::ValueToValueMapTy vmap;
    auto clone = CloneFunction( node.function(), vmap, true, nullptr );
    node.function()->getParent()->getFunctionList().push_back( clone );

    for ( const auto & v : vmap ) {
        if ( fields.has( v.first ) )
            fields.alias( v.first, v.second );
    }

    std::set< AbstractValue > roots;
    for ( const auto & origin : node.roots() )
        if ( const auto & arg = origin.safeGet< llvm::Argument >() )
            roots.emplace( argAt( clone, arg->getArgNo() ), origin.domain );

    auto a = query::query( *node.function() ).flatten();
    auto b = query::query( *clone ).flatten();
    auto aIt = a.begin();
    auto bIt = b.begin();

    for ( ; aIt != a.end() && bIt != b.end(); ++aIt, ++bIt ) {
        auto root = std::find_if( node.roots().begin(), node.roots().end(),
        [&] ( const auto & r ) { return r.value == &(*aIt); } );
        if ( root != node.roots().end() )
            roots.emplace( &(*bIt), root->domain );
    }

    if ( fns.count( node.function() ) )
        fns.assign( clone, node.function() );
    return Abstraction::FunctionNode( clone, roots );
}

} // abstract
} // lart

