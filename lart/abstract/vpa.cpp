// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/vpa.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/lowerselect.h>
#include <lart/abstract/metadata.h>
#include <lart/abstract/util.h>

#include <lart/analysis/postorder.h>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

bool valid_root_metadata( const MDValue &mdv ) {
    auto ty = mdv.value()->getType();
    return ( ty->isIntegerTy() ) ||
           ( ty->isPointerTy() && ty->getPointerElementType()->isIntegerTy() );
}

void add_abstract_metadata( Instruction *i, Domain dom ) {
    auto& ctx = i->getContext();

    auto fn = get_function( i );
    fn->setMetadata( "lart.abstract.roots", MDTuple::get( ctx, {} ) );

    MDBuilder mdb( ctx );
    std::vector< Metadata* > doms;

    doms.emplace_back( mdb.domain_node( dom ) );
    // TODO extend domains if "lart.domains" metadata exists

    i->setMetadata( "lart.domains", MDTuple::get( ctx, doms ) );
}

Values value_succs( Value *v ) { return { v->user_begin(), v->user_end() }; }

Values reach_from( Values roots ) {
    return lart::analysis::postorder( roots, value_succs );
};

Values reach_from( Value *root ) { return reach_from( Values{ root } ); }

Value* get_source( Value *val ) {
    while ( true ) {
        if ( auto gep = dyn_cast< GetElementPtrInst >( val ) )
            val = gep->getPointerOperand();
        else if ( auto load = dyn_cast< LoadInst >( val ) )
            val = load->getPointerOperand();
        else if ( isa< AllocaInst >( val ) )
            return val;
        else if ( isa< GlobalValue >( val ) )
            return val;
        else if ( isa< Argument >( val ) )
            return val;
        else if ( isa< CallInst >( val ) )
            return val; // TODO if pointer do we need to propagate through return?
        else {
            val->dump();
            UNREACHABLE( "Unknown parent instruction." );
        }
    }
}

} // anonymous namespace

void VPA::preprocess( Function * fn ) {
    if ( !seen_funs.count( fn ) ) {
        auto lsi = std::unique_ptr< llvm::FunctionPass >( createLowerSwitchPass() );
        lsi.get()->runOnFunction( *fn );

        LowerSelect().runOnFunction( *fn );
        UnifyFunctionExitNodes().runOnFunction( *fn );

        seen_funs.insert( fn );
    }
}

void VPA::propagate_value( Value *val, Domain dom ) {
    if ( seen_vals.count( { val, dom } ) )
        return;

    if ( isa< GlobalValue >( val ) ) {
        for ( auto u : val->users() )
            tasks.push_back( [=]{ propagate_value( u, dom ); } );
        return;
    }

    auto deps = reach_from( val );
    for ( auto & dep : lart::util::reverse( deps ) ) {
        seen_vals.emplace( dep, dom );

        if ( auto i = dyn_cast< Instruction >( dep ) ) {
            add_abstract_metadata( i, dom );
        }

        if ( auto s = dyn_cast< StoreInst >( dep ) ) {
            if ( seen_vals.count( { s->getValueOperand(), dom } ) ) {
                auto src = get_source( s->getPointerOperand() );
                tasks.push_back( [=]{ propagate_value( src, dom ); } );
                if ( auto a = dyn_cast< Argument >( s->getPointerOperand() ) )
                    if ( !entry_args.count( { a, dom } ) )
                        propagate_back( a, dom );
            } else {
                if ( auto a = dyn_cast< Argument >( s->getValueOperand() ) )
                    if ( !entry_args.count( { a, dom } ) )
                        propagate_back( a, dom );
            }
        }
        else if ( auto call = dyn_cast< CallInst >( dep ) ) {
            auto fn = call->getCalledFunction();
            if ( !fn->isIntrinsic() ) {
                for ( auto &op : call->arg_operands() ) {
                    auto val = op.get();
                    if ( seen_vals.count( { val, dom } ) ) {
                        auto arg = std::next( fn->arg_begin(), op.getOperandNo() );
                        tasks.push_back( [=]{ propagate_value( arg, dom ); } );
                        entry_args.emplace( arg, dom );
                    }
                }
            }
        }
        else if ( auto r = dyn_cast< ReturnInst >( dep ) ) {
            step_out( get_function( r ), dom );
        }
        else if ( auto a = dyn_cast< Argument >( dep ) ) {
            if ( !entry_args.count( { a, dom } ) )
                propagate_back( a, dom );
        }
    }

    seen_vals.emplace( val, dom );
}

void VPA::propagate_back( Argument *arg, Domain dom ) {
    if ( !arg->getType()->isPointerTy() )
        return;
    for ( auto u : get_function( arg )->users() ) {
        if ( auto call = dyn_cast< CallInst >( u ) ) {
            auto src = get_source( call->getOperand( arg->getArgNo() ) );
            tasks.push_back( [=]{ propagate_value( src, dom ); } );
        }
    }
}

void VPA::step_out( Function *fn, Domain dom ) {
    for ( auto u : fn->users() )
        if ( auto call = dyn_cast< CallInst >( u ) ) {
            preprocess( get_function( call ) );
            tasks.push_back( [=]{ propagate_value( call, dom ); } );
        }
}

void VPA::run( Module &m ) {
    for ( const auto & mdv : abstract_metadata( m ) ) {
        if ( !valid_root_metadata( mdv ) )
            throw std::runtime_error( "Only annotation of integer values is allowed" );

        auto val = mdv.value();
        auto dom = mdv.domain();
        preprocess( get_function( val ) );
        tasks.push_back( [=]{ propagate_value( val, dom ); } );
    }

    for ( auto &fn : m ) {
        if ( auto md = fn.getMetadata( "lart.abstract.return" ) ) {
            auto &tup = cast< MDNode >( md )->getOperand( 0 );
            auto &mdn = cast< MDNode >( tup )->getOperand( 0 );
            auto dom_name = cast< MDString >( mdn )->getString().str();
            auto dom = DomainTable[ dom_name ];

            for ( auto u : fn.users() )
                if ( auto call = dyn_cast< CallInst >( u ) ) {
                    preprocess( get_function( call ) );
                    tasks.push_back( [=]{ propagate_value( call, dom ); } );
                }
        }
    }

    while ( !tasks.empty() ) {
        tasks.front()();
        tasks.pop_front();
    }
}

} // namespace abstract
} // namespace lart
