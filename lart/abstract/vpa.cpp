// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/vpa.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/lowerselect.h>
#include <lart/abstract/metadata.h>
#include <lart/abstract/util.h>

#include <lart/analysis/postorder.h>

#include <brick-llvm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

inline Argument* get_argument( Function *fn, unsigned idx ) {
    ASSERT_LT( idx, fn->arg_size() );
    return &*std::next( fn->arg_begin(), idx );
}

Values value_succs( Value *v ) { return { v->user_begin(), v->user_end() }; }

Values reach_from( Values roots ) {
    return lart::analysis::postorder( roots, value_succs );
};

Values reach_from( Value *root ) { return reach_from( Values{ root } ); }

struct AbstractionSources {

    explicit AbstractionSources( Value *from ) : from( from ) {}

    Values get() {
        auto values = get_impl( from );

        Values res;
        std::unique_copy( values.begin(), values.end(), std::back_inserter( res ) );
        return res;
    }

private:
    Values get_impl( Value *val ) {
        Values res;

        llvmcase( val,
            [&] ( GetElementPtrInst *inst ) { res = get_impl( inst->getPointerOperand() ); },
            [&] ( LoadInst *inst )          { res = get_impl( inst->getPointerOperand() ); },
            [&] ( BitCastInst *inst )       { res = get_impl( inst->getOperand( 0 ) ); },
            [&] ( ConstantExpr *inst )      { res = get_impl( inst->getOperand( 0 ) ); },
            [&] ( IntToPtrInst *inst )      { res = get_impl( inst->getOperand( 0 ) ); },
            [&] ( AllocaInst * )            { res = { val }; },
            [&] ( Argument * )              { res = { val }; },
            [&] ( CallInst * )              { res = { val }; },
            [&] ( GlobalValue * )           { res = { val }; },
            [&] ( ExtractValueInst * )      { res = { val }; },
            [&] ( Constant * ) {
                res = {};
                ASSERT( !isa< ConstantExpr >( val ) );
            },
            [&] ( PHINode *inst ) {
                if ( !seen_phi_nodes.count( inst ) ) {
                    seen_phi_nodes.insert( inst );
                    for ( auto &in : inst->incoming_values() ) {
                        auto values = get_impl( in.get() );
                        std::move( values.begin(), values.end(), std::back_inserter( res ) );
                    }
                }
            },
            [&] ( Value * ) {
                UNREACHABLE( "Unknown parent instruction:", val );
            }
        );

        return res;
    }

    Value * from;
    std::set< Value * > seen_phi_nodes;
};

bool is_indirect_call( CallInst * call ) {
    return call->getCalledFunction() == nullptr;
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

        auto ignore_function = [] ( Function * fn ) {
            return fn->getMetadata( FunctionTag::ignore );
        };
        if ( auto call = dyn_cast< CallInst >( dep ) ) {
            auto fns = get_potentialy_called_functions( call );
            if ( query::query( fns ).any( ignore_function ) )
                continue;
        }

        if ( auto i = dyn_cast< Instruction >( dep ) ) {
            add_abstract_metadata( i, dom );
        }

        llvmcase( dep,
            [&] ( StoreInst *store ) { propagate( store, dom ); },
            [&] ( CallInst *call )   { propagate( call, dom ); },
            [&] ( ReturnInst *ret )  { propagate( ret, dom ); },
            [&] ( Argument *arg )    { propagate_back( arg, dom ); }
        );
    }

    seen_vals.emplace( val, dom );
}

void VPA::propagate( StoreInst *store, Domain dom ) {
    auto ptr = store->getPointerOperand();
    auto val = store->getValueOperand();

    if ( seen_vals.count( { val, dom } ) ) {
        preprocess( store->getParent()->getParent() );
        for ( auto src : AbstractionSources( ptr ).get() )
            tasks.push_back( [=]{ propagate_value( src, dom ); } );
        if ( auto arg = dyn_cast< Argument >( ptr ) )
            propagate_back( arg, dom );
    } else {
        if ( auto arg = dyn_cast< Argument >( val ) )
            propagate_back( arg, dom );
    }
}

void VPA::propagate( CallInst *call, Domain dom ) {
    run_on_potentialy_called_functions( call, [&] ( auto fn ) {
        FunctionMetadata fmd{ fn };

        if ( !fn->isIntrinsic() ) {
            preprocess( fn );
            for ( auto &op : call->arg_operands() ) {
                auto val = op.get();
                if ( seen_vals.count( { val, dom } ) ) {
                    unsigned idx = op.getOperandNo();
                    auto arg = get_argument( fn, idx );
                    tasks.push_back( [=]{ propagate_value( arg, dom ); } );
                    if ( is_base_type( arg->getType() ) ) // TODO domain basetype
                        fmd.set_arg_domain( idx, dom );
                    entry_args.emplace( arg, dom );
                }
            }
        }
        else if ( auto mem = dyn_cast< MemTransferInst >( call ) ) {
            if ( seen_vals.count( { mem->getSource(), dom } ) ) {
                ASSERT( seen_funs.count( call->getParent()->getParent() ) );
                for ( auto src : AbstractionSources( mem->getDest() ).get() )
                    tasks.push_back( [=]{ propagate_value( src, dom ); } );
            }
        }
    } );
}

void VPA::propagate( ReturnInst *ret, Domain dom ) {
    step_out( get_function( ret ), dom );
}

void VPA::propagate_back( Argument *arg, Domain dom ) {
    if ( entry_args.count( { arg, dom } ) )
        return;
    if ( !arg->getType()->isPointerTy() )
        return;

    preprocess( get_function( arg ) );

    for ( auto u : get_function( arg )->users() ) {
        if ( auto call = dyn_cast< CallInst >( u ) ) {
            auto op = call->getOperand( arg->getArgNo() );
            ASSERT( seen_funs.count( get_function( arg ) ) );
            for ( auto src : AbstractionSources( op ).get() ) {
                tasks.push_back( [=]{ propagate_value( src, dom ); } );
            }
        }
    }
}

void VPA::step_out( Function *fn, Domain dom ) {
    auto process_call = [&] ( auto call ) {
        preprocess( get_function( call ) );
        tasks.push_back( [=]{ propagate_value( call, dom ); } );
    };

    for ( auto u : fn->users() ) {
        if ( isa< CallInst >( u ) ) {
            process_call( u );
        } else if ( isa< ConstantExpr >( u ) ) {
            for ( auto ceu : u->users() )
                if ( isa< CallInst >( ceu ) )
                    process_call( ceu );
        }
    }
}

void VPA::run( Module &m ) {
    for ( const auto & mdv : abstract_metadata( m ) ) {
        auto val = mdv.value();
        auto dom = mdv.domain();
        preprocess( get_function( val ) );
        tasks.push_back( [=]{ propagate_value( val, dom ); } );
    }

    for ( auto &fn : m ) {
        if ( auto md = fn.getMetadata( abstract_tag ) ) {
            // TODO use domain interface
            auto &tup = cast< MDNode >( md )->getOperand( 0 );
            auto &mdn = cast< MDNode >( tup )->getOperand( 0 );
            auto dom = Domain( cast< MDString >( mdn )->getString().str() );

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
