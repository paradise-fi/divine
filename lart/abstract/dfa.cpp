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

namespace lart::abstract {

    using MapValue = DataFlowAnalysis::MapValuePtr;

    struct AddAbstractMetaVisitor : llvm::InstVisitor< AddAbstractMetaVisitor >
    {
        AddAbstractMetaVisitor( const MapValue &mval )
            : _mval( mval )
        {}

        static constexpr char op_prefix[] = "lart.abstract.op_";

        void add_meta( llvm::Instruction *val, const std::string& operation )
        {
            // TODO set correct domain kind?
            meta::abstract::set( val, to_string( DomainKind::scalar ) );

            auto m = val->getModule();
            if ( auto op = m->getFunction( operation ) ) {
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

            auto fn = call.getCalledFunction();
            ASSERT( fn->hasName() );
            auto op = std::string( op_prefix ) + fn->getName().str();
            add_meta( &call, op );
        }

        const MapValue &_mval;
    };

    bool is_call( llvm::Value * val ) noexcept
    {
        return util::is_one_of< llvm::CallInst, llvm::InvokeInst >( val );
    }

    auto calls( llvm::Function * fn ) noexcept
    {
        return query::query( fn->users() )
            .filter( [] ( auto val ) { return is_call( val ); } )
            .freeze();
    }

    bool is_propagable( llvm::Value * val ) noexcept
    {
        if ( llvm::isa< llvm::ConstantData >( val ) )
            return false;
        if ( is_call( val ) )
            return true;
        return util::is_one_of< llvm::Argument
                              , llvm::GlobalVariable
                              , llvm::ReturnInst
                              , llvm::GetElementPtrInst
                              , llvm::CmpInst
                              , llvm::UnaryInstruction
                              , llvm::BinaryOperator
                              , llvm::PHINode >( val );

    }

