// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/vpa.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Transforms/Utils.h>
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

bool is_accessing_concrete_aggr_element( llvm::Value * val ) {
    if ( !llvm::isa< llvm::GetElementPtrInst >( val ) )
        return false;

    auto gep = llvm::cast< llvm::GetElementPtrInst >( val );
    if ( gep->getNumIndices() != 2 )
        return false;

    auto ptr = gep->getPointerOperand()->stripPointerCasts();
    if ( !meta::aggregate::has( ptr ) )
        return false;

    auto idx = std::next( gep->idx_begin() );
    if ( auto ci = llvm::dyn_cast< llvm::ConstantInt >( idx ) ) {
        auto indices = meta::aggregate::indices( ptr );
        return !std::count( indices.begin(), indices.end(), ci->getZExtValue() );
    }

    return false;
}

Values reach_from( Values roots, Domain dom ) {
    auto value_succs = [=] ( llvm::Value * val ) -> Values {
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
            if ( !is_propagable_in_domain( inst, dom ) )
                return {};

        if ( is_accessing_concrete_aggr_element( val ) ) {
            return {};
        }

        auto succs = query::query( val->users() )
            .filter( query::negate( is_accessing_concrete_aggr_element ) )
            .map( query::llvmdyncast< llvm::Value > )
            .freeze();

        return succs;
    };
    return lart::analysis::postorder( roots, value_succs );
};

Values reach_from( Value *root, Domain dom ) { return reach_from( Values{ root }, dom ); }

struct AbstractionSources
{

    explicit AbstractionSources( Value *from ) : from( from ) {}

    Values get()
    {
        auto values = get_impl( from, std::nullopt );

        std::set< llvm::Value * > sources;
        for ( const auto & [val, idx] : values ) {
            if ( idx.has_value() ) {
                if ( auto inst = llvm::dyn_cast< llvm::Instruction >( val ) )
                    meta::aggregate::set( inst, idx.value() );
                if ( auto glob = llvm::dyn_cast< llvm::GlobalVariable >( val ) )
                    meta::aggregate::set( glob, idx.value() );
                // TODO llvm::Argument
            }
            sources.insert( val );
        }

        return { sources.begin(), sources.end() };
    }

private:
    using Source = std::pair< llvm::Value *, std::optional< size_t > >;

