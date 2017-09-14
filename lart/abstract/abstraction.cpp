// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/abstraction.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/intrinsic.h>
#include <lart/support/lowerselect.h>

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

void Abstraction::run( llvm::Module & m ) {
    auto preprocess = [] ( llvm::Function * fn ) {
        auto lowerSwitchInsts = std::unique_ptr< llvm::FunctionPass >(
                                llvm::createLowerSwitchPass() );
        lowerSwitchInsts.get()->runOnFunction( *fn );

        // FIXME lower only abstract selects
        LowerSelect lowerSelectInsts;
        lowerSelectInsts.runOnFunction( *fn );

        llvm::UnifyFunctionExitNodes ufen;
        ufen.runOnFunction( *fn );
    };

    Functions remove;
    auto functions = Walker( m, preprocess ).postorder();

    // create function prototypes
    Map< FunctionNodePtr, llvm::Function * > prototypes;

    for ( const auto & node : functions )
        prototypes[ node ] = process( node );
    for ( const auto & node : functions ) {
        LiftMap< llvm::Value *, llvm::Value * > vmap;
        auto builder = make_builder( vmap, data.tmap, fns );

        // 1. if signature changes create a new function declaration
        // if proccessed function is called with abstract argument create clone of it
        // to preserve original function for potential call without abstract argument
        // TODO what about function with abstract argument and abstract annotation?
        bool called = query::query( node->origins ).any( [] ( const auto & n ) {
            return n.template isa< llvm::Argument >();
        } );

        if ( called ) clone( node );

        // 2. process function abstract entries
        auto postorder = node->postorder();
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
        auto fn = node->function;
        auto & changed = prototypes[ node ];
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

llvm::Function * Abstraction::process( const FunctionNodePtr & node ) {
    auto rets = filterA< llvm::ReturnInst >( node->postorder() );
    auto args = filterA< llvm::Argument >( node->postorder() );

    // Signature does not need to be changed
    if ( rets.empty() && args.empty() )
        return node->function;

    auto fn = node->function;
    auto dom = !rets.empty() ? rets[ 0 ].domain : Domain::LLVM;
    auto rty = liftType( fn->getReturnType(), dom, data.tmap );

    Map< unsigned, llvm::Type * > amap;
    for ( const auto & a : args )
        amap[ a.get< llvm::Argument >()->getArgNo() ] = a.type( data.tmap );

    auto as = remapFn( node->function->args(), [&] ( const auto & a ) {
        return amap.count( a.getArgNo() ) ? amap[ a.getArgNo() ] : a.getType();
    } );

    auto fty = llvm::FunctionType::get( rty, as, fn->getFunctionType()->isVarArg() );
    auto newfn = llvm::Function::Create( fty, fn->getLinkage(), fn->getName(), fn->getParent() );
    fns.insert( fn, newfn );
    return newfn;
}

void Abstraction::clone( const FunctionNodePtr & node ) {
    llvm::ValueToValueMapTy vmap;
    auto clone = CloneFunction( node->function, vmap, true, nullptr );
    node->function->getParent()->getFunctionList().push_back( clone );

    Set< AbstractValue > origins;
    for ( const auto & origin : node->origins )
        if ( const auto & arg = origin.safeGet< llvm::Argument >() )
            origins.emplace( argAt( clone, arg->getArgNo() ), origin.domain );

    auto a = query::query( *node->function ).flatten();
    auto b = query::query( *clone ).flatten();
    auto aIt = a.begin();
    auto bIt = b.begin();

    for ( ; aIt != a.end() && bIt != b.end(); ++aIt, ++bIt ) {
        auto o = node->isOrigin( &( *aIt ) );
        if ( o.isJust() )
            origins.emplace( &(*bIt), o.value().domain );
    }

    if ( fns.count( node->function ) )
        fns.assign( clone, node->function );

    node->function = clone;
    node->origins = origins;
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