    bool DataFlowAnalysis::visited( llvm::Value * val ) const noexcept
    {
        return _intervals.has( val );
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

    bool DataFlowAnalysis::propagate( llvm::Value * to, const MapValuePtr& from ) noexcept
    {
        auto task = [=] { propagate( to ); };
        if ( !visited( to ) ) {
            _intervals[ to ] = from;
            push( task );
            return true; // change
        } else if ( interval( to )->join( from ) ) {
            // if join introduces any new information
            push( task );
            return true; // change
        }

        return false; // no change
    }

    bool DataFlowAnalysis::propagate_wrap( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        bool forward = [&] {
            return visited( rhs ) ? propagate( lhs, _intervals[ rhs ]->peel() ) : false;
        } ();

        bool backward = [&] {
            return visited( lhs ) ? propagate( rhs, _intervals[ lhs ]->cover() ) : false;
        } ();

        return forward || backward;

    }

    bool DataFlowAnalysis::propagate_identity( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        bool forward = [&] {
            return visited( rhs ) ? propagate( lhs, _intervals[ rhs ] ) : false;
        } ();

        bool backward = [&] {
            return visited( lhs ) ? propagate( rhs, _intervals[ lhs ] ) : false;
        } ();

        return forward || backward;
    }

    void DataFlowAnalysis::propagate_back( llvm::Argument * arg ) noexcept
    {
        ASSERT( visited( arg ) );
        if ( _intervals[ arg ]->is_covered() ) {
            auto fn = arg->getParent();

            ASSERT( !fn->hasAddressTaken() ); // TODO deal with indirect calls
            for ( auto call : calls( fn ) ) {
                auto op = call->getOperand( arg->getArgNo() );
                if ( propagate_identity( op, arg ) ) {
                    if ( auto opa = llvm::dyn_cast< llvm::Argument >( op ) )
                        propagate_back( opa );
                }
            }
        }
    }

    void DataFlowAnalysis::propagate( llvm::Value * val ) noexcept
    {
        llvm::Value * ptr = nullptr;
        bool change = false;

        llvmcase( val,
            [&] ( llvm::StoreInst * store ) {
                auto op = store->getValueOperand();
                ptr = store->getPointerOperand();
                if ( visited( op ) )
                    change = propagate( ptr, _intervals[ op ]->cover() );
                // TODO store to abstract value?
            },
            [&] ( llvm::LoadInst * load ) {
                ptr = load->getPointerOperand();
                change = propagate_wrap( load, ptr );
            },
            [&] ( llvm::CastInst * cast ) {
                ptr = cast->getOperand( 0 ); // TODO what if is not a pointer?
                change = propagate_identity( cast, ptr );
            },
            [&] ( llvm::GetElementPtrInst * gep ) {
                ptr = gep->getPointerOperand();
                change = propagate_wrap( gep, ptr );
            }
        );

        if ( ptr && change && llvm::isa< llvm::Argument >( ptr ) )
            propagate_back( llvm::cast< llvm::Argument >( ptr ) );

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
                    [&] ( llvm::GetElementPtrInst * gep ) {
                        push( [=] { propagate( gep ); } );
                    },
                    [&] ( llvm::CastInst * cast ) {
                        push( [=] { propagate( cast ); } );
                    },
                    [&] ( llvm::CallInst * call ) {
                        auto fn = llvm::CallSite( call ).getCalledFunction();
                        if ( !meta::function::ignore_call( fn ) )
                            push( [=] { propagate_in( call ); } );
                    },
                    [&] ( llvm::ReturnInst * ret ) {
                        push( [=] { propagate_out( ret ); } );
                    },
                    [&] ( llvm::Value * ) {
                        ASSERT( visited( val ) );
                        propagate( u, _intervals[ val ] );
                    }
                );
            }
        }
    }

    void DataFlowAnalysis::propagate_in( llvm::CallInst * call ) noexcept
    {
        auto fn = call->getCalledFunction();
        preprocess( fn );

        for ( const auto & arg : fn->args() ) {
            auto oparg = call->getArgOperand( arg.getArgNo() );
            if ( visited( oparg ) ) {
                auto a = const_cast< llvm::Argument * >( &arg );
                propagate( a, _intervals[ oparg ] );
            }
        }
    }

    void DataFlowAnalysis::propagate_out( llvm::ReturnInst * ret ) noexcept
    {
        auto fn = ret->getFunction();
        for ( auto call : calls( fn ) ) {
            preprocess( util::get_function( call ) );
            auto fn = llvm::CallSite( call ).getCalledFunction();
            if ( !meta::function::ignore_return( fn ) ) {
                auto val = ret->getReturnValue();
                propagate( call, _intervals[ val ] );
            }
        }
    }

    void DataFlowAnalysis::run( llvm::Module &m )
    {
        for ( auto * val : meta::enumerate( m ) ) {
            preprocess( util::get_function( val ) );
            interval( val )->to_top();
            push( [=] { propagate( val ); } );
        }

        while ( !_tasks.empty() ) {
            _tasks.front()();
            _tasks.pop_front();
        }

        for ( const auto & [ val, in ] : _intervals ) {
            // TODO decide what to annotate
            if ( in->is_core() )
                add_meta( val, in );
        }

        auto stores = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::StoreInst > )
            .filter( query::notnull )
            .freeze();

        for ( auto store : stores ) {
            auto val = store->getValueOperand();
            if ( _intervals.has( val ) ) {
                auto& mval =_intervals[ val ];
                if ( mval->is_core() )
                    add_meta( store, mval );
            }
        }
    }

    void DataFlowAnalysis::add_meta( llvm::Value * v, const MapValuePtr& mval ) noexcept
    {
        if ( llvm::isa< llvm::Constant >( v ) )
            return;

        AddAbstractMetaVisitor visitor( mval );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( v ) ) {
            visitor.visit( inst );
        }
        else if ( auto arg = llvm::dyn_cast< llvm::Argument >( v ) ) {
            // TODO set correct domain kind?
            if ( _intervals[ arg ]->is_core() ) {
                meta::argument::set( arg, to_string( DomainKind::scalar ) );
            }
        }

        for ( auto u : v->users() ) {
            if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( u ) ) {
                visitor.visit( ret );
            }
        }
    }

} // namespace lart::abstract