    auto get_impl( Value *val, std::optional< size_t > idx ) -> std::vector< Source >
    {
        std::vector< Source > sources;

        llvmcase( val,
            [&] ( GetElementPtrInst *inst ) {
                idx = std::nullopt;

                if ( inst->getNumIndices(), 2 ) {
                    auto i = std::next( inst->idx_begin() );
                    if ( auto ci = llvm::dyn_cast< llvm::ConstantInt >( i ) )
                        idx = ci->getZExtValue();
                }

                sources = get_impl( inst->getPointerOperand(), idx );
            },
            [&] ( LoadInst *inst ) {
                sources = get_impl( inst->getPointerOperand(), std::nullopt );
            },
            [&] ( BitCastInst *inst ) {
                sources = get_impl( inst->getOperand( 0 ), idx );
            },
            [&] ( ConstantExpr *inst ) {
                sources = get_impl( inst->getOperand( 0 ), std::nullopt );
            },
            [&] ( IntToPtrInst *inst ) {
                sources = get_impl( inst->getOperand( 0 ), idx );
            },
            [&] ( PtrToIntInst *inst ) {
                sources = get_impl( inst->getOperand( 0 ), idx );
            },
            [&] ( BinaryOperator *inst ) {
                sources = get_impl( inst->getOperand( 0 ), idx );
                auto values = get_impl( inst->getOperand( 1 ), idx );
                std::move( values.begin(), values.end(), std::back_inserter( sources ) );
            },
            [&] ( SelectInst *inst ) {
                for ( auto i : { 1, 2 } )
                {
                    auto v = get_impl( inst->getOperand( i ), idx );
                    std::move( v.begin(), v.end(), std::back_inserter( sources ) );
                }
            },
            [&] ( AllocaInst * ) {
                sources = { { val, idx } };
            },
            [&] ( Argument * ) {
                sources = { { val, idx } };
            },
            [&] ( CallInst * ) {
                sources = { { val, idx } };
            },
            [&] ( GlobalVariable * ) {
                sources = { { val, idx } };
            },
            [&] ( ExtractValueInst * ) {
                sources = { { val, idx } };
            },
            [&] ( Constant * ) {
                sources = {};
                ASSERT( !isa< ConstantExpr >( val ) );
            },
            [&] ( PHINode *inst ) {
                if ( auto [val, inserted] = seen_phi_nodes.insert( inst ); inserted ) {
                    for ( auto &in : inst->incoming_values() ) {
                        auto values = get_impl( in.get(), idx );
                        std::move( values.begin(), values.end(), std::back_inserter( sources ) );
                    }
                }
            },
            [&] ( Value * ) {
                UNREACHABLE( "Unknown parent instruction" );
            }
        );

        return sources;
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
    auto fns = get_potentially_called_functions( call );
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

    if ( isa< GlobalVariable >( val ) ) {
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
            if ( !is_transformable_in_domain( call, dom ) && ignore_call_of_function( call ) )
                continue;
        }

        if ( auto inst = dyn_cast< Instruction >( dep ) ) {
            if ( is_propagable_in_domain( inst, dom ) ) {
                if ( forbidden_propagation_by_domain( inst, dom ) ) {
                    stop_on_forbidden_propagation( inst, dom );
                }
                Domain::set( inst, dom );
            }
            else if ( is_transformable_in_domain( inst, dom ) ) {
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

struct ArgumentsAnnotation {

    ArgumentsAnnotation( llvm::Function * fn ) : _fn( fn ) {}

    void run() {
        std::vector< llvm::Value * > stack = { _fn->user_begin(), _fn->user_end() };

        while ( !stack.empty() ) {
            auto val = stack.back();
            stack.pop_back();

            llvmcase( val,
                [&] ( llvm::CallInst * call ) {
                    meta::set( call, meta::tag::abstract_arguments );
                },
                [&] ( llvm::BitCastInst * btcst ) {
                    for ( auto u : btcst->users() ) {
                        stack.push_back( u );
                    }
                },
                [&] ( llvm::StoreInst * ) {
                    UNREACHABLE( "Unknown arguments propagation rule" );
                },
                [&] ( llvm::LoadInst * ) {
                    UNREACHABLE( "Unknown arguments propagation rule" );
                },
                [&] ( llvm::ExtractValueInst * ) {
                    UNREACHABLE( "Unknown arguments propagation rule" );
                },
                [&] ( llvm::InsertValueInst * ) {
                    UNREACHABLE( "Unknown arguments propagation rule" );
                },
                [&] ( llvm::PHINode * phi ) {
                    for ( auto u : phi->users() ) {
                        stack.push_back( u );
                    }
                },
                [&] ( llvm::CallInst * ) {
                    UNREACHABLE( "Unknown arguments propagation rule" );
                }
            );
        }

        meta::set( _fn, meta::tag::abstract_arguments );
    }

    llvm::Function * _fn;
};

void VPA::propagate( CallInst *call, Domain dom ) {
    run_on_potentially_called_functions( call, [&] ( auto fn ) {
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
                    if ( is_base_type( fn->getParent(), arg ) ) {
                        Domain::set( arg, dom );
                        ArgumentsAnnotation( fn ).run();
                    }
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

void VPA::propagate( llvm::ReturnInst *ret, Domain dom ) {
    auto fn = ret->getFunction();
    auto m = fn->getParent();
    auto val = ret->getReturnValue();
    if ( is_base_type_in_domain( m, val, dom ) )
        meta::set( fn, meta::tag::abstract_return );
    step_out( fn, dom, ret );
}

void VPA::propagate_back( Argument *arg, Domain dom )
{
    if ( entry_args.count( { arg, dom } ) )
        return;
    if ( !arg->getType()->isPointerTy() )
        return;

    preprocess( get_function( arg ) );

    auto process_users = [&]( llvm::Value *val, auto recurse ) -> void
    {
        if ( auto go = llvm::dyn_cast< llvm::GlobalObject >( val ) )
        {
            for ( auto &a : go->getParent()->aliases() )
                if ( a.getBaseObject() == go )
                    recurse( &a, recurse );
        }

        for ( auto u : val->users() )
            if ( auto call = dyn_cast< CallInst >( u ) )
            {
                auto op = call->getOperand( arg->getArgNo() );
                ASSERT( seen_funs.count( get_function( arg ) ) );
                for ( auto src : AbstractionSources( op ).get() ) {
                    tasks.push_back( [=]{ propagate_value( src, dom ); } );
                }
            }
    };

    process_users( get_function( arg ), process_users );
}

void VPA::step_out( llvm::Function * fn, Domain dom, llvm::ReturnInst * ret )
{
    auto val = ret->getReturnValue();

    auto process_call = [&] ( llvm::CallInst * call )
    {
        preprocess( get_function( call ) );
        if ( is_base_type_in_domain( fn->getParent(), call, dom ) )
            meta::set( call, meta::tag::abstract_return );

        if ( meta::aggregate::has( val ) )
            meta::aggregate::inherit( call, val );

        tasks.push_back( [=]{ propagate_value( call, dom ); } );
    };

    auto process_users = [&]( llvm::Value *val, auto recurse ) -> void
    {
        if ( auto go = llvm::dyn_cast< llvm::GlobalObject >( val ) )
        {
            for ( auto &a : go->getParent()->aliases() )
                if ( a.getBaseObject() == go )
                    recurse( &a, recurse );
        }
        for ( auto u : val->users() )
        {
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( u ) )
                process_call( call );
            if ( llvm::isa< llvm::ConstantExpr >( u ) )
                recurse( u, recurse );
        }
    };

    process_users( fn, process_users );
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
                    if ( auto c = lower_constant_expr_call( ce ) )
                        process( c, dom );
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
