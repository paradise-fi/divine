// -*- C++ -*- (c) 2016-2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/dfa.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Transforms/Utils.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/lowerselect.h>
#include <lart/support/util.h>
#include <lart/support/metadata.h>
#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>

#include <brick-llvm>

namespace lart::abstract
{
    struct AddAbstractMetaVisitor : llvm::InstVisitor< AddAbstractMetaVisitor >
    {
        type_onion _type;
        static constexpr char op_prefix[] = "lart.abstract.op_";

        AddAbstractMetaVisitor( const type_onion &t )
            : _type( t )
        {}

        void add_meta( llvm::Instruction *val, const std::string &operation )
        {
            meta::abstract::set( val, to_string( DomainKind::scalar ) ); /* FIXME */

            auto m = val->getModule();
            if ( auto op = m->getFunction( operation ) )
            {
                auto index = op->getMetadata( meta::tag::operation::index );
                val->setMetadata( meta::tag::operation::index, index );
            }
        }

        void visitStoreInst( llvm::StoreInst &store )
        {
            // TODO if is freeze
            // if ( store.getValueOperand()->getType()->isIntegerTy() ) // TODO remove
            meta::set( &store, meta::tag::operation::freeze );
            // TODO if is store
        }

        void visitLoadInst( llvm::LoadInst &load )
        {
            // TODO if is thaw
            // if ( load.getType()->isIntegerTy() ) // TODO remove
            meta::set( &load, meta::tag::operation::thaw );
            auto op = std::string( op_prefix ) + "thaw";
            add_meta( &load, op );
            // TODO if is load
        }

        void visitCmpInst( llvm::CmpInst &cmp )
        {
            auto op = std::string( op_prefix )
                    + PredicateTable.at( cmp.getPredicate() );
            add_meta( &cmp, op );
        }

        void visitBitCastInst( llvm::BitCastInst & ) { /* skip */ }
        void visitIntToPtrInst( llvm::IntToPtrInst & ) { /* skip */ }
        void visitPtrToIntInst( llvm::PtrToIntInst & ) { /* skip */ }

        void visitCastInst( llvm::CastInst &cast )
        {
            auto op = std::string( op_prefix )
                    + std::string( cast.getOpcodeName() );
            add_meta( &cast, op );
        }

        void visitBinaryOperator( llvm::BinaryOperator &bin )
        {
            auto op = std::string( op_prefix )
                    + std::string( bin.getOpcodeName() );
            add_meta( &bin, op );
        }

        void visitReturnInst( llvm::ReturnInst &ret )
        {
            meta::set( &ret, meta::tag::abstract_return );
        }

        void visitCallInst( llvm::CallInst &call )
        {
            if ( meta::abstract::has( &call ) )
                return;

            /* TODO what happens if there is more than one? */
            for ( auto fn : resolve_call( &call ) )
            {
                ASSERT( fn->hasName() );
                auto op = std::string( op_prefix ) + fn->getName().str();
                add_meta( &call, op );
            }
        }

        void visitPHINode( llvm::PHINode &phi )
        {
            meta::set( &phi, meta::tag::operation::phi );
        }
    };

    using ValueSet = std::set< llvm::Value * >;

    template< typename F >
    void each_call( llvm::Function *fn, F f, llvm::Value *val = nullptr,
                    std::shared_ptr< ValueSet > seen = std::make_shared< ValueSet >() ) noexcept
    {
        if ( !val )
            for ( auto u : fn->users() )
                each_call( fn, f, u, seen );

        if ( val && !seen->count( val ) )
        {
            seen->insert( val );

            if ( auto ce = llvm::dyn_cast< llvm::ConstantExpr >( val ) )
                for ( auto u : ce->users() )
                    each_call( fn, f, u, seen );

            if ( util::is_one_of< llvm::CallInst, llvm::InvokeInst >( val ) )
                for ( auto callee : resolve_call( llvm::CallSite( val ) ) )
                    if ( callee == fn )
                    {
                        if ( auto invoke = llvm::dyn_cast< llvm::InvokeInst >( val ) )
                            f( invoke );
                        if ( auto call = llvm::dyn_cast< llvm::CallInst >( val ) )
                            f( call );
                    }
        }
    }

