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

using namespace detail;

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

template< typename Globals >
FNode init_globals( llvm::Module & m, Globals & gvars, Fields *fields ) {
    auto fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy( m.getContext() ), false );

    auto initFunction = llvm::cast< llvm::Function >(
        m.getOrInsertFunction( "__lart_globals_initialize", fty ) );

    initFunction->deleteBody();
    auto bb = llvm::BasicBlock::Create( m.getContext(), "entry", initFunction );

    FNode fn = { initFunction, {}, fields };
    auto irb = llvm::IRBuilder<>( bb );

    for ( auto & g : gvars ) {
        auto gvar = g.template get< llvm::GlobalVariable >();
        assert( gvar->getInitializer() );
        auto store = irb.CreateStore( gvar->getInitializer(), gvar );
        fn.roots().insert( { store, g.domain } );
    }

    fn.init();

    irb.CreateRetVoid();
    return fn;
}

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

template< typename Functions >
auto create_fnodes( Functions &fns, Fields *fields ) {
    std::vector< FNode > fnodes;
    for ( auto &fn : fns )
        for ( const auto &roots : fn.second )
            fnodes.emplace_back( fn.first, roots, fields );
    std::sort( fnodes.begin(), fnodes.end() );
    return fnodes;
}

} // anonymous namespace

void FNode::init() {
    auto filter = [&] ( const AbstractValue & av ) {
        if ( auto icmp = Cmp( av ) )
            return !isScalarType( icmp->getOperand( 0 )->getType() );
        if ( !isScalarOp( av ) )
            return false;
        if ( auto load = Load( av ) )
            return !_fields->has( load );
        if ( auto store = Store( av ) )
            return !_fields->has( store->getPointerOperand() );
        if ( auto gep = GEP( av ) )
            return !_fields->has( gep );
        return false;
    };

    AbstractValues avroots;
    for ( const auto & av : roots() ) {
        Domain dom = av.domain;
        if ( auto mdom = _fields->getDomain( av.value ) )
            dom = mdom.value();
        else if ( auto mdom = _fields->getDomain( { av.value, { LoadStep{} } } ) )
            dom = mdom.value();
        avroots.emplace_back( av.value, dom );
    }

    _ainsts = reachFrom( avroots, filter );
    _change_sig = _need_change_signature();
}

std::optional< AbstractValue > FNode::ret() const {
    auto rets = filterA< llvm::ReturnInst >( abstract_insts() );
    return rets.empty() ? std::nullopt : std::make_optional( rets[ 0 ] );
}

AbstractValues FNode::args() const {
    return filterA< llvm::Argument >( roots() );
}

bool FNode::_need_change_signature() const {
    auto frty = _fn->getReturnType();
    return ( ret() && isScalarType( frty ) ) || !args().empty();
}

Domain FNode::return_domain() const {
    auto ret_v = ret();
    return ret_v ? ret_v.value().domain : Domain::LLVM;
}

llvm::Type* FNode::return_type( TMap &tmap ) const {
    auto dom = return_domain();
    auto frty = _fn->getReturnType();
    return !isScalarType( frty ) ? frty : liftType( _fn->getReturnType(), dom, tmap );
}

llvm::FunctionType* FNode::function_type( TMap &tmap ) const {
    Map< unsigned, llvm::Type * > amap;
    for ( const auto & a : args() ) {
        auto ano = a.get< llvm::Argument >()->getArgNo();
        amap[ ano ] = stripPtrs( a.value->getType() )->isStructTy()
                    ? a.value->getType() : a.type( tmap );
    }

    auto as = remapFn( _fn->args(), [&] ( const auto & a ) {
        return amap.count( a.getArgNo() ) ? amap[ a.getArgNo() ] : a.getType();
    } );

    return llvm::FunctionType::get( return_type( tmap ), as, _fn->getFunctionType()->isVarArg() );
}

template< typename Builder >
void FNode::process( Builder & b ) {
    auto ai = abstract_insts();
    for ( auto & av : lart::util::reverse( ai ) ) {
        if ( isScalarOp( av ) )
            b.process( av );
        else
            b.processStructOp( av );
    }
}

void FNode::clean() {
    std::set< llvm::Value * > deps;
    for ( auto & av : abstract_insts() )
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
        _fields->erase( inst );
    for ( auto & inst : is )
        inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
    for ( auto & inst : is )
        delete inst;
}


void Abstraction::run( llvm::Module & m ) {
    VPA::Globals globals;
    Reached functions;
    Fields fields;

    // Value propagation analysis
    std::tie( functions, globals, fields ) = VPA().run( m );

    std::vector< std::pair< FNode, llvm::Function* > > prototypes;
    for ( auto n : create_fnodes( functions, &fields ) )
        prototypes.emplace_back( n, create_prototype( n ) );

    auto ig = init_globals( m, globals, &fields );
    prototypes.emplace_back( ig, create_prototype( ig ) );

    GlobMap globmap( data, m, globals );

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
        bool has_annotation_roots = fnode.function()->getMetadata( "lart.abstract.values" );
        if ( fnode.need_change_signature() && has_annotation_roots )
            remove.insert( fnode.function() );

        fnode = !fnode.args().empty() ? clone( fnode ) : fnode;

        fnode.process( builder );
        fnode.clean();

        // Copy function to declaration and handle function uses
        if ( fnode.need_change_signature() ) {
            auto &changed = p.second;

            remove.insert( fnode.function() );

            llvm::ValueToValueMapTy vtvmap;
            cloneFunctionInto( changed, fnode.function(), vtvmap );
            removeArgumentLifts( changed, data );

            // force edge interrupt in recursive calls
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


llvm::Function * Abstraction::create_prototype( const FNode & fnode ) {
    auto fn = fnode.function();
    if ( !fnode.need_change_signature() )
        return fn;

    auto fty = fnode.function_type( data.tmap );
    auto newfn = llvm::Function::Create( fty, fn->getLinkage(), fn->getName(), fn->getParent() );
    fns.insert( fn, argIndices( fnode.args() ), newfn );
    return newfn;
}

FNode Abstraction::clone( const FNode & node ) {
    llvm::ValueToValueMapTy vmap;
    auto clone = CloneFunction( node.function(), vmap, true, nullptr );
    node.function()->getParent()->getFunctionList().push_back( clone );

    for ( const auto & v : vmap ) {
        if ( node.fields()->has( v.first ) )
            node.fields()->alias( v.first, v.second );
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
    return { clone, roots, node.fields() };
}

} // abstract
} // lart

