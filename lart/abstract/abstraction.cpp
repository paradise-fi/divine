// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/abstraction.h>
#include <lart/abstract/intrinsic.h>

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

}

AbstractValues Abstraction::FunctionNode::reachedValues() const {
    return reachFrom( { roots().begin(), roots().end() } );
}

void Abstraction::run( llvm::Module & m ) {
    // create function prototypes
    Map< FunctionNode, llvm::Function * > prototypes;

    std::vector< FunctionNode > sorted;
    for ( auto & fn : VPA().run( m ) )
        for ( const auto & rs : fn.second )
            sorted.emplace_back( fn.first, rs );

    std::sort( sorted.begin(), sorted.end(), [] ( FunctionNode l, FunctionNode r ) {
        auto arghash = [] ( const FunctionNode & fn ) -> size_t {
            size_t sum = 0;
            for ( auto & a : filterA< llvm::Argument >( fn.roots() ) )
                sum += a.get< llvm::Argument >()->getArgNo();
            return sum;
        };

        auto ln = l.first->getName().str();
        auto rn = r.first->getName().str();

        return ln == rn ? arghash( l ) < arghash( r ) : ln < rn;
    } );

    for ( auto & fnode : sorted )
        prototypes[ fnode ] = process( fnode );

    Functions remove;
    for ( const auto & p : prototypes ) {
        LiftMap< llvm::Value *, llvm::Value * > vmap;
        auto builder = make_builder( vmap, data.tmap, fns );
        auto fnode = p.first;

        // 1. if signature changes create a new function declaration
        // if proccessed function is called with abstract argument create clone of it
        // to preserve original function for potential call without abstract argument
        // TODO what about function with abstract argument and abstract annotation?
        bool called = query::query( fnode.roots() ).any( [] ( const auto & n ) {
            return n.template isa< llvm::Argument >();
        } );

        if ( called )
            fnode = clone( fnode );

        // 2. process function abstract entries
        auto postorder = fnode.reachedValues();
        for ( auto & av : lart::util::reverse( postorder ) )
            builder.process( av );

        // 3. clean function
        // FIXME let builder take annotation structure
        Values deps;
        for ( auto & av : postorder )
            if ( !av.isa< llvm::GetElementPtrInst >() )
                deps.emplace_back( av.value );
        clean( deps );

        // 4. copy function to declaration and handle function uses
        auto fn = fnode.function();
        auto & changed = p.second;
        if ( changed != fn ) {
            remove.push_back( fn );
            llvm::ValueToValueMapTy vtvmap;
            cloneFunctionInto( changed, fn, vtvmap );
            // Remove lifts of abstract arguments
            for ( auto & arg : changed->args() ) {
                if ( data.tmap.isAbstract( arg.getType() ) ) {
                    for ( auto & lift : lifts( arg.users() ) ) {
                        lift->replaceAllUsesWith( &arg );
                        lift->eraseFromParent();
                    }
                }
            }
            CallInterupt().run( changed );
        }
    }

    for ( auto & fn : lart::util::reverse( remove ) )
        fn->eraseFromParent();
}

llvm::Function * Abstraction::process( const FunctionNode & fnode ) {
    auto rets = filterA< llvm::ReturnInst >( fnode.reachedValues() );
    auto args = filterA< llvm::Argument >( fnode.reachedValues() );

    // Signature does not need to be changed
    if ( rets.empty() && args.empty() )
        return fnode.first;

    auto fn = fnode.first;
    auto dom = !rets.empty() ? rets[ 0 ].domain : Domain::LLVM;
    auto rty = liftType( fn->getReturnType(), dom, data.tmap );

    Map< unsigned, llvm::Type * > amap;
    for ( const auto & a : args )
        amap[ a.get< llvm::Argument >()->getArgNo() ] = a.type( data.tmap );

    auto as = remapFn( fnode.first->args(), [&] ( const auto & a ) {
        return amap.count( a.getArgNo() ) ? amap[ a.getArgNo() ] : a.getType();
    } );

    auto fty = llvm::FunctionType::get( rty, as, fn->getFunctionType()->isVarArg() );
    auto newfn = llvm::Function::Create( fty, fn->getLinkage(), fn->getName(), fn->getParent() );
    fns.insert( fn, newfn );
    return newfn;
}

Abstraction::FunctionNode Abstraction::clone( const FunctionNode & node ) {
    llvm::ValueToValueMapTy vmap;
    auto clone = CloneFunction( node.function(), vmap, true, nullptr );
    node.function()->getParent()->getFunctionList().push_back( clone );

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

void Abstraction::clean( Values & deps ) {
    auto is = llvmFilter< llvm::Instruction >( deps );
    for ( auto & inst : is ) {
        for ( auto & lift : liftsOf( inst ) )
            if ( std::find( is.begin(), is.end(), lift ) == is.end() )
                    lift->eraseFromParent();
        inst->removeFromParent();
    }
    for ( auto & inst : is )
        inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
    for ( auto & inst : is )
        delete inst;
}

} // abstract
} // lart