    bool is_propagable( llvm::Value *val ) noexcept
    {
        if ( llvm::isa< llvm::ConstantData >( val ) )
            return false;

        return util::is_one_of< llvm::Argument
                              , llvm::CallInst
                              , llvm::InvokeInst
                              , llvm::GlobalVariable
                              , llvm::ReturnInst
                              , llvm::GetElementPtrInst
                              , llvm::CmpInst
                              , llvm::UnaryInstruction
                              , llvm::BinaryOperator
                              , llvm::PHINode >( val );
    }

    void DataFlowAnalysis::preprocess( llvm::Function * fn ) noexcept
    {
        if ( !tagFunctionWithMetadata( *fn, "lart.abstract.preprocessed" ) )
            return;

        auto lse = std::unique_ptr< llvm::FunctionPass >( lart::createLowerSelectPass() );
        lse.get()->runOnFunction( *fn );

        auto lsi = std::unique_ptr< llvm::FunctionPass >( llvm::createLowerSwitchPass() );
        lsi.get()->runOnFunction( *fn );
    }

    bool DataFlowAnalysis::propagate( llvm::Value * to, const type_onion &from ) noexcept
    {
        auto old = _types.get( to );

        if ( old != from )
        {
            _types.set( to, join( old, from ) );
            TRACE( "pushing dirty value", *to, "previously", old, "now", _types.get( to ) );
            push( [=] { propagate( to ); } );
            return true;
        }

        return false; // no change
    }

    bool DataFlowAnalysis::propagate_wrap( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        bool forward  = propagate( lhs, _types.get( rhs ).peel() );
        bool backward = propagate( rhs, _types.get( lhs ).wrap() );
        return forward || backward;
    }

    bool DataFlowAnalysis::propagate_identity( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        bool forward  = propagate( lhs, _types.get( rhs ) );
        bool backward = propagate( rhs, _types.get( lhs ) );
        return forward || backward;
    }

    void DataFlowAnalysis::propagate_back( llvm::Argument *arg ) noexcept
    {
        auto handle_call = [&]( auto call )
        {
            auto op = call->getOperand( arg->getArgNo() );
            if ( propagate_identity( op, arg ) )
            {
                if ( auto opa = llvm::dyn_cast< llvm::Argument >( op ) )
                    propagate_back( opa );
            }
        };

        if ( _types.get( arg ).maybe_pointer() )
            each_call( arg->getParent(), handle_call );
    }

