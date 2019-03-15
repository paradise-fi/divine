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
#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>

#include <lart/analysis/postorder.h>

#include <brick-llvm>

namespace lart {
namespace abstract {

using namespace llvm;

using lart::util::get_function;

namespace {

inline Argument* get_argument( Function *fn, unsigned idx ) {
    ASSERT_LT( idx, fn->arg_size() );
    return &*std::next( fn->arg_begin(), idx );
}

Values reach_from( Values roots, Domain dom ) {
    auto value_succs = [=] ( llvm::Value * val ) -> Values {
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            if ( !is_propagable_in_domain( inst, dom ) )
                return {};
        return Values{ val->user_begin(), val->user_end() };
    };
    return lart::analysis::postorder( roots, value_succs );
};

Values reach_from( Value *root, Domain dom ) { return reach_from( Values{ root }, dom ); }

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

template< typename Meta, typename Call >
bool has_called_function_meta( Call call, Meta meta ) {
    auto fns = get_potentialy_called_functions( call );
    return query::query( fns ).all( meta );
}

bool ignore_call_of_function( CallInst * call ) {
    return has_called_function_meta( call, meta::function::ignore_call );
}

bool ignore_return_of_function( CallInst * call ) {
    return has_called_function_meta( call, meta::function::ignore_return );
}

void stop_on_forbidden_propagation( Instruction * inst, Domain dom ) {
    std::string msg = "Propagating through unsupported operation in "
                      + dom.name() + " domain:\n";
    llvm::raw_string_ostream os( msg );
    inst->print( os, true );

    throw std::runtime_error( os.str() );
}

void VPA::propagate_value( Value *val, Domain dom ) {
    if ( seen_vals.count( { val, dom } ) )
        return;

    if ( isa< GlobalValue >( val ) ) {
        seen_vals.emplace( val, dom );
        for ( auto u : val->users() )
            tasks.push_back( [=]{ propagate_value( u, dom ); } );
        return;
    }

    auto deps = reach_from( val, dom );

    if ( auto call = dyn_cast< CallInst >( val ) ) {
        if ( ignore_return_of_function( call ) )
            deps.pop_back(); // root dependency (val) is last
    }

    for ( auto & dep : lart::util::reverse( deps ) ) {
        seen_vals.emplace( dep, dom );

        if ( auto call = dyn_cast< CallInst >( dep ) ) {
            if ( ignore_call_of_function( call ) )
                continue;
        }

        if ( auto inst = dyn_cast< Instruction >( dep ) ) {
            if ( is_propagable_in_domain( inst, dom ) ) {
                if ( forbidden_propagation_by_domain( inst, dom ) ) {
                    stop_on_forbidden_propagation( inst, dom );
                }
                Domain::set( inst, dom );
            }
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
        for ( auto src : AbstractionSources( ptr ).get() ) {
            tasks.push_back( [=]{ propagate_value( src, dom ); } );
        }
        if ( auto arg = dyn_cast< Argument >( ptr ) )
            propagate_back( arg, dom );
    } else {
        if ( auto arg = dyn_cast< Argument >( val ) )
            propagate_back( arg, dom );
    }
}

void VPA::propagate( CallInst *call, Domain dom ) {
    run_on_potentialy_called_functions( call, [&] ( auto fn ) {
        if ( meta::function::ignore_call( fn ) )
            return;

        if ( is_transformable_in_domain( call, dom ) )
            return;

        if ( meta::function::is_forbidden( fn ) ) {
            auto name = fn->hasName() ? fn->getName().str() : "anonymous";
            throw std::runtime_error( "transforming forbidden function: " + name );
        }

        if ( !fn->isIntrinsic() ) {
            preprocess( fn );
            for ( auto &op : call->arg_operands() ) {
                auto val = op.get();
                if ( seen_vals.count( { val, dom } ) ) {
                    unsigned idx = op.getOperandNo();
                    auto arg = get_argument( fn, idx );
                    tasks.push_back( [=]{ propagate_value( arg, dom ); } );
                    if ( is_base_type( fn->getParent(), arg ) )
                        Domain::set( arg, dom );
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
    for ( auto * val : meta::enumerate( m ) ) {
        preprocess( get_function( val ) );
        tasks.push_back( [=]{ propagate_value( val, Domain::get( val ) ); } );
    }

    auto process = [&] ( const auto &val, const Domain &dom ) {
        if ( auto call = dyn_cast< CallInst >( val ) ) {
            auto fn = get_function( call );
            preprocess( fn );
            tasks.push_back( [=]{ propagate_value( call, dom ); } );
        }
    };

    for ( auto &fn : m ) {
        if ( auto meta = meta::abstract::get( &fn ) ) {
            auto dom = Domain{ meta.value() };
            for ( auto u : fn.users() ) {
                if ( auto ce = dyn_cast< ConstantExpr >( u ) ) {
                    process( lower_constant_expr_call( ce ), dom );
                } else {
                    process( u, dom );
                }
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
