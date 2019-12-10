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
                              , llvm::InsertValueInst
                              , llvm::ExtractValueInst
                              , llvm::PHINode >( val );
    }

    llvm::Instruction * lower_constant_expr( llvm::ConstantExpr * ce, llvm::Instruction * loc )
    {
        auto i = ce->getAsInstruction();
        i->insertBefore( loc );
        return i;
    }

    llvm::ConstantExpr * has_constexpr( llvm::Value * value, std::set< llvm::Value * > & seen )
    {
        if ( auto [it, inserted] = seen.insert( value ); !inserted )
            return nullptr;

        if ( llvm::isa< llvm::GlobalValue >( value ) )
            return nullptr;

        if ( auto con = llvm::dyn_cast< llvm::Constant >( value ) ) {
            if ( auto ce = llvm::dyn_cast< llvm::ConstantExpr >( con ) ) {
                return ce;
            } else {
                // recurse for ConstantStruct, ConstantArray ...
                for ( auto & op : con->operands() ) {
                    if ( auto ce = has_constexpr( op.get(), seen ) )
                        return ce;
                }
            }
        }

        return nullptr;
    }

    llvm::ConstantExpr * has_constexpr( llvm::Value * value )
    {
        std::set< llvm::Value * > seen;
        return has_constexpr( value, seen );
    }

    bool lower_constant_expr( llvm::Function & fn )
    {
        std::set< llvm::Instruction * > worklist;

        for ( auto & bb : fn ) {
            for ( auto & i : bb ) {
                for ( auto & op : i.operands() ) {
                    if ( has_constexpr( op ) ) {
                        worklist.insert( &i );
                        break;
                    }
                }
            }
        }

        bool change = !worklist.empty();

        while ( !worklist.empty() ) {
            auto inst = worklist.extract( worklist.begin() ).value();
            if ( auto phi = llvm::dyn_cast< llvm::PHINode >( inst ) ) {
                for ( unsigned int i = 0; i < phi->getNumIncomingValues(); ++i ) {
                    auto loc = phi->getIncomingBlock( i )->getTerminator();
                    assert( loc );

                    if ( auto ce = has_constexpr( phi->getIncomingValue( i ) ) ) {
                        auto new_inst = lower_constant_expr( ce, loc );
                        for ( unsigned int j = i; j < phi->getNumIncomingValues(); ++j ) {
                            if ( ( phi->getIncomingValue( j ) == phi->getIncomingValue( i ) ) &&
                                 ( phi->getIncomingBlock( j ) == phi->getIncomingBlock( i ) ) )
                            {
                                phi->setIncomingValue( j, new_inst );
                            }
                        }

                        worklist.insert( new_inst );
                    }
                }
            } else {
                for ( auto & op : inst->operands() ) {
                    if ( auto ce = has_constexpr( op.get() ) ) {
                        auto new_inst = lower_constant_expr( ce, inst );
                        inst->replaceUsesOfWith( ce, new_inst );
                        worklist.insert( new_inst );
                    }
                }
            }
        }

        return change;
    }

    void DataFlowAnalysis::preprocess( llvm::Function * fn ) noexcept
    {
        if ( !tagFunctionWithMetadata( *fn, "lart.abstract.preprocessed" ) )
            return;

        auto lse = std::unique_ptr< llvm::FunctionPass >( lart::createLowerSelectPass() );
        lse.get()->runOnFunction( *fn );

        auto lsi = std::unique_ptr< llvm::FunctionPass >( llvm::createLowerSwitchPass() );
        lsi.get()->runOnFunction( *fn );

        lower_constant_expr( *fn );
    }

    bool DataFlowAnalysis::propagate( llvm::Value * to, const type_onion &from ) noexcept
    {
        bool visited = has_type( to );

        auto change = [&] ( auto last, auto next ) {
            if ( !visited || last != next ) {
                _types.set( to, next );
                _blocked.clear();
                push( to );
                return true;
            }
            return false; // no change
        };

        auto last = visited ? type( to ) : from;
        auto next = visited ? join( last, from ) : from;
        return change( last, next );
    }

    bool DataFlowAnalysis::propagate_wrap( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        bool forward  = propagate( lhs, peel( rhs ) );
        bool backward = propagate( rhs, wrap( lhs ) );
        return forward || backward;
    }

    bool DataFlowAnalysis::propagate_identity( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        bool forward  = propagate( lhs, type( rhs ) );
        bool backward = propagate( rhs, type( lhs ) );
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

        if ( maybe_pointer( arg ) )
            each_call( arg->getParent(), handle_call );
    }

    void DataFlowAnalysis::propagate( llvm::Value * val ) noexcept
    {
        if ( ! llvm::isa< llvm::Constant >( val ) )
            preprocess( util::get_function( val ) );

        using Task = std::function< void( llvm::Value* ) >;
        Task propagate_back_task = [this, &propagate_back_task] ( llvm::Value * v ) {
            if ( auto arg = llvm::dyn_cast< llvm::Argument >( v ) )
                propagate_back( arg );

            if ( auto phi = llvm::dyn_cast< llvm::PHINode >( v ) )
                for ( auto & val : phi->incoming_values() )
                    if ( propagate_identity( phi, val.get() ) )
                        propagate_back_task( val.get() );
        };

        llvmcase( val,
            [&] ( llvm::AllocaInst * alloca )
            {
                auto size = alloca->getArraySize();
                if ( maybe_abstract( size ) )
                    UNREACHABLE( "unsupported allocation of abstract size" );
            },
            [&] ( llvm::StoreInst * store )
            {
                auto op = store->getValueOperand();
                auto ptr = store->getPointerOperand();
                if ( !maybe_aggregate( ptr ) && propagate( ptr, wrap( op ) ) )
                    propagate_back_task( ptr );
            },
            [&] ( llvm::LoadInst * load )
            {
                auto ptr = load->getPointerOperand();
                if ( maybe_aggregate( ptr ) )
                    propagate( val, peel( load ).make_abstract() );
                    // TODO enable store of aggregates to abstract aggregates
                else if ( propagate_wrap( load, ptr ) )
                    propagate_back_task( ptr );
            },
            [&] ( llvm::CastInst * cast )
            {
                auto ptr = cast->getOperand( 0 );
                if ( propagate_identity( cast, ptr ) )
                    propagate_back_task( ptr );
            },
            [&] ( llvm::GetElementPtrInst * gep )
            {
                auto ptr = gep->getPointerOperand();
                if ( maybe_aggregate( ptr ) )
                    propagate_identity( gep, ptr );
                else if ( propagate_identity( gep, ptr ) )
                    propagate_back_task( ptr );
            },
            [&] ( llvm::InsertValueInst * insert )
            {
                auto agg = insert->getAggregateOperand();
                auto ival = insert->getInsertedValueOperand();
                if ( maybe_abstract( ival ) && !maybe_abstract( agg ) )
                    _types.set( insert, type( agg ).make_abstract_aggregate() );
                if ( maybe_abstract( agg ) )
                    propagate_identity( insert, agg );
            },
            [&] ( llvm::ExtractValueInst * extract )
            {
                auto agg = extract->getAggregateOperand();
                if ( maybe_abstract( agg ) ) {
                    auto ty = extract->getType();
                    // FIXME maybe pointer from int
                    if ( ty->isIntegerTy() )
                        _types.set( extract, type( extract ).make_abstract() );
                    else if ( ty->isPointerTy() )
                        _types.set( extract, type( extract ).make_abstract_pointer() );
                    else
                        UNREACHABLE( "unsupported type" );
                }
            }
        );

        if ( is_propagable( val ) )
            for ( auto u : val->users() )
            {
                if ( util::is_one_of< llvm::LoadInst
                                    , llvm::StoreInst
                                    , llvm::GetElementPtrInst
                                    , llvm::CastInst
                                    , llvm::InsertValueInst
                                    , llvm::ExtractValueInst >( u ) )
                    push( u );
                else if ( util::is_one_of< llvm::CallInst, llvm::InvokeInst >( u ) )
                {
                    for ( auto &fn : resolve_call( llvm::CallSite( u ) ) ) {
                        if ( is_abstractable( fn ) ) {
                            // TODO transform function for domains
                            auto meta = meta::abstract::get( val );
                            if ( ( meta.has_value() ) )
                                propagate( u, type_from_meta( u, meta.value() ) );
                        } else {
                            if ( !meta::function::ignore_call( fn ) )
                                propagate_in( fn, llvm::CallSite( u ) );
                        }
                    }
                }
                else if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( u ) )
                    propagate_out( ret );
                else if ( util::is_one_of< llvm::SelectInst, llvm::SwitchInst >( u ) )
                    UNREACHABLE( "unsupported propagation" );
                else
                    propagate( u, type( val ) );
            }
    }

    void DataFlowAnalysis::propagate_in( llvm::Function *fn, llvm::CallSite call ) noexcept
    {
        for ( const auto & arg : fn->args() )
        {
            auto oparg = call.getArgOperand( arg.getArgNo() );

            if ( auto t = type( oparg ); t.maybe_abstract() )
                propagate( const_cast< llvm::Argument * >( &arg ), t );
        }
    }

    void DataFlowAnalysis::propagate_out( llvm::ReturnInst * ret ) noexcept
    {
        auto handle_call = [&]( auto call )
        {
            for ( auto fn : resolve_call( call ) )
                if ( !meta::function::ignore_return( fn ) )
                {
                    auto val = ret->getReturnValue();
                    propagate( call, type( val ) );
                }
        };
        each_call( ret->getFunction(), handle_call );
    }

    void DataFlowAnalysis::run( llvm::Module &m )
    {
        _types = type_map::get( m );
        for ( auto * val : meta::enumerate( m ) )
            push( val );

        while ( !_todo.empty() )
        {
            TRACE( "pop" );
            auto v = _todo.front();
            _queued.erase( v );
            _todo.pop_front();
            propagate( v );
        }

        for ( const auto & [ val, t ] : _types )
            if ( t.maybe_abstract() )
                add_meta( val );

        auto stores = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::StoreInst > )
            .filter( query::notnull )
            .freeze();

        for ( auto s : stores ) {
            if ( maybe_abstract( s->getValueOperand() ) )
                add_meta( s );
            if ( maybe_aggregate( s->getPointerOperand() ) )
                add_meta( s );
        }
    }

    void DataFlowAnalysis::add_meta( llvm::Value * v ) noexcept
    {
        if ( llvm::isa< llvm::Constant >( v ) )
            return;

        AddAbstractMetaVisitor visitor( _types );

        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( v ) )
            visitor.visit( inst );
        else if ( auto arg = llvm::dyn_cast< llvm::Argument >( v ) ) {
            auto abstract = maybe_abstract( arg );
            auto ptr = maybe_pointer( arg );
            auto agg = maybe_aggregate( arg );

            if ( abstract ) {
                auto kind = [&] {
                    if ( agg ) return DomainKind::aggregate;
                    if ( ptr ) return DomainKind::pointer;
                    return DomainKind::scalar;
                } ();

                meta::argument::set( arg, to_string( kind ) );
            }
        }

        for ( auto u : v->users() )
            if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( u ) )
                visitor.visit( ret );
    }

} // namespace lart::abstract