    void DataFlowAnalysis::propagate( llvm::Value * val ) noexcept
    {
        auto propagate_back_task = [&] ( llvm::Value * v )
        {
            if ( auto arg = llvm::dyn_cast< llvm::Argument >( v ) )
                push( [=] { propagate_back( arg ); } );
        };

        llvmcase( val,
            [&] ( llvm::StoreInst * store )
            {
                auto op = store->getValueOperand();
                auto ptr = store->getPointerOperand();
                if ( propagate( ptr, _types.get( op ).wrap() ) )
                    propagate_back_task( ptr );
                // TODO store to abstract value?
            },
            [&] ( llvm::LoadInst * load )
            {
                auto ptr = load->getPointerOperand();
                if ( propagate_wrap( load, ptr ) )
                    propagate_back_task( ptr );
            },
            [&] ( llvm::CastInst * cast )
            {
                auto ptr = cast->getOperand( 0 ); // TODO what if is not a pointer? (core)
                if ( propagate_identity( cast, ptr ) )
                    propagate_back_task( ptr );
            },
            [&] ( llvm::PHINode * phi )
            {
                for ( auto & val : phi->incoming_values() )
                    if ( propagate_identity( phi, val.get() ) )
                        if ( auto arg = llvm::dyn_cast< llvm::Argument >( val.get() ) )
                            push( [=] { propagate_back( arg ); } );
            },
            [&] ( llvm::GetElementPtrInst * gep )
            {
                auto ptr = gep->getPointerOperand();
                if ( propagate_wrap( gep, ptr ) )
                    propagate_back_task( ptr );
            }
        );

        // ssa operations
        if ( is_propagable( val ) ) {
            for ( auto u : val->users() ) {
                llvmcase( u,
                    [&] ( llvm::LoadInst * load ) {
                        push( [=] { propagate( load ); } );
                    },
                    [&] ( llvm::StoreInst * store ) {
                        push( [=] { propagate( store ); } );
                    },
                    [&] ( llvm::GetElementPtrInst * gep )
                    {
                        push( [=] { propagate( gep ); } );
                    },
                    [&] ( llvm::CastInst * cast )
                    {
                        push( [=] { propagate( cast ); } );
                    },
                    [&] ( llvm::PHINode * phi )
                    {
                        push( [=] { propagate( phi ); } );
                    },
                    [&] ( llvm::CallInst * call )
                    {
                        for ( auto &fn : resolve_call( call ) )
                            if ( !meta::function::ignore_call( fn ) )
                                push( [=] { propagate_in( fn, call ); } );
                    },
                    [&] ( llvm::ReturnInst * ret )
                    {
                        push( [=] { propagate_out( ret ); } );
                    },
                    [&] ( llvm::SelectInst * )
                    {
                        UNREACHABLE( "unsupported propagation" );
                    },
                    [&] ( llvm::SwitchInst * )
                    {
                        UNREACHABLE( "unsupported propagation" );
                    },
                    [&] ( llvm::Value * )
                    {
                        propagate( u, _types.get( val ) );
                    }
                );
            }
        }
    }

    void DataFlowAnalysis::propagate_in( llvm::Function *fn, llvm::CallInst * call ) noexcept
    {
        preprocess( fn );

        for ( const auto & arg : fn->args() )
        {
            auto oparg = call->getArgOperand( arg.getArgNo() );

            if ( auto t = _types.get( oparg ); t.maybe_abstract() )
                propagate( const_cast< llvm::Argument * >( &arg ), t );
        }
    }

    void DataFlowAnalysis::propagate_out( llvm::ReturnInst * ret ) noexcept
    {
        auto handle_call = [&]( auto call )
        {
            preprocess( util::get_function( call ) );
            for ( auto fn : resolve_call( call ) )
                if ( !meta::function::ignore_return( fn ) )
                {
                    auto val = ret->getReturnValue();
                    propagate( call, _types.get( val ) );
                }
        };
        each_call( ret->getFunction(), handle_call );
    }

    void DataFlowAnalysis::run( llvm::Module &m )
    {
        for ( auto * val : meta::enumerate( m ) )
        {
            preprocess( util::get_function( val ) );
            _types.set( val, _types.get( val ).make_abstract() );
            push( [=] { propagate( val ); } );
        }

        while ( !_tasks.empty() )
        {
            _tasks.front()();
            _tasks.pop_front();
        }

        for ( const auto & [ val, t ] : _types )
            if ( t.maybe_abstract() )
                add_meta( val, t );

        auto stores = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::StoreInst > )
            .filter( query::notnull )
            .freeze();

        for ( auto store : stores ) /* TODO redundant? */
        {
            auto val = store->getValueOperand();
            if ( auto t = _types.get( val ); t.maybe_abstract() )
                add_meta( store, t );
        }
    }

    void DataFlowAnalysis::add_meta( llvm::Value * v, const type_onion &t ) noexcept
    {
        if ( llvm::isa< llvm::Constant >( v ) )
            return;

        AddAbstractMetaVisitor visitor( t );

        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( v ) )
            visitor.visit( inst );
        else if ( auto arg = llvm::dyn_cast< llvm::Argument >( v ) )
        {
            if ( _types.get( arg ).maybe_abstract() )
                meta::argument::set( arg, to_string( DomainKind::scalar ) ); /* FIXME */
        }

        for ( auto u : v->users() )
            if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( u ) )
                visitor.visit( ret );
    }

} // namespace lart::abstract
